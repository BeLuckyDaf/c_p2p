//
// Created by Vladislav on 3/14/2019.
//

#include <dirent.h>
#include <sys/stat.h>
#include "server.h"

char splitby = ':';
pthread_mutex_t mutex;
p_array_list peers_list;
p_array_list files_list;
char my_name[NODE_NAME_LENGTH];

char *dirpath = "shared";

// FILES

DIR* get_dir_ptr() {
    DIR* dirptr = opendir(dirpath);
    if (dirptr == NULL) {
        if (mkdir(dirpath, 0770) != 0) {
            printf("could not create shared dir, crashed");
            exit(-1);
        }
        dirptr = opendir(dirpath);
    }
    return dirptr;
}

int file_exists(char* path) {
    if (access(path, W_OK) != -1) return 1;
    return 0;
}

file_data* parse_file(char* line) {
    char* strtok_saveptr;
    char* part = strtok_r(line, ":", &strtok_saveptr);
    if (part == NULL) return NULL;
    file_data* data = (file_data*)malloc(sizeof(file_data));
    strcpy(data->path, part);
    part = strtok_r(NULL, ":", &strtok_saveptr);
    if (part == NULL) return NULL;
    int size = atoi(part);
    if (size == 0 && part[0] != '0') return NULL;
    data->size = (size_t) size;
    return data;
}

// END FILES

void p2p_initialize(char* name) {
    pthread_mutex_init(&mutex, NULL);
    peers_list = create_array_list(INITIAL_LIST_SIZE);
    files_list = create_array_list(INITIAL_LIST_SIZE);

    size_t len = strlen(name);
    size_t bytes_to_copy = len > NODE_NAME_LENGTH - 1 ? NODE_NAME_LENGTH - 1 : len;
    memcpy(my_name, name, bytes_to_copy);
    my_name[bytes_to_copy] = '\0';

    DIR* dirptr = get_dir_ptr();
    struct dirent* entry;
    while((entry = readdir(dirptr)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        file_data* fldata = (file_data*)malloc(sizeof(file_data));
        memset(fldata, 0, sizeof(file_data));
        strcpy(fldata->path, entry->d_name);
        char path[FILEPATH_LENGTH + 1];
        sprintf(path, "%s/%s", dirpath, fldata->path);
        int fd = open(path, O_RDONLY);
        ssize_t brd;
        char c;

        char word = 1;
        while((brd = read(fd, &c, 1)) > 0) {
            if (c != ' ' && c != '\n') {
                if (word == 1) {
                    fldata->size++;
                    word = 0;
                }
            } else word = 1;
        }
        array_list_add(files_list, fldata);
        printf("path: %s, size: %ld\n", fldata->path, fldata->size);
    }
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

    // send HELO message to say hello and start communication
    char message_buffer[PAYLOAD_BUFFER_SIZE];
    char strtok_buffer[PAYLOAD_BUFFER_SIZE];
    ssize_t bytes_read;

    // send HELO to the server
    sprintf(message_buffer, "%s\n%s\n.\n", COMMAND_HELO_TEXT, my_name);
    send(tcpsock, message_buffer, strlen(message_buffer), 0);

    p_network_node node;
    p_network_node server_node;
    char *saveptr;
    char *item;

    // read other nodes while possible
    if ((bytes_read = recv(tcpsock, strtok_buffer, PAYLOAD_BUFFER_SIZE - 1, 0)) > 0) {
        strtok_buffer[bytes_read] = '\0';

        item = strtok_r(strtok_buffer, MESSAGE_PART_DELIM, &saveptr);
        if (item == NULL || strcmp(item, MESSAGE_DELIM) == 0) {
            printf("invalid name received, connection discarded\n");
            return;
        }

        server_node = create_empty_node();
        strcpy(server_node->name, item);
        server_node->nodeaddr = *sin;
        LOCK(mutex);
        array_list_add(peers_list, server_node);
        UNLOCK(mutex);
        printf("NAME: %s, IP: %s, PORT: %d\n", server_node->name, addr, port);
        printf("connection successful\n");
    } else {
        printf("server was not added as a node\n");
        return;
    }

    // send get node list
    sprintf(message_buffer, "%s\n.\n", COMMAND_GET_NODE_LIST_TEXT);
    send(tcpsock, message_buffer, strlen(message_buffer), 0);

    if ((bytes_read = recv(tcpsock, strtok_buffer, PAYLOAD_BUFFER_SIZE - 1, 0)) > 0) {
        item = strtok_r(strtok_buffer, MESSAGE_PART_DELIM, &saveptr);
        while (item != NULL) {
            if (strcmp(item, MESSAGE_DELIM) == 0) {
                printf("imported all nodes from server\n");
                break;
            }
            node = parse_node(item, splitby);
            if (node != NULL) {
                LOCK(mutex);
                array_list_add(peers_list, node);
                UNLOCK(mutex);
            }
            item = strtok_r(NULL, MESSAGE_PART_DELIM, &saveptr);
        }
    }

    // send sync files
    char mbuf[PAYLOAD_BUFFER_SIZE];
    sprintf(mbuf, "%s", COMMAND_SYNC_FILE_LIST_TEXT);

    LOCK(mutex);
    int nodeiter = array_list_iter(files_list);
    file_data* fldata;
    while(nodeiter >= 0) {
        fldata = array_list_get(files_list, nodeiter);
        sprintf(mbuf + strlen(mbuf), "\n%s:%ld", fldata->path, fldata->size);
        nodeiter = array_list_next(files_list, nodeiter);
    }
    UNLOCK(mutex);

    sprintf(mbuf + strlen(mbuf), "\n.\n");
    send(tcpsock, mbuf, strlen(mbuf), 0);

    char *get_files_saveptr;
    if (recv(tcpsock, mbuf, PAYLOAD_BUFFER_SIZE, 0) < 1) {
        printf("did not receive file list\n");
        close(tcpsock);
        return;
    }
    char *line = strtok_r(mbuf, MESSAGE_PART_DELIM, &saveptr);
    while(line != NULL && strcmp(line, MESSAGE_DELIM) != 0) {
        file_data* data = parse_file(line);
        if (data != NULL) {
            LOCK(mutex);
            array_list_add(files_list, data);
            UNLOCK(mutex);
            printf("file: %s, size: %ld\n", data->path, data->size);
        }
        line = strtok_r(NULL, MESSAGE_PART_DELIM, &saveptr);
    }

    close(tcpsock);
}

int parse_command(char *command) {
    if (strcmp(command, COMMAND_HELO_TEXT) == 0) return COMMAND_HELO;
    else if (strcmp(command, COMMAND_GET_NODE_LIST_TEXT) == 0) return COMMAND_GET_NODE_LIST;
    else if (strcmp(command, COMMAND_SYNC_FILE_LIST_TEXT) == 0) return COMMAND_SYNC_FILE_LIST;
    else if (strcmp(command, COMMAND_RETR_TEXT) == 0) return COMMAND_RETR;
    else return -1;
}

// Supposendly, this message must be of the following format
// "COMMAND\n ... \n ... \n.\n", where " ... " stands for some
// payload data that some commands might want to have.
// Returns -1 when the command is invalid.
int handle_request(char *message_buffer, client_data cldata) {
    char *strtok_saveptr;
    char *message_line;
    char *helo_name;
    char address_buffer[INET_ADDRSTRLEN];
    p_network_node peer_node;
    inet_ntop(AF_INET, &cldata.claddr.sin_addr.s_addr, address_buffer, INET_ADDRSTRLEN);
    message_line = strtok_r(message_buffer, MESSAGE_PART_DELIM, &strtok_saveptr);

    printf("%s\n", message_buffer);
    int command = parse_command(message_line);
    int iterator = address_exists(peers_list, &cldata.claddr);

    // if turns out that we do not have them in our peers list
    if (iterator < 0) {
        // unauthorized node tries a command they don't have access to
        if (command != COMMAND_HELO) {
            printf("Unauthorized command attempt from %s\n", address_buffer);
            return -1;
        }

        // fetch the name which is supposed to be in the next line
        helo_name = strtok_r(NULL, MESSAGE_PART_DELIM, &strtok_saveptr);
        if (strcmp(helo_name, MESSAGE_DELIM) == 0) {
            printf("Name expected, got the end of message\n");
            return -1;
        }

        // create node and add it to the list
        LOCK(mutex);
        peer_node = add_node(peers_list, helo_name, cldata.claddr);
        UNLOCK(mutex);

        // print the newly added node
        printf("new node connected: ");
        print_node(peer_node);

        // he's gonna send GET_NODE_LIST
        // gotta respond here
        char message_buffer[PAYLOAD_BUFFER_SIZE];
        char *get_nodes_saveptr;
        if (recv(cldata.clsock, message_buffer, PAYLOAD_BUFFER_SIZE, 0) < 1) {
            printf("did not receive request for nodes\n");
            return 0;
        }
        char *line = strtok_r(message_buffer, MESSAGE_PART_DELIM, &get_nodes_saveptr);
        if (strcmp(line, COMMAND_GET_NODE_LIST_TEXT) != 0) {
            printf("expected node request command, received shit\n");
            return 0;
        }
        LOCK(mutex);
        int nodeiter = array_list_iter(peers_list);
        char address_buffer[INET_ADDRSTRLEN];
        p_network_node node;
        while(nodeiter >= 0) {
            node = array_list_get(peers_list, nodeiter);
            if (node->nodeaddr.sin_addr.s_addr != cldata.claddr.sin_addr.s_addr) {
                inet_ntop(AF_INET, &node->nodeaddr.sin_addr, address_buffer, INET_ADDRSTRLEN);
                sprintf(message_buffer, "%s:%s:%d\n", node->name, address_buffer, ntohs(node->nodeaddr.sin_port));
                send(cldata.clsock, message_buffer, strlen(message_buffer), 0);
            }
            nodeiter = array_list_next(peers_list, nodeiter);
        }
        UNLOCK(mutex);
        send(cldata.clsock, ".\n", strlen(".\n"), 0);

        // then he'll ask for the file list
        // gonna have to give it too
        char *get_files_saveptr;
        if (recv(cldata.clsock, message_buffer, PAYLOAD_BUFFER_SIZE, 0) < 1) {
            printf("did not receive request for files\n");
            return 0;
        }
        line = strtok_r(message_buffer, MESSAGE_PART_DELIM, &get_nodes_saveptr);
        if (strcmp(line, COMMAND_SYNC_FILE_LIST_TEXT) != 0) {
            printf("expected files request command, received shit\n");
            return 0;
        }
        line = strtok_r(NULL, MESSAGE_PART_DELIM, &get_nodes_saveptr);
        while(line != NULL && strcmp(line, MESSAGE_DELIM) != 0) {
            file_data* data = parse_file(line);
            if (data != NULL) {
                LOCK(mutex);
                array_list_add(files_list, data);
                UNLOCK(mutex);
                printf("file: %s, size: %ld\n", data->path, data->size);
            }
            line = strtok_r(NULL, MESSAGE_PART_DELIM, &get_nodes_saveptr);
        }
        LOCK(mutex);
        nodeiter = array_list_iter(files_list);
        file_data* fldata;
        while(nodeiter >= 0) {
            fldata = array_list_get(files_list, nodeiter);
            sprintf(message_buffer, "%s:%ld\n", fldata->path, fldata->size);
            send(cldata.clsock, message_buffer, strlen(message_buffer), 0);
            nodeiter = array_list_next(files_list, nodeiter);
        }
        UNLOCK(mutex);
        send(cldata.clsock, ".\n", strlen(".\n"), 0);


        // tell everyone that there is a new node
        nodeiter = array_list_iter(peers_list);
        int connsock;
        inet_ntop(AF_INET, &cldata.claddr.sin_addr, address_buffer, INET_ADDRSTRLEN);
        sprintf(message_buffer, "%s\n%s:%s:%d\n.\n", COMMAND_NEW_NODE_TEXT,
                helo_name, address_buffer, ntohs(cldata.claddr.sin_port));
        while(nodeiter >= 0) {
            connsock = create_tcp_socket();
            node = array_list_get(peers_list, nodeiter);
            if (node->nodeaddr.sin_addr.s_addr != cldata.claddr.sin_addr.s_addr) {
                if (connect(connsock, (p_sockaddr)&node->nodeaddr, sizeof(struct sockaddr_in)) != 0) {
                    printf("could not connect");
                } else {
                    send(cldata.clsock, message_buffer, strlen(message_buffer), 0);
                }
            }
            close(connsock);
            nodeiter = array_list_next(peers_list, nodeiter);
        }

        close(cldata.clsock);
        return 0;
    }

    switch (command) {
        case COMMAND_GET_NODE_LIST:
            printf("GET_NODE_LIST command received.\n");

            int nodeiter = array_list_iter(peers_list);
            char address_buffer[INET_ADDRSTRLEN];
            p_network_node node;
            while(nodeiter >= 0) {
                node = array_list_get(peers_list, nodeiter);
                if (node->nodeaddr.sin_addr.s_addr != cldata.claddr.sin_addr.s_addr) {
                    inet_ntop(AF_INET, &node->nodeaddr.sin_addr, address_buffer, INET_ADDRSTRLEN);
                    sprintf(message_buffer, "%s:%s:%d\n", node->name, address_buffer, ntohs(node->nodeaddr.sin_port));
                    send(cldata.clsock, message_buffer, strlen(message_buffer), 0);
                }
                nodeiter = array_list_next(peers_list, nodeiter);
            }
            UNLOCK(mutex);
            send(cldata.clsock, ".\n", strlen(".\n"), 0);
            break;
        case COMMAND_SYNC_FILE_LIST:
            printf("SYNC_FILE_LIST command received.\n");

            message_line = strtok_r(NULL, MESSAGE_PART_DELIM, &strtok_saveptr);
            while(message_line != NULL && strcmp(message_line, MESSAGE_DELIM) != 0) {
                file_data* data = parse_file(message_line);
                if (data != NULL) {
                    LOCK(mutex);
                    array_list_add(files_list, data);
                    UNLOCK(mutex);
                    printf("file: %s, size: %ld\n", data->path, data->size);
                }
                message_line = strtok_r(NULL, MESSAGE_PART_DELIM, &strtok_saveptr);
            }
            LOCK(mutex);
            nodeiter = array_list_iter(files_list);
            file_data* fldata;
            while(nodeiter >= 0) {
                fldata = array_list_get(files_list, nodeiter);
                sprintf(message_buffer, "%s:%ld\n", fldata->path, fldata->size);
                send(cldata.clsock, message_buffer, strlen(message_buffer), 0);
                nodeiter = array_list_next(files_list, nodeiter);
            }
            UNLOCK(mutex);
            send(cldata.clsock, ".\n", strlen(".\n"), 0);

            break;
        case COMMAND_NEW_NODE:
            printf("NEW_NODE command received.\n");
            message_line = strtok_r(NULL, MESSAGE_PART_DELIM, &strtok_saveptr);
            p_network_node new_node = parse_node(message_line, splitby);
            if (new_node != NULL) {
                LOCK(mutex);
                array_list_add(peers_list, new_node);
                UNLOCK(mutex);
                printf("new node added: %s\n", message_line);
            } else {
                printf("could not parse node\n");
            }
            break;
        case COMMAND_RETR:
            printf("RETR command received.\n");
            message_line = strtok_r(NULL, MESSAGE_PART_DELIM, &strtok_saveptr);
            char path[FILEPATH_LENGTH];
            sprintf(path, "%s/%s", dirpath, message_line);
            char msgbuf[PAYLOAD_BUFFER_SIZE];
            char c;
            size_t ind = 0;
            memset(msgbuf, 0, PAYLOAD_BUFFER_SIZE);
            if (file_exists(path) == 1) {
                int fd = open(path, O_RDONLY);
                if (fd == -1) {
                    printf("could not open file '%s'\n", path);
                    break;
                }
                while(read(fd, &c, 1) > 0) {
                    if (ind == PAYLOAD_BUFFER_SIZE-3) break;
                    if (c == ' ' || c == '\n') msgbuf[ind] = '\n';
                    else msgbuf[ind] = c;
                    ind++;
                }
                msgbuf[PAYLOAD_BUFFER_SIZE-3] = '\n';
                msgbuf[PAYLOAD_BUFFER_SIZE-2] = '.';
                msgbuf[PAYLOAD_BUFFER_SIZE-1] = '\n';
                send(cldata.clsock, msgbuf, strlen(msgbuf), 0);
                close(fd);
            }
            break;
        default:
            printf("invalid command received: %s\n", message_line);
    }

    close(cldata.clsock);
    return 0;
}

// find the end of the message
ssize_t p2p_find_end_line(char *str, char *delimiter) {
    char *saveptr;
    char *copy = (char *) malloc(strlen(str));
    strcpy(copy, str);
    char *line = strtok_r(copy, "\n", &saveptr);
    while (line != NULL) {
        if (strcmp(line, delimiter) == 0) return line - copy;
        line = strtok_r(NULL, "\n", &saveptr);
    }
    return -1;
}

// When the client connects to us, this function is called in a separate thread
void * p2p_process_client(void *cldata) {
    client_data data = *((client_data*)cldata);
    char input_stream_buffer[PAYLOAD_BUFFER_SIZE];
    char *message_buffer;

    int message_end, next_message_start;
    int corrupted_message = 0;
    size_t input_buffer_end_index = 0;
    ssize_t bytes_received;

    // receive from the socket while possible
    while(1) {
        // read from the socket waiting for further instructions
        if ((bytes_received = recv(data.clsock, input_stream_buffer + input_buffer_end_index,
                                   INPUT_STREAM_BUFFER_SIZE - input_buffer_end_index, 0)) > 0) {
            input_buffer_end_index += bytes_received;
            message_end = (int) p2p_find_end_line(input_stream_buffer, MESSAGE_DELIM);
            next_message_start = message_end + 2;

            if (message_end == -1) {
                if (input_buffer_end_index == INPUT_STREAM_BUFFER_SIZE) {
                    corrupted_message = 1;
                    input_buffer_end_index = 0;
                    printf("marked current message in the buffer as 'corrupted'");
                }
            } else {
                // if the message is not marked as corrupted, fill the message buffer
                if (corrupted_message == 0) {
                    message_buffer = (char *) malloc(message_end + 3);
                    memcpy(message_buffer, input_stream_buffer, message_end + 2);
                    message_buffer[message_end + 2] = '\0';
                    message_buffer[message_end + 1] = '\n';
                    message_buffer[message_end - 1] = '\n';
                }

                // reset the index to the next message
                // regardless of the validity of the previous message
                memcpy(input_stream_buffer,
                       input_stream_buffer + next_message_start,
                       input_buffer_end_index - next_message_start);
                input_buffer_end_index -= next_message_start;

                // if the message was valid, proceed with handling the request
                // otherwise, clean the corrupted flag, that message is gone now
                if (corrupted_message == 0) {
                    handle_request(message_buffer, data);
                    free(message_buffer);
                }
                else corrupted_message = 0;
            }
        } else break;
    }

    close(data.clsock);
}

// TCP server which accepts connections.
void* p2p_master_server() {
    int master_server_socket = create_tcp_socket();
    p_sockaddr_in sin = create_sockaddr(MASTER_PORT);
    if (bind_socket(master_server_socket, sin) != 0) {
        printf("could not bind\n");
    }

    if (listen(master_server_socket, MAX_QUEUE) != 0) {
        printf("could not listen\n");
    }

    printf("Listening at port %d\n", ntohs(sin->sin_port));

    while(1) {
        struct sockaddr_in cin = {0};
        client_data *data = (client_data *) malloc(sizeof(data));
        socklen_t socket_length = sizeof(cin);
        int client_socket = accept(master_server_socket, (p_sockaddr) &cin, &socket_length);

        if (client_socket == -1) {
            printf("Error accepting connection...");
            break;
        }

        data->claddr = cin;
        data->clsock = client_socket;
        pthread_t tid;
        pthread_create(&tid, NULL, p2p_process_client, data);
    }
}

// UDP server looking for pings from the clients.
void* p2p_ping_master() {
    int ping_master_socket = create_udp_socket();
    p_sockaddr_in my_address = create_sockaddr(PING_RCV_PORT);
    bind_socket(ping_master_socket, my_address);

    char received_message_buffer[16];
    char address_buffer[INET_ADDRSTRLEN];

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

        // converting client address to string for printing
        inet_ntop(AF_INET, &client_address.sin_addr.s_addr, address_buffer, INET_ADDRSTRLEN);

        if (strcmp(received_message_buffer, "ping") == 0) {
            sendto(ping_master_socket, PONG_MESSAGE, strlen(PONG_MESSAGE), 0, (p_sockaddr)&client_address, socklen);
            printf("echoed pong to %s:%d\n", address_buffer, ntohs(client_address.sin_port));
        } else if (strcmp(received_message_buffer, "pong") == 0) {
            int iter = array_list_iter(peers_list);
            p_network_node node = 0;
            while (iter >= 0) {
                node = array_list_get(peers_list, iter);
                if (node->nodeaddr.sin_addr.s_addr == client_address.sin_addr.s_addr) {
                    node->pingval = PING_TIMEOUT_VALUE;
                    break;
                }
                iter = array_list_next(peers_list, iter);
            }

            if (iter == -1) {
                printf("received pong from unknown address: %s:%d\n", address_buffer, ntohs(client_address.sin_port));
            } else {
                printf("received pong from %s (%s:%d)\n", node->name, address_buffer, ntohs(client_address.sin_port));
            }
        }
    }
}

// UDP client tries to ping each client
void* p2p_pinger() {
    int pinger_socket = create_udp_socket();

    char buf[16];

    while(1) {
        p_array_list remlist = create_array_list(10);
        p_network_node node;
        LOCK(mutex);
        int iter = array_list_iter(peers_list);
        while(iter >= 0) {
            memset(buf, 0, 16);
            node = array_list_get(peers_list, iter);
            struct sockaddr_in claddr = node->nodeaddr;
            claddr.sin_port = htons(12150);
            socklen_t socklen = sizeof(claddr);
            sendto(pinger_socket, PING_MESSAGE, strlen(PING_MESSAGE), 0, (p_sockaddr) &claddr, socklen);
            iter = array_list_next(peers_list, iter);
            if (--(node->pingval) < 0) {
                array_list_add(remlist, node);
                printf("%s:%d has been unresponsive for too long\n", inet_ntoa(claddr.sin_addr),
                       ntohs(claddr.sin_port));
                continue;
            }
            printf("pinged %s:%d (%d)\n", inet_ntoa(claddr.sin_addr), ntohs(claddr.sin_port), node->pingval);
        }

        int remiter = array_list_iter(remlist);
        while (remiter >= 0) {
            node = array_list_get(remlist, remiter);
            array_list_remove(peers_list, node);
            remiter = array_list_next(remlist, remiter);
        }
        UNLOCK(mutex);
        sleep(PING_DELAY_SECONDS);
    }
}
