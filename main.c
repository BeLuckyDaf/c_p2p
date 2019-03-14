//
// Created by Vladislav on 3/14/2019.
//

#include "server.h"

int main(int argc, char** argv) {
    if (argc < 2) return -1;
    
    p2p_initialize(argv[1]);
    if (argc > 3) p2p_initialize_network_connection(argv[2], atoi(argv[3]));

    pthread_t master_server_tid;
    pthread_t ping_master_tid;
    pthread_t pinger_tid;
    pthread_create(&master_server_tid, NULL, p2p_master_server, NULL);
    pthread_create(&ping_master_tid, NULL, p2p_ping_master, NULL);
    pthread_create(&pinger_tid, NULL, p2p_pinger, NULL);

    pthread_join(master_server_tid, NULL);

    return 0;
}