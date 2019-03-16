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
#define PING_RCV_PORT 12100
#define PAYLOAD_BUFFER_SIZE 512
#define INPUT_STREAM_BUFFER_SIZE 512
#define PING_DELAY_SECONDS 2
#define PING_MESSAGE "ping"
#define PONG_MESSAGE "pong"
#define INITIAL_LIST_SIZE 10

#define LOCK(M) (pthread_mutex_lock(&(M)))
#define UNLOCK(M) (pthread_mutex_unlock(&(M)))
#define MESSAGE_DELIM "."
#define MESSAGE_PART_DELIM "\n"
#define MESSAGE_DELIM_CH '.'
#define MESSAGE_PART_DELIM_CH '\n'

#define COMMAND_HELO 0
#define COMMAND_HELO_TEXT "HELO"
#define COMMAND_GET_NODE_LIST 1
#define COMMAND_GET_NODE_LIST_TEXT "GET_NODE_LIST"
#define COMMAND_SYNC_FILE_LIST 2
#define COMMAND_SYNC_FILE_LIST_TEXT "SYNC_FILE_LIST"
#define COMMAND_NEW_NODE 3
#define COMMAND_NEW_NODE_TEXT "NEW_NODE"
#define COMMAND_RETR 4
#define COMMAND_RETR_TEXT "RETR"

void p2p_initialize(char* name);
void p2p_initialize_network_connection(char *addr, unsigned short port);
void* p2p_process_client(void *cldata);
void* p2p_master_server();
void* p2p_ping_master();
void* p2p_pinger();

ssize_t p2p_find_end_line(char *str, char *delimiter);

#endif //P2P_SERVER_H
