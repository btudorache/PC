#include <stdio.h>
#include <queue.h>
#include "skel.h"

#define MAX_LINE_LENGTH 256
#define MAX_IP_LENGTH 16

struct arp_entry {
    uint32_t ip;
    uint8_t mac[6];
};

struct route_table_entry {
    uint32_t prefix;
    uint32_t next_hop;
    uint32_t mask;
    int interface;
} __attribute__((packed));

// custom inet_aton()
uint32_t ip_address_to_int(char* string_ip_address) {
    unsigned int first_part, second_part, third_part, fourth_part;
    sscanf(string_ip_address, "%u.%u.%u.%u", &first_part, &second_part, &third_part, &fourth_part);
    
    return (first_part << 24) + (second_part << 16) + (third_part << 8) + fourth_part;
}

// comparator for qsort
int route_entry_comparator(const void* first, const void* second) {
    struct route_table_entry* first_entry = (struct route_table_entry*)first;
    struct route_table_entry* second_entry = (struct route_table_entry*)second;

    uint32_t first_prefix = first_entry->prefix;
    uint32_t first_mask = first_entry->mask;

    uint32_t second_prefix = second_entry->prefix;
    uint32_t second_mask = second_entry->mask;

    if (first_prefix != second_prefix) {
        return first_prefix - second_prefix;
    } else {
        return first_mask - second_mask;
    }
}

struct route_table_entry* parse_route_table(char* rtable_name, int* rtable_size) {
    // open file
    FILE* rtable_file = fopen(rtable_name, "r");
    DIE(rtable_file == NULL, "Couldn't open file");

    // count num lines
    char line[MAX_LINE_LENGTH];
    int num_entries = 0;
    while (fgets(line, MAX_LINE_LENGTH, rtable_file)) {
        num_entries++;
    }
    fseek(rtable_file, 0, SEEK_SET);

    // allocate table
    struct route_table_entry* rtable = malloc(sizeof(struct route_table_entry) * (num_entries + 1));
    DIE(rtable == NULL, "Couldn't allocate memory");

    // parse table
    char prefix[MAX_IP_LENGTH], next_hop[MAX_IP_LENGTH], mask[MAX_IP_LENGTH];
    int interface;
    for (int i = 0; fgets(line, MAX_LINE_LENGTH, rtable_file); i++) {
        sscanf(line, "%s %s %s %d", prefix, next_hop, mask, &interface);
        rtable[i].prefix = ip_address_to_int(prefix);
        rtable[i].next_hop = ip_address_to_int(next_hop);
        rtable[i].mask = ip_address_to_int(mask);
        rtable[i].interface = interface;
    }

    // sort route_table in O(nlogn)
    qsort(rtable, num_entries, sizeof(struct route_table_entry), route_entry_comparator);

    // set number of entries
    *rtable_size = num_entries;

    fclose(rtable_file);

    return rtable;
}

// route_table_entry search in O(logn)
struct route_table_entry *get_best_route(uint32_t dest_ip, struct route_table_entry* rtable, int rtable_size) {
    int best_entry = -1;

    int left = 0;
    int right = rtable_size;
    while (left <= right) {
        int mid = (left + right) / 2;

        if (rtable[mid].prefix <= (dest_ip & rtable[mid].mask)) {
            best_entry = mid;
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    if (best_entry == -1) {
        return NULL;
    }

    return &rtable[best_entry];
}


struct arp_entry *get_arp_entry(uint32_t ip, struct arp_entry* arp_table, int arp_table_size) {
    for (int i = 0; i < arp_table_size; i++) {
        if (ip == arp_table[i].ip) {
            return &arp_table[i];
        }
    }
    return NULL;
}


int main(int argc, char *argv[])
{
    packet m;
    int rc;

    init(argc - 2, argv + 2);

    // parse route_table
    int rtable_size = 0;
    struct route_table_entry* rtable = parse_route_table(argv[1], &rtable_size);

    // allocate arp_entries array
    int discovered_arp_entries = 0;
    struct arp_entry* arp_table = malloc(10 * sizeof(struct arp_entry));
    DIE(arp_table == NULL, "Couldn't allocate memory");

    // allocate queues for every interface
    struct queue** queue_list = malloc(ROUTER_NUM_INTERFACES * sizeof(struct queue*));
    DIE(queue_list == NULL, "Couldn't allocate memory");
    for (int i = 0; i < ROUTER_NUM_INTERFACES; i++) {
        queue_list[i] = queue_create();
        DIE(queue_list[i] == NULL, "Couldn't allocate memory");
    }

    while (1) {
        rc = get_packet(&m);
        DIE(rc < 0, "get_message");

        struct ether_header *eth_hdr = (struct ether_header *)m.payload;

        // if package is icmp/ip
        if (ntohs(eth_hdr->ether_type) == ETHERTYPE_IP) {
            struct iphdr *ip_hdr = (struct iphdr *)(m.payload + sizeof(struct ether_header));
            struct icmphdr* icmp_hdr = parse_icmp(m.payload);

            uint8_t source_mac[ETH_ALEN];
            get_interface_mac(m.interface, source_mac);
            
            // if package is destined to router
            if (icmp_hdr != NULL 
                && ip_hdr->daddr == htonl(ip_address_to_int(get_interface_ip(m.interface)))
                && icmp_hdr->type == ICMP_ECHO) {
                send_icmp(ip_hdr->saddr,
                          htonl(ip_address_to_int(get_interface_ip(m.interface))),
                          source_mac,
                          eth_hdr->ether_shost,
                          ICMP_ECHOREPLY,
                          0,
                          m.interface,
                          icmp_hdr->un.echo.id,
                          icmp_hdr->un.echo.sequence);
                printf("Sent ECHO Reply\n");

            // else if package is not directly addresed to router (the usual case)
            } else {
                // check checksum
                if (ip_checksum(ip_hdr, sizeof(struct iphdr))) {
                    printf("Bad checksum\n");
                    continue;
                }

                // check ttl
                if (ip_hdr->ttl <= 1) {
                    send_icmp(ip_hdr->saddr,
                              htonl(ip_address_to_int(get_interface_ip(m.interface))),
                              source_mac,
                              eth_hdr->ether_shost,
                              ICMP_TIME_EXCEEDED,
                              0,
                              m.interface,
                              getpid(),
                              1);
                    printf("Discard package\n");
                    continue;
                }

                // find next hop
                struct route_table_entry* best_route = get_best_route(ntohl(ip_hdr->daddr), rtable, rtable_size);
                if (best_route == NULL) {
                    send_icmp_error(ip_hdr->saddr,
                                    htonl(ip_address_to_int(get_interface_ip(m.interface))),
                                    source_mac,
                                    eth_hdr->ether_shost,
                                    ICMP_DEST_UNREACH,
                                    0,
                                    m.interface);
                    printf("Couldn't find next hop\n");
                    continue;
                }

                // update ip header
                ip_hdr->ttl -= 1;
                ip_hdr->check = 0;
                ip_hdr->check = ip_checksum(ip_hdr, sizeof(struct iphdr));

                // find next hop mac
                struct arp_entry* curr_arp_entry = get_arp_entry(best_route->next_hop, arp_table, discovered_arp_entries);

                // if no mac is found for next hop, send an ARP REQUEST and enqueue current package
                if (curr_arp_entry == NULL) {
                    struct ether_header new_eth_hdr;
                    get_interface_mac(best_route->interface, new_eth_hdr.ether_shost);
                    memset(new_eth_hdr.ether_dhost, 0xff, sizeof(new_eth_hdr.ether_dhost));
                    new_eth_hdr.ether_type = htons(ETHERTYPE_ARP);

                    send_arp(htonl(best_route->next_hop),
                             htonl(ip_address_to_int(get_interface_ip(best_route->interface))),
                             &new_eth_hdr,
                             best_route->interface,
                             htons(ARPOP_REQUEST));
                    printf("Sending ARP request\n");

                    // save current package in a queue
                    packet* package_copy = malloc(sizeof(packet));
                    DIE(package_copy == NULL, "Couldn't allocate memory");
                    memcpy(package_copy, &m, sizeof(packet));

                    queue_enq(queue_list[best_route->interface], package_copy);

                // else send packages as usual
                } else {
                    get_interface_mac(best_route->interface, eth_hdr->ether_shost);
                    memcpy(eth_hdr->ether_dhost, curr_arp_entry->mac, sizeof(curr_arp_entry->mac));

                    send_packet(best_route->interface, &m);
                    printf("Forwarding package\n");
                }
            }

        // if package is arp    
        } else if (ntohs(eth_hdr->ether_type) == ETHERTYPE_ARP) {
            struct arp_header* arp_hdr = parse_arp(m.payload);

            // if ARP REQUEST, send a reply with mac address
            if (ntohs(arp_hdr->op) == ARPOP_REQUEST) {
                struct ether_header new_eth_hdr;
                get_interface_mac(m.interface, new_eth_hdr.ether_shost);
                memcpy(new_eth_hdr.ether_dhost, arp_hdr->sha, sizeof(arp_hdr->sha));
                new_eth_hdr.ether_type = htons(ETHERTYPE_ARP);

                send_arp(arp_hdr->spa,
                         htonl(ip_address_to_int(get_interface_ip(m.interface))),
                         &new_eth_hdr,
                         m.interface,
                         htons(ARPOP_REPLY));
                printf("Sent ARP Reply\n");

            // if ARP REPLY, save new arp entry and send all enqueued packages
            } else if (ntohs(arp_hdr->op) == ARPOP_REPLY) {
                struct arp_entry new_arp_entry;
                new_arp_entry.ip = ntohl(arp_hdr->spa);
                memcpy(new_arp_entry.mac, arp_hdr->sha, sizeof(arp_hdr->sha));

                arp_table[discovered_arp_entries] = new_arp_entry;
                discovered_arp_entries++;
                printf("Saved reply entry\n");

                // send enqueued packages to destination
                while (!queue_empty(queue_list[m.interface])) {
                    packet* queue_package = (packet*)queue_deq(queue_list[m.interface]);
                    struct ether_header *enqued_eth_hdr = (struct ether_header *)queue_package->payload;

                    get_interface_mac(m.interface, enqued_eth_hdr->ether_shost);
                    memcpy(enqued_eth_hdr->ether_dhost, arp_hdr->sha, sizeof(arp_hdr->sha));

                    send_packet(m.interface, queue_package);
                    free(queue_package);
                }
                printf("Sent packets in queue\n");
            }
        }
    }
    free(rtable);
    free(arp_table);
    free(queue_list);
}
