#include "../third_party/sockutil.h"

int main(int argc, char** argv) {
    if (argc < 5) return -1;

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &addr.sin_addr);
    addr.sin_port = htons(atoi(argv[2]));

    unsigned int msg = htonl(1);
    int iterations = atoi(argv[3]);
    int times = atoi(argv[4]);

    int* sockets = (int*)malloc(times*sizeof(int));
    memset(sockets, 0, sizeof(int)*times);

    for (int j = 0; j < iterations; j++) {
        for (int i = 0; i < times; i++) {
            sockets[i] = create_tcp_socket();
            if (sockets[i] == -1) {
                printf("0");
                continue;
            }

            if (connect(sockets[i], (p_sockaddr) &addr, sizeof(struct sockaddr_in)) == -1) {
                printf("-");
                continue;
            }

            write(sockets[i], (char *) &msg, sizeof(int));
            printf("+");
        }

        for (int i = 0; i < times; i++) {
            close(sockets[i]);
            memset(sockets, 0, sizeof(int)*times);
        }

        printf("\n");
        sleep(3);
    }

    return 0;
}

