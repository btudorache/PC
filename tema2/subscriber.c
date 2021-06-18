#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <math.h>
#include "utils.h"

void usage(char *file) {
	fprintf(stderr, "Usage: %s client_id server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[]) {
	if (argc < 4) {
		usage(argv[0]);
	}

	// disable stdout buffering
	setvbuf(stdout, NULL, _IONBF, 0);

	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];
	struct server_message message;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	// disable nagle's algorithm
	int yes = 1;
	int result = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&yes, sizeof(int));
	DIE(result < 0, "couldn't disable nagle");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	// sending connect message to client
	memset(buffer, 0, BUFLEN);
	sprintf(buffer, "connect %s", argv[1]);
	n = send(sockfd, buffer, strlen(buffer), 0);
	DIE(n < 0, "send");

	fd_set read_fds;
	fd_set tmp_fds;
	int fdmax;

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	FD_SET(sockfd, &read_fds);
	fdmax = sockfd;

	FD_SET(0, &read_fds);

	while (1) {
		tmp_fds = read_fds; 
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		// keyboard input
		if (FD_ISSET(0, &tmp_fds)) {
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN, stdin);
			buffer[strlen(buffer) - 1] = '\0';

			// send the input to the server and print log messages if necessary
			n = send(sockfd, buffer, strlen(buffer), 0);
			DIE(n < 0, "send");

			if (strncmp(buffer, "exit", strlen("exit")) == 0) {
				break;
			} else if (strncmp(buffer, "subscribe", strlen("subscribe")) == 0) {
				printf("Subscribed to topic.\n");
			} else if (strncmp(buffer, "unsubscribe", strlen("unsubscribe")) == 0) {
				printf("Unsubscribed from topic.\n");
			}

		// incoming message from server
		} else if (FD_ISSET(sockfd, &tmp_fds)) {
			memset(&message, 0, sizeof(struct server_message));
			n = recv(sockfd, &message, sizeof(struct server_message), 0);

			// end program if getting an "exit" message from the server
			if (strncmp((char*)&message, "exit", strlen("exit")) == 0) {
				break;
			} else {
				// case for INT type topic
				if (message.message.type == INT) {
					uint8_t sign = message.message.data[0];
					uint32_t value;
					memcpy((char*)&value, message.message.data + 1, 4);

					value = (sign == 1) ? (-ntohl(value)) : (ntohl(value));

					printf("%s:%d - %s - INT - %d\n",
						   inet_ntoa(message.addr.sin_addr),
						   ntohs(message.addr.sin_port),
						   message.message.topic,
						   value);

				// case for SHORT_REAL type topic
				} else if (message.message.type == SHORT_REAL) {
					int value = 0;
					memcpy(((char*)&value), message.message.data, 2);

					printf("%s:%d - %s - SHORT_REAL - %.2f\n",
						   inet_ntoa(message.addr.sin_addr),
						   ntohs(message.addr.sin_port),
						   message.message.topic,
						   ((float)ntohs(value)) / 100);

				// case for FLOAT type topic
				} else if (message.message.type == FLOAT) {
					uint8_t sign = message.message.data[0];
					uint32_t value;
					memcpy((char*)&value, message.message.data + 1, 4);
					uint8_t floating_point = message.message.data[5];

					value = ntohl(value);
					float prelucrated = ((float)value) / (pow(10, (float)floating_point));
					prelucrated = (sign == 1) ? -prelucrated : prelucrated;

					printf("%s:%d - %s - FLOAT - %lf\n",
						   inet_ntoa(message.addr.sin_addr),
						   ntohs(message.addr.sin_port),
						   message.message.topic,
						   prelucrated);

				// case for STRING type topic
				} else if (message.message.type == STRING) {
					printf("%s:%d - %s - STRING - %s\n",
						   inet_ntoa(message.addr.sin_addr),
						   ntohs(message.addr.sin_port),
						   message.message.topic,
						   message.message.data);
				}
			}
		}
	}
	close(sockfd);

	return 0;
}
