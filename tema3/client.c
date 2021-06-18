#include <stdio.h>      /* printf, sprintf */
#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <unistd.h>     /* read, write, close */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <arpa/inet.h>
#include "helpers.h"
#include "requests.h"
#include "parson.h"

#define SERVER_PORT 8080
#define COMMAND_LEN 20
#define COOKIE_LEN 256
#define TOKEN_LEN 512
#define PATH_LEN 80

int main(int argc, char *argv[])
{
    char *message;
    char *response;
    int sockfd;

    char HOST_IP[] = "34.118.48.238";

    char JSON_CONTENT_TYPE[] = "application/json";
    
    char REGISTER_PATH[] = "/api/v1/tema/auth/register";
    char LOGIN_PATH[] = "/api/v1/tema/auth/login";
    char LOGOUT_PATH[] = "/api/v1/tema/auth/logout";
    char ACCESS_PATH[] = "/api/v1/tema/library/access";
    char ALL_BOOKS_PATH[] = "/api/v1/tema/library/books";

    char login_cookie[COOKIE_LEN];
    memset(login_cookie, 0, COOKIE_LEN);

    char access_token[TOKEN_LEN];
    memset(access_token, 0, TOKEN_LEN);

    while (1) {
        char command[COMMAND_LEN];
        scanf("%s", command);

        sockfd = open_connection(HOST_IP, SERVER_PORT, AF_INET, SOCK_STREAM, 0);

        if (strcmp(command, "register") == 0) {
            char username[COMMAND_LEN];
            char password[COMMAND_LEN];
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            char *serialized_string = NULL;

            printf("username=");
            scanf("%s", username);
            json_object_set_string(root_object, "username", username);

            printf("password=");
            scanf("%s", password);
            json_object_set_string(root_object, "password", password);

            serialized_string = json_serialize_to_string_pretty(root_value);
            message = compute_post_request(HOST_IP, REGISTER_PATH, JSON_CONTENT_TYPE, serialized_string, NULL, NULL, 0);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);

            JSON_Value *response_json = json_parse_string(strstr(response, "{"));
            if (response_json == NULL) {
                printf("Successfully registered\n");
            } else {
                printf("%s\n", json_object_get_string(json_object(response_json), "error"));
            }

            json_free_serialized_string(serialized_string);
            json_value_free(response_json);
            json_value_free(root_value);
        } else if (strcmp(command, "login") == 0) {
            char username[COMMAND_LEN];
            char password[COMMAND_LEN];
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            char *serialized_string = NULL;

            printf("username=");
            scanf("%s", username);
            json_object_set_string(root_object, "username", username);

            printf("password=");
            scanf("%s", password);
            json_object_set_string(root_object, "password", password);

            serialized_string = json_serialize_to_string_pretty(root_value);
            message = compute_post_request(HOST_IP, LOGIN_PATH, JSON_CONTENT_TYPE, serialized_string, NULL, NULL, 0);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);

            JSON_Value *response_json = json_parse_string(strstr(response, "{"));
            if (response_json == NULL) {
                printf("Successfully logged in\n");
                char *pch = strtok(response, "\n");
                while (pch != NULL) {
                    if (strncmp(pch, "Set-Cookie", strlen("Set-Cookie")) == 0) {
                        sscanf(pch, "Set-Cookie: %s; Path=/; HttpOnly", login_cookie);
                        login_cookie[strlen(login_cookie) - 1] = '\0';
                        break;
                    }
                    pch = strtok(NULL, "\n");
                }
            } else {
                printf("%s\n", json_object_get_string(json_object(response_json), "error"));
            }

            json_value_free(response_json);
            json_free_serialized_string(serialized_string);
            json_value_free(root_value);
        } else if (strcmp(command, "logout") == 0) {
            if (strlen(login_cookie) == 0) {
                message = compute_get_request(HOST_IP, LOGOUT_PATH, NULL, NULL, NULL, 0);
            } else {
                char** cookies = calloc(1, sizeof(char *));
                cookies[0] = calloc(COOKIE_LEN, sizeof(char));
                strcpy(cookies[0], login_cookie);

                message = compute_get_request(HOST_IP, LOGOUT_PATH, NULL, NULL, cookies, 1);

                free(cookies[0]);
                free(cookies);
            };

            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);

            JSON_Value *response_json = json_parse_string(strstr(response, "{"));
            if (response_json == NULL) {
                printf("Successfully logged out\n");
                memset(login_cookie, 0, COOKIE_LEN);
            } else {
                printf("%s\n", json_object_get_string(json_object(response_json), "error"));
            }

            json_value_free(response_json);
        } else if (strcmp(command, "enter_library") == 0) {
            if (strlen(login_cookie) == 0) {
                message = compute_get_request(HOST_IP, ACCESS_PATH, NULL, NULL, NULL, 0);
            } else {
                char** cookies = calloc(1, sizeof(char *));
                cookies[0] = calloc(COOKIE_LEN, sizeof(char));
                strcpy(cookies[0], login_cookie);

                message = compute_get_request(HOST_IP, ACCESS_PATH, NULL, NULL, cookies, 1);

                free(cookies[0]);
                free(cookies);
            }

            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);

            JSON_Value *response_json = json_parse_string(strstr(response, "{"));
            JSON_Object *response_json_object = json_object(response_json);
            if (json_object_has_value(response_json_object, "token")) {
                printf("Successfully entered library\n");
                strcpy(access_token, json_object_get_string(response_json_object, "token"));
            } else if (json_object_has_value(response_json_object, "error")) {
                printf("%s\n", json_object_get_string(response_json_object, "error"));
            }

            json_value_free(response_json);
        } else if (strcmp(command, "get_books") == 0) {
            if (strlen(access_token) == 0) {
                message = compute_get_request(HOST_IP, ALL_BOOKS_PATH, NULL, NULL, NULL, 0); 
            } else {
                message = compute_get_request(HOST_IP, ALL_BOOKS_PATH, NULL, access_token, NULL, 0); 
            }

            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);

            JSON_Value *response_json = json_parse_string(strstr(response, "["));
            if (response_json == NULL) {
                response_json = json_parse_string(strstr(response, "{"));
                printf("%s\n", json_object_get_string(json_object(response_json), "error"));
            } else {
                printf("Successfully received books\n");
                printf("%s\n", json_serialize_to_string_pretty(response_json));
            }

            json_value_free(response_json);
        } else if(strcmp(command, "get_book") == 0) {
            int id;
            char book_path[PATH_LEN];

            printf("id=");
            scanf("%d", &id);
            sprintf(book_path, "%s/%d", ALL_BOOKS_PATH, id);

            if (strlen(access_token) == 0) {
                message = compute_get_request(HOST_IP, book_path, NULL, NULL, NULL, 0); 
            } else {
                message = compute_get_request(HOST_IP, book_path, NULL, access_token, NULL, 0); 
            }

            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);

            JSON_Value *response_json = json_parse_string(strstr(response, "{"));
            JSON_Object *response_json_object = json_object(response_json);
            if (json_object_has_value(response_json_object, "error")) {
                printf("%s\n", json_object_get_string(response_json_object, "error"));
            } else {
                printf("Successfully received book\n");
                printf("%s\n", json_serialize_to_string_pretty(response_json));
            }

            json_value_free(response_json);
        } else if (strcmp(command, "add_book") == 0) {
            char title[COMMAND_LEN];
            char author[COMMAND_LEN];
            char genre[COMMAND_LEN];
            char publisher[COMMAND_LEN];
            int page_count;
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            char *serialized_string = NULL;

            printf("title=");
            scanf("%s", title);
            json_object_set_string(root_object, "title", title);

            printf("author=");
            scanf("%s", author);
            json_object_set_string(root_object, "author", author);

            printf("genre=");
            scanf("%s", genre);
            json_object_set_string(root_object, "genre", genre);

            printf("page_count=");
            int res = scanf("%d", &page_count);
            json_object_set_number(root_object, "page_count", (double)page_count);
            if (res != 1) {
                printf("Error parsing POST data\n");
                goto invalid_command;
            }

            printf("publisher=");
            scanf("%s", publisher);
            json_object_set_string(root_object, "publisher", publisher);

            serialized_string = json_serialize_to_string_pretty(root_value);
            if (strlen(access_token) == 0) {
                message = compute_post_request(HOST_IP, ALL_BOOKS_PATH, JSON_CONTENT_TYPE, serialized_string, NULL, NULL, 0);
            } else {
                message = compute_post_request(HOST_IP, ALL_BOOKS_PATH, JSON_CONTENT_TYPE, serialized_string, access_token, NULL, 0);
            }

            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);

            JSON_Value *response_json = json_parse_string(strstr(response, "{"));
            if (response_json == NULL) {
                printf("Successfully added book\n");
            } else {
                printf("%s\n", json_object_get_string(json_object(response_json), "error"));
            }

            json_free_serialized_string(serialized_string);
            json_value_free(response_json);
            json_value_free(root_value);
        } else if (strcmp(command, "delete_book") == 0) {
            int id;
            char book_path[PATH_LEN];

            printf("id=");
            scanf("%d", &id);
            sprintf(book_path, "%s/%d", ALL_BOOKS_PATH, id);

            if (strlen(access_token) == 0) {
                message = compute_delete_request(HOST_IP, book_path, NULL, NULL, 0); 
            } else {
                message = compute_delete_request(HOST_IP, book_path, access_token, NULL, 0); 
            }

            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);

            JSON_Value *response_json = json_parse_string(strstr(response, "{"));
            if (response_json == NULL) {
                printf("Successfully deleted book\n");
            } else {
                printf("%s\n", json_object_get_string(json_object(response_json), "error"));
            }

            json_value_free(response_json);
        } else if (strcmp(command, "exit") == 0) {
            close_connection(sockfd);
            break;
        } else {
            printf("Invalid command\n");
            goto invalid_command;
        }
        free(message);

        invalid_command:
        close_connection(sockfd);
    }
    return 0;
}
