//
// Created by Vladislav on 3/14/2019.
//

#include <ifaddrs.h>
#include "server.h"

int main(int argc, char** argv) {
    if (argc < 5) {
        printf("PASS THE FOLLOWING ARGS: <NAME> <DOWNLOAD FILE (+/-)> <MY IP> <MY PORT> <CONNECT TO IP> <CONNECT TO PORT>\n");
        printf("THE LAST TWO ARGUMENTS ARE NOT NECESSARY\n");
        return -1;
    }

    p2p_initialize(argv[1], argv[3], (ushort)atoi(argv[4]));
    if (argc > 6) p2p_initialize_network_connection(argv[5], (ushort)atoi(argv[6]));

    pthread_t master_server_tid;
    pthread_t sync_master_tid;
    pthread_create(&master_server_tid, NULL, p2p_master_server, NULL);
    pthread_create(&sync_master_tid, NULL, sync_sender, NULL);

    sleep(1);

    if (strcmp(argv[2], "+") == 0) {
        while(1) {
            printf("<Input>: please type filename or '.' to cancel requesting\n");
            char file[PAYLOAD_BUFFER_SIZE];
            scanf("%s", file);
            if (strcmp(file, ".") != 0) {
                if (find_and_request_file(file) == -1) {
                    printf("<Request>: could not find node sharing file \"%s\"\n", file);
                }
            } else break;
        }
    }

    pthread_join(master_server_tid, NULL);

    return 0;
}