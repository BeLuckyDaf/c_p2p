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
#define PONG_MESSAGE "pong"
#define INITIAL_LIST_SIZE 10

#define LOCK(M) (pthread_mutex_lock(&(M)))
#define UNLOCK(M) (pthread_mutex_unlock(&(M)))
#define MESSAGE_DELIM "."
#define MESSAGE_PART_DELIM "\n"
#define MESSAGE_DELIM_CH '.'
#define MESSAGE_PART_DELIM_CH '\n'

void p2p_initialize(char* name);
void p2p_initialize_network_connection(char *addr, unsigned short port);
void* p2p_process_client(void *cldata);
void* p2p_master_server();
void* p2p_ping_master();
void* p2p_pinger();

#endif //P2P_SERVER_H
