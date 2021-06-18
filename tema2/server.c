#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "utils.h"

// creates a new topic structure for the subscriber if it doesn't have it
// and subscribes to it
void subscribe_to_topic(struct subscriber_entry* entry, char* new_topic, int sf) {
	list tmp = entry->topics;
	struct topic_entry* tmp_topic;

	while (tmp != NULL) {
		tmp_topic = (struct topic_entry *)tmp->element;
		if (strcmp(tmp_topic->topic, new_topic) == 0) {
			tmp_topic->sf = sf;
			tmp_topic->subscribed = 1;
			return;
		}
		tmp = tmp->next;
	}

	// builds new topic struct
	struct topic_entry* new_topic_entry = malloc(sizeof(struct topic_entry));

	strcpy(new_topic_entry->topic, new_topic);
	new_topic_entry->sf = sf;
	new_topic_entry->subscribed = 1;

	// adds new topic struct to the struct list
	entry->topics = cons(new_topic_entry, entry->topics);
}

// unsubscribes a subscriber from an entry
void unsubscribe_from_topic(struct subscriber_entry* entry, char* new_topic) {
	list tmp = entry->topics;
	struct topic_entry* tmp_topic;

	while (tmp != NULL) {
		tmp_topic = (struct topic_entry *)tmp->element;
		if (strcmp(tmp_topic->topic, new_topic) == 0) {
			tmp_topic->sf = 0;
			tmp_topic->subscribed = 0;
			return;
		}
		tmp = tmp->next;
	}
}

// tells if a subscriber has a specific topic;
// the has_sf flag verifies if the topic is saved as store and forward
int has_topic(struct subscriber_entry* entry, char* new_topic, int has_sf) {
	list tmp = entry->topics;
	struct topic_entry* tmp_topic;

	while(tmp != NULL) {
		tmp_topic = (struct topic_entry *)tmp->element;
		if (strcmp(tmp_topic->topic, new_topic) == 0) {
			if (has_sf) {
				return tmp_topic->subscribed == 1 && tmp_topic->sf == 1;
			} else {
				return tmp_topic->subscribed == 1;
			}
		}
		tmp = tmp->next;
	}

	return 0;
}

// gets the subscriber with a specific id from the subscriber list
struct subscriber_entry* get_subscriber(list subscriber_list, char* new_id) {
	list tmp = subscriber_list;

	while (tmp != NULL) {
		struct subscriber_entry* subscriber = (struct subscriber_entry*)tmp->element;
		if (strcmp(subscriber->id, new_id) == 0) {
			return subscriber;
		}
		tmp = tmp->next;
	}

	return NULL;
}

// adds a new subscriber to the subscriber list;
// returns 0 if the subscriber is not connected and 1 if he is already connected
// if the subscriber reconnects, send all the messages stored from the sf functionality
int add_new_subscriber(list* subscriber_list,  
					   char* new_id, 
					   struct tcp_map_entry* tcp_list, 
					   int sockfd) {
	list tmp = (*subscriber_list);

	while (tmp != NULL) {
		struct subscriber_entry* subscriber = (struct subscriber_entry*)tmp->element;
		if (strcmp(subscriber->id, new_id) == 0) {
			if (subscriber->connected) {
				return 1;
			} else {
				subscriber->connected = 1;
				memcpy(&subscriber->addr, &tcp_list[sockfd].addr, sizeof(struct sockaddr_in));
				while (!queue_empty(subscriber->store)) {
					struct server_message* stored_message = queue_deq(subscriber->store);
					send(sockfd, stored_message, sizeof(struct server_message), 0);
					free(stored_message);
				}
				return 0;
			}
		}
		tmp = tmp->next;
	}

	// builds new subscriber struct
	struct subscriber_entry* new_subscriber = malloc(sizeof(struct subscriber_entry));

	strcpy(new_subscriber->id, new_id);
	new_subscriber->connected = 1;
	memcpy(&new_subscriber->addr, &tcp_list[sockfd].addr, sizeof(struct sockaddr_in));
	new_subscriber->store = queue_create();

	// adds struct to the subscriber list
	(*subscriber_list) = cons(new_subscriber, (*subscriber_list));

	return 0;
}

// free the subscriber list
void free_subscribers(list* subscriber_list) {
	list tmp_sub_list = (*subscriber_list);
	while (tmp_sub_list != NULL) {
		struct subscriber_entry* subscriber = (struct subscriber_entry*)tmp_sub_list->element;
		list tmp_topic_list = (subscriber->topics);
		while (tmp_topic_list != NULL) {
			struct topic_entry* tmp_topic = (struct topic_entry *)tmp_topic_list->element;
			free(tmp_topic);
			tmp_topic_list = cdr_and_free(tmp_topic_list);
		}
		free(subscriber->store);
		free(subscriber);
		tmp_sub_list = cdr_and_free(tmp_sub_list);
	}
}

void usage(char *arg) {
	fprintf(stderr, "Usage: %s server_port\n", arg);
	exit(0);
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		usage(argv[0]);
	}

	// disable stdout buffering
	setvbuf(stdout, NULL, _IONBF, 0);

	int udp_sockfd, tcp_sockfd, new_subscriber_fd;
	char buffer[BUFLEN];
	struct udp_message udp_message;
	struct server_message server_message;
	struct sockaddr_in serv_addr, subs_addr, udp_message_addr;
	int n, i, ret;
	socklen_t subs_addr_len, udp_message_addr_len;

	list subscriber_list = NULL;

	// maps local socket_fd to tcp_map_entry struct, containing id and tcp client's address
	struct tcp_map_entry* fd_map = calloc(MAX_CLIENTS, sizeof(struct tcp_map_entry));
	DIE(fd_map == NULL, "couldn't allocate memory");

	udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(udp_sockfd < 0, "udp socket");

	tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(tcp_sockfd < 0, "tcp socket");

	// disable nagle's algorithm after initializing tcp_sockfd
	int yes = 1;
	int result = setsockopt(tcp_sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&yes, sizeof(int));
	DIE(result < 0, "couldn't disable nagle");

	int portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(udp_sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind udp");

	ret = bind(tcp_sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind tcp");

	ret = listen(tcp_sockfd, MAX_CLIENTS);
	DIE(ret < 0, "listen");

	fd_set read_fds;
	fd_set tmp_fds;	
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	FD_SET(tcp_sockfd, &read_fds);
	FD_SET(udp_sockfd, &read_fds);
	FD_SET(0, &read_fds);
	int fdmax = (tcp_sockfd > udp_sockfd) ? tcp_sockfd : udp_sockfd;

	while (1) {
		tmp_fds = read_fds; 
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				// check for a new tcp connection
				if (i == tcp_sockfd) {
					subs_addr_len = sizeof(subs_addr);
					new_subscriber_fd = accept(tcp_sockfd, (struct sockaddr *) &subs_addr, &subs_addr_len);
					DIE(new_subscriber_fd < 0, "accept");

					// disable nagle's algorithm
					int result = setsockopt(tcp_sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&yes, sizeof(int));
					DIE(result < 0, "couldn't disable nagle");

					// add new connection to read_fds
					FD_SET(new_subscriber_fd, &read_fds);
					if (new_subscriber_fd > fdmax) { 
						fdmax = new_subscriber_fd;
					}

					// save tcp_addr in fd_map
					memcpy(&fd_map[new_subscriber_fd].addr, &subs_addr, sizeof(struct sockaddr_in));

				// check for new message from tcp connection
				} else if (i == udp_sockfd) {
					memset(&udp_message, 0, sizeof(struct udp_message));
					udp_message_addr_len = sizeof(udp_message_addr);
					int bytes_read = recvfrom(i, &udp_message, sizeof(struct udp_message), 0, (struct sockaddr*)&udp_message_addr, &udp_message_addr_len);

					// build a server_message struct
					memset(&server_message, 0, sizeof(struct server_message));
					memcpy(&server_message.addr, &udp_message_addr, sizeof(struct sockaddr_in));
					memcpy(&server_message.message, &udp_message, bytes_read);

					// for every open connection, try to send the message
					for (int k = 0; k <= fdmax; k++) {
						if (FD_ISSET(k, &read_fds) && k != tcp_sockfd && k != udp_sockfd && k != 0) {
							struct subscriber_entry* subscriber = get_subscriber(subscriber_list, fd_map[k].id);
							if (subscriber->connected && has_topic(subscriber, udp_message.topic, 0)) {
								send(k, &server_message, sizeof(struct server_message), 0);
							}
						}
					}	

					// if there are disconnected users subscribed to the topic, save the message in a queue
					list tmp = subscriber_list;
					while (tmp != NULL) {
						struct subscriber_entry* subscriber = (struct subscriber_entry*)tmp->element;
						if (!subscriber->connected && has_topic(subscriber, udp_message.topic, 1)) {
							struct server_message* stored_message = malloc(sizeof(struct server_message));
							memcpy(stored_message, &server_message, sizeof(struct server_message));
							queue_enq(subscriber->store, stored_message);
						}
						tmp = tmp->next;
					}

				// keyboard inputs
				} else if (i == 0) {
					memset(buffer, 0, BUFLEN);
					fgets(buffer, BUFLEN - 1, stdin);

					// for "exit", close all clients and send closing message to them
					if (strncmp(buffer, "exit", strlen("exit")) == 0) {
						for (int k = 0; k <= fdmax; k++) {
							if (FD_ISSET(k, &read_fds) && k != tcp_sockfd && k != udp_sockfd && k != 0) {
								memset(buffer, 0, BUFLEN);
								strcpy(buffer, "exit");
								send(k, buffer, BUFLEN, 0);
								close(k);
							}
						}
						goto close_server;
					}
				
				// incoming message from tcp clients
				} else {
					memset(buffer, 0, BUFLEN);
					n = recv(i, buffer, sizeof(buffer), 0);
					DIE(n < 0, "recv");

					// close the connection if nothing received
					if (n == 0) {
						close(i);
						FD_CLR(i, &read_fds);
					} else {
						// for a "connect %s" message, connect the new subscriber (if possible) 
						// and add him to the subscriber list
						if (strncmp(buffer, "connect", strlen("connect")) == 0) {
							char new_id[ID_LEN];
							sscanf(buffer, "connect %s", new_id);
							strcpy(fd_map[i].id, new_id);

							int res = add_new_subscriber(&subscriber_list, new_id, fd_map, i);

							if (res == 0) {
								printf("New client %s connected from %s:%d.\n", 
									   new_id, 
									   inet_ntoa(fd_map[i].addr.sin_addr),
									   ntohs(fd_map[i].addr.sin_port));
							} else {
								printf("Client %s already connected.\n", new_id);
								memset(buffer, 0, BUFLEN);
								strcpy(buffer, "exit");
								send(i, buffer, BUFLEN, 0);
							}

						// for a "subscribe %s" message, subscribe the user to the topic
						} else if (strncmp(buffer, "subscribe", strlen("subscribe")) == 0) {
							char new_topic[50];
							int sf;
							sscanf(buffer, "subscribe %s %d", new_topic, &sf);

							struct subscriber_entry* subscriber = get_subscriber(subscriber_list, fd_map[i].id);
							subscribe_to_topic(subscriber, new_topic, sf);

						// for a "unsubscribe %s" message, unsubscribe the user from the topic
						} else if (strncmp(buffer, "unsubscribe", strlen("unsubscribe")) == 0) {
							char new_topic[50];
							sscanf(buffer, "unsubscribe %s", new_topic);

							struct subscriber_entry* subscriber = get_subscriber(subscriber_list, fd_map[i].id);
							unsubscribe_from_topic(subscriber, new_topic);

						// for a "exit" message, disconnect the user and close the incoming connection
						} else if (strncmp(buffer, "exit", strlen("exit")) == 0) {
							struct subscriber_entry* subscriber = get_subscriber(subscriber_list, fd_map[i].id);
							subscriber->connected = 0;

							printf("Client %s disconnected.\n", fd_map[i].id);
							close(i);
							FD_CLR(i, &read_fds);
						}
					}
				}
			}
		}
	}

	close_server:
	free(fd_map);
	free_subscribers(&subscriber_list);

	close(udp_sockfd);
	close(tcp_sockfd);

	return 0;
}
