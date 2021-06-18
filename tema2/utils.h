#ifndef _UTILS_H
#define _UTILS_H 1

#include <stdio.h>
#include <stdlib.h>
#include "list.h"
#include "queue.h"

/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define BUFLEN 256
#define ID_LEN 10
#define MAX_CLIENTS	20000
#define INT 0
#define SHORT_REAL 1
#define FLOAT 2
#define STRING 3

struct udp_message {
	char topic[50];
	uint8_t type;
	char data[1500];
} __attribute__((packed));

struct server_message {
	struct sockaddr_in addr;
	struct udp_message message;
} __attribute__((packed));

struct tcp_map_entry {
	struct sockaddr_in addr;
	char id[ID_LEN];
} __attribute__((packed));

struct subscriber_entry {
	int connected;
	list topics;
	struct queue *store;
	char id[ID_LEN];
	struct sockaddr_in addr;
} __attribute__((packed));

struct topic_entry {
	char topic[50];
	int sf;
	int subscribed;
} __attribute__((packed));

#endif
