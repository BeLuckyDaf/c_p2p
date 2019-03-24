//
// Created by Vladislav on 3/14/2019.
//

#ifndef P2P_SERVER_H
#define P2P_SERVER_H

#include "utils/alist.h"
#include "utils/sockutil.h"
#include <pthread.h>
#include "node.h"

#define MASTER_PORT 12000
#define MAX_QUEUE 3
#define PAYLOAD_BUFFER_SIZE 512
#define INITIAL_LIST_SIZE 10

#define LOCK(M) (pthread_mutex_lock(&(M)))
#define UNLOCK(M) (pthread_mutex_unlock(&(M)))

void p2p_initialize(char* name, char* ip, unsigned short port);
void p2p_initialize_network_connection(char *addr, unsigned short port);
void* p2p_process_client(void *cldata);
void* p2p_master_server();
void *sync_sender();
db_node* parse_sync_node(char* line);
char* node_to_string(db_node* node);
int send_sync(p_sockaddr_in address);
int handle_sync(int sockfd);
int send_request(db_node node, char* filepath);
int handle_request(int sockfd);
int find_and_request_file(char* filepath);

ssize_t p2p_find_end_line(char *str, char *delimiter);

#endif //P2P_SERVER_H
