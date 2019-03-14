//
// Created by Vladislav on 3/14/2019.
//

#include "server.h"

char splitby = ':';
pthread_mutex_t mutex;
p_array_list peers_list;
char my_name[NODE_NAME_LENGTH];

void p2p_initialize(char* name) {
    pthread_mutex_init(&mutex, NULL);
    peers_list = create_array_list(INITIAL_LIST_SIZE);

    size_t len = strlen(name);
    size_t bytes_to_copy = len > NODE_NAME_LENGTH - 1 ? NODE_NAME_LENGTH - 1 : len;
    memcpy(my_name, name, bytes_to_copy);
    my_name[bytes_to_copy] = '\0';
}

void p2p_initialize_network_connection(char *addr, unsigned short port) {
    // creating my master socket
    int tcpsock = create_tcp_socket();

    // first peer address here
    p_sockaddr_in sin = create_sockaddr(port);
    inet_pton(AF_INET, addr, &sin->sin_addr.s_addr);

    // try to connect to the remove host
    if (connect(tcpsock, (p_sockaddr)sin, sizeof(struct sockaddr_in)) != 0) {
        printf("unsuccessful connection, %s:%d, errno: %d\n", addr, ntohs(sin->sin_port), errno);
        return;
    }

    // send GET_LIST message to say hello and start communication
    char message[100];
    char buf[512];
    ssize_t bread;

    sprintf(message, "GET_LIST\n%s\n.\n", my_name);
    send(tcpsock, message, strlen(message), 0);

    while ((bread = recv(tcpsock, buf, 511, 0)) > 0) {
        buf[bread] = '\0';
        pthread_mutex_lock(&mutex);
        char *item = strtok(buf, "\n");
        // change here!!!!!!!!! this loop will fuck it up.
        p_network_node node = create_empty_node();
        strcpy(node->name, item);
        node->nodeaddr = *sin;
        array_list_add(peers_list, node);
        int i = 1;
        while(item != NULL) {
            if (strcmp(item, ".") == 0) {
                pthread_mutex_unlock(&mutex);
                close(tcpsock);
                printf("Done.\n");
                return;
            }
            p_network_node node = parse_node(item, splitby);
            if (node != NULL) array_list_add(peers_list, node);
            item = strtok(NULL, "\n");
            i++;
        }
        pthread_mutex_unlock(&mutex);
    }
    printf("Done.\n");
}

// Supposendly, this message must be of the following format
// "COMMAND\n ... \n ... \n.\n", where " ... " stands for some
// payload data that some commands might want to have.
// Returns -1 when the command is invalid.
int handle_request(char * message_buffer) {

    /*char* line = strtok(input_stream_buffer, "\n");
    while(line != NULL) {
        if (strcmp(line, "GET_LIST") == 0) {
            // printf("GET_LIST RECEIVED\n");
            iter = array_list_iter(peers_list);
            line = strtok(NULL, "\n");
            strcpy(new_node_name, line);
            sprintf(message_buffer, "%s\n", node_name);
            send(data.clsock, message_buffer, strlen(message_buffer), 0);
            while(iter >= 0) {
                memset(address_buffer, 0, INET_ADDRSTRLEN);
                LOCK(mutex);
                p_network_node node = array_list_get(peers_list, iter);
                UNLOCK(mutex);
                inet_ntop(AF_INET, &node->nodeaddr.sin_addr, address_buffer, INET_ADDRSTRLEN);
                sprintf(message_buffer, "%s:%s:%d\n", node->name, address_buffer, ntohs(node->nodeaddr.sin_port));
                send(data.clsock, message_buffer, strlen(message_buffer), 0);
                iter = array_list_next(peers_list, iter);
            }
            strcpy(message_buffer, ".\n");
            send(data.clsock, message_buffer, strlen(message_buffer), 0);
        } else if (strcmp(line, ".") == 0) {
            // end of command
        }
        line = strtok(NULL, "\n");
    }*/

    free(message_buffer); // message buffer is stored in the heap, free it
}

// When the client connects to us, this function is called in a separate thread.
// It basically tries to talk to the client and adds them to the list
void * p2p_process_client(void *cldata) {
    client_data data = *((client_data*)cldata);
    char address_buffer[INET_ADDRSTRLEN];
    char input_stream_buffer[PAYLOAD_BUFFER_SIZE];
    char new_node_name[NODE_NAME_LENGTH];
    char *message_buffer;

    int message_end, next_message_start;
    int corrupted_message = 0;
    size_t input_buffer_end_index = 0;
    ssize_t bytes_received;

    LOCK(mutex);
    int iter = array_list_iter(peers_list);
    UNLOCK(mutex);
    p_network_node peer_node;

    // receive client's address and check if the client is in the list
    // might want to try to make this a separate function
    inet_ntop(AF_INET, &data.claddr.sin_addr.s_addr, address_buffer, INET_ADDRSTRLEN);
    LOCK(mutex);
    while(iter >= 0) {
        peer_node = array_list_get(peers_list, iter);
        if (peer_node->nodeaddr.sin_addr.s_addr == data.claddr.sin_addr.s_addr) break;
        iter = array_list_next(peers_list, iter);
    }
    UNLOCK(mutex);

    // if turns out that we do not have them in our peers list
    if (iter < 0) {
        // create node and add it to the list
        LOCK(mutex);
        peer_node = add_node(peers_list, new_node_name, data.claddr);
        UNLOCK(mutex);

        // print the newly added node
        printf("new node connected: ");
        print_node(peer_node);
    }

    // receive from the socket while possible
    // TODO drop the peer from the list when it's gone
    while(1) {
        // read from the socket waiting for further instructions
        if ((bytes_received = recv(data.clsock, input_stream_buffer + input_buffer_end_index,
                                   INPUT_STREAM_BUFFER_SIZE - input_buffer_end_index, 0)) > 0) {
            input_buffer_end_index += bytes_received;
            message_end = find_char_ind(MESSAGE_DELIM_CH, input_stream_buffer, INPUT_STREAM_BUFFER_SIZE);
            next_message_start = message_end + 2;

            if (message_end == -1) {
                if (input_buffer_end_index == INPUT_STREAM_BUFFER_SIZE) corrupted_message = 1;
                input_buffer_end_index = 0;
            } else {
                // if the message is not marked as corrupted, fill the message buffer
                if (corrupted_message == 0) {
                    message_buffer = (char *) malloc(message_end + 3);
                    memcpy(message_buffer, input_stream_buffer, message_end + 2);
                    message_buffer[message_end + 2] = '\0';
                }

                // reset the index to the next message
                // regardless of the validity of the previous message
                memcpy(input_stream_buffer,
                       input_stream_buffer + next_message_start,
                       input_buffer_end_index - next_message_start);
                input_buffer_end_index -= next_message_start;

                // if the message was valid, proceed with handling the request
                // otherwise, clean the corrupted flag, that message is gone now
                if (corrupted_message == 0) handle_request(message_buffer);
                else corrupted_message = 0;
            }
        } else break; // break if met EOF, better to erase the client from the list here
    }

    close(data.clsock);
}

// TCP server which accepts connections.
void* p2p_master_server() {
    int mastersock = create_tcp_socket();
    p_sockaddr_in sin = create_sockaddr(MASTER_PORT);
    if (bind_socket(mastersock, sin) != 0) {
        printf("could not bind\n");
    }

    if(listen(mastersock, MAX_QUEUE) != 0) {
        printf("could not listen\n");
    }

    printf("Listening at port %d\n", ntohs(sin->sin_port));

    while(1) {
        struct sockaddr_in cin = {0};
        client_data* cldata = (client_data*)malloc(sizeof(client_data));
        socklen_t socklen = sizeof(cin);
        int clientsock = accept(mastersock, (p_sockaddr)&cin, &socklen);

        if (clientsock == -1) {
            printf("Error accepting connection...");
            break;
        }

        cldata->claddr = cin;
        cldata->clsock = clientsock;
        pthread_t tid;
        pthread_create(&tid, NULL, p2p_process_client, cldata);
    }
}

// UDP server looking for pings from the clients.
void* p2p_ping_master() {
    int ping_master_socket = create_udp_socket();
    p_sockaddr_in my_address = create_sockaddr(PING_RCV_PORT);
    bind_socket(ping_master_socket, my_address);

    char received_message_buffer[16];

    while(1) {
        memset(received_message_buffer, 0, 16);
        struct sockaddr_in client_address;
        socklen_t socklen = sizeof(client_address);

        ssize_t bytes_received = recvfrom(ping_master_socket, received_message_buffer, 16, 0,
                                   (p_sockaddr)&client_address, &socklen);
        if (bytes_received == -1) {
            printf("Ping master received -1 byte, error: %d", errno);
            break; // TODO make something smarter, it should not break here
        }

        if (strcmp(received_message_buffer, "ping") == 0) {
            sendto(ping_master_socket, PONG_MESSAGE, strlen(PONG_MESSAGE), 0, (p_sockaddr)&client_address, socklen);
            printf("sent pong\n");
        } else if (strcmp(received_message_buffer, "pong") == 0) {
            printf("ponged\n");
        }
    }
}

// UDP client tries to ping each client
void* p2p_pinger() {
    int pinger_socket = create_udp_socket();

    char buf[16];
    char *message = "ping";

    while(1) {
        LOCK(mutex);
        int iter = array_list_iter(peers_list);
        while(iter >= 0) {
            memset(buf, 0, 16);
            p_network_node node = array_list_get(peers_list, iter);
            struct sockaddr_in claddr = node->nodeaddr;
            claddr.sin_port = htons(12150);
            socklen_t socklen = sizeof(claddr);
            sendto(pinger_socket, message, strlen(message), 0, (p_sockaddr)&claddr, socklen);
            iter = array_list_next(peers_list, iter);
            printf("pinged %s:%d\n", inet_ntoa(claddr.sin_addr), ntohs(claddr.sin_port));
        }
        UNLOCK(mutex);
        sleep(PING_DELAY_SECONDS);
    }
}
