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
p_array_list db;
db_node self;
char my_name[NODE_NAME_LENGTH];
unsigned short master_port;

char *shared_folder_path = "shared";
char *downloaded_folder_path = "downloaded";

// FILES

DIR* get_dir_ptr() {
    DIR* dirptr = opendir(shared_folder_path);
    if (dirptr == NULL) {
        if (mkdir(shared_folder_path, 0770) != 0) {
            printf("could not create shared dir, crashed");
            exit(-1);
        }
        dirptr = opendir(shared_folder_path);
    }
    return dirptr;
}

size_t get_file_size(char *path) {
    int fd = open(path, O_RDONLY);
    char p, c;
    size_t sz = 0;
    p = c = ' ';
    while(read(fd, &c, 1) > 0) {
        if (c == ' ') {
            p = c;
        } else if (c != ' ' && p == ' ') {
            p = c;
            sz++;
        }
    }

    return sz;
}

db_node* parse_sync_node(char* line) {
    db_node* node = (db_node*)malloc(sizeof(db_node));
    node->file_list = create_array_list(INITIAL_LIST_SIZE);

    char* buf = (char*)malloc(strlen(line) + 1);
    strcpy(buf, line);

    char* strtok_saveptr;
    char* strtok_ptr = strtok_r(buf, ":", &strtok_saveptr);
    strcpy(node->name, strtok_ptr);
    char ip[INET_ADDRSTRLEN];
    char port[6];
    strtok_ptr = strtok_r(NULL, ":", &strtok_saveptr);
    strcpy(ip, strtok_ptr);
    strtok_ptr = strtok_r(NULL, ":", &strtok_saveptr);
    strcpy(port, strtok_ptr);
    strtok_ptr = strtok_r(NULL, ":", &strtok_saveptr);

    inet_pton(AF_INET, ip, &node->address.sin_addr);
    node->address.sin_family = AF_INET;
    node->address.sin_port = htons(atoi(port));

    if (strtok_ptr != NULL && strlen(strtok_ptr) > 0) {
        char* strtok_file_saveptr;
        char* fileptr = strtok_r(strtok_ptr, ",", &strtok_file_saveptr);
        while(fileptr != NULL) {
            char* filepath = (char*)malloc(sizeof(PAYLOAD_BUFFER_SIZE));
            strcpy(filepath, fileptr);
            array_list_add(node->file_list, filepath);
            fileptr = strtok_r(NULL, ",", &strtok_file_saveptr);
        }
    }

    return node;
}

char* node_to_string(db_node* node) {
    char* str = (char*)malloc(PAYLOAD_BUFFER_SIZE);
    memset(str, 0, PAYLOAD_BUFFER_SIZE);
    char addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &node->address.sin_addr, addr, INET_ADDRSTRLEN);
    sprintf(str, "%s:%s:%d:", node->name, addr, ntohs(node->address.sin_port));

    int iter = array_list_iter(node->file_list);
    if (iter >= 0) {
        char* filepath = array_list_get(node->file_list, iter);
        strcat(str, filepath);
        iter = array_list_next(node->file_list, iter);
        while(iter >= 0) {
            char* filepath = array_list_get(node->file_list, iter);
            strcat(str, ",");
            strcat(str, filepath);
            iter = array_list_next(node->file_list, iter);
        }
    }

    return str;
}

char* node_to_string_no_files(db_node* node) {
    char* str = (char*)malloc(PAYLOAD_BUFFER_SIZE);
    memset(str, 0, PAYLOAD_BUFFER_SIZE);
    char addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &node->address.sin_addr, addr, INET_ADDRSTRLEN);
    sprintf(str, "%s:%s:%d:", node->name, addr, ntohs(node->address.sin_port));

    return str;
}

// END FILES

void p2p_initialize(char* name, char* ip, unsigned short port) {
    pthread_mutex_init(&mutex, NULL);
    peers_list = create_array_list(INITIAL_LIST_SIZE);
    db = create_array_list(INITIAL_LIST_SIZE);

    size_t len = strlen(name);
    size_t bytes_to_copy = len > NODE_NAME_LENGTH - 1 ? NODE_NAME_LENGTH - 1 : len;
    memcpy(my_name, name, bytes_to_copy);
    my_name[bytes_to_copy] = '\0';

    master_port = port;

    strcpy(self.name, my_name);
    self.file_list = create_array_list(INITIAL_LIST_SIZE);
    self.address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &self.address.sin_addr);

    struct stat st;
    if (stat(shared_folder_path, &st) == -1) {
        return;
    }

    DIR* dirptr = get_dir_ptr();
    struct dirent* entry;
    printf("Files found in shared folder:\n");
    while((entry = readdir(dirptr)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        file_data* fldata = (file_data*)malloc(sizeof(file_data));
        memset(fldata, 0, sizeof(file_data));
        strcpy(fldata->path, entry->d_name);
        char path[PAYLOAD_BUFFER_SIZE];
        sprintf(path, "%s/%s", shared_folder_path, fldata->path);
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
        array_list_add(self.file_list, fldata);
        printf("- %s (%ld words)\n", fldata->path, fldata->size);
    }
    printf("\n");
}

void p2p_initialize_network_connection(char *addr, unsigned short port) {
    struct sockaddr_in address;
    inet_pton(AF_INET, addr, &address.sin_addr);
    address.sin_port = htons(port);
    address.sin_family = AF_INET;
    send_sync(&address);
}

int is_known(db_node* node) {
    if (strcmp(node->name, self.name) == 0) return 1;
    int iter = array_list_iter(db);
    while(iter >= 0) {
        db_node* n = array_list_get(db, iter);
        if (strcmp(n->name, node->name) == 0) return 1;
        iter = array_list_next(db, iter);
    }
    return 0;
}

void recv_until(int sock, char* buf, size_t bytes, int flags) {
    size_t count = 0;
    ssize_t received = 0;
    while((received = recv(sock, buf, bytes-count, flags)) > 0) {
        count += received;
        if (count == bytes) return;
    }
    printf("Could not receive everything\n");
}

int handle_request(int sockfd) {
    char file_path[PAYLOAD_BUFFER_SIZE];

    recv_until(sockfd, file_path, PAYLOAD_BUFFER_SIZE, 0);
    char full_path[PAYLOAD_BUFFER_SIZE];
    sprintf(full_path, "%s/%s", shared_folder_path, file_path);
    int reqfd = open(full_path, O_RDONLY);
    size_t sz = get_file_size(full_path);
    char wordbuf[PAYLOAD_BUFFER_SIZE];
    sprintf(wordbuf, "%ld", sz);
    send(sockfd, wordbuf, PAYLOAD_BUFFER_SIZE, 0);
    int i = 0;

    while(sz > 0) {
        if (read(reqfd, wordbuf + i, 1) > 0) {
            if (wordbuf[i] == ' ') {
                send(sockfd, wordbuf, PAYLOAD_BUFFER_SIZE, 0);
                i = 0;
                memset(wordbuf, 0, PAYLOAD_BUFFER_SIZE);
                sz--;
            } else i++;
        } else {
            wordbuf[i] = '\0';
            send(sockfd, wordbuf, PAYLOAD_BUFFER_SIZE, 0);
            break;
        }
    }

    return 0;
}

int handle_sync(int sockfd) {
    char line[PAYLOAD_BUFFER_SIZE];
    memset(line, 0, PAYLOAD_BUFFER_SIZE);
    recv_until(sockfd, line, PAYLOAD_BUFFER_SIZE, 0);

    char log[1500];
    memset(log, 0, 1500);

    db_node* node = parse_sync_node(line);
    LOCK(mutex);
    int known = is_known(node);
    if (known == 0) {
        array_list_add(db, node);
        printf("\n");
        printf("New node connected: %s\n", node->name);
        printf("Files:\n");

        int iter = array_list_iter(node->file_list);
        while(iter >= 0) {
            char* filepath = array_list_get(node->file_list, iter);
            printf(" - %s\n", filepath);
            iter = array_list_next(node->file_list, iter);
        }
    }

    UNLOCK(mutex);

    recv_until(sockfd, line, PAYLOAD_BUFFER_SIZE, 0);
    int count = atoi(line);

    for (int i = 0; i < count; i++) {
        recv_until(sockfd, line, PAYLOAD_BUFFER_SIZE, 0);

        strcat(log, line);
        strcat(log, "\n");

        node = parse_sync_node(line);
        LOCK(mutex);
        if (known == 0) {
            array_list_add(db, node);
        }
        UNLOCK(mutex);
    }

    return 0;
}

int send_sync(p_sockaddr_in address) {
    int sock = create_tcp_socket();
    connect(sock, (p_sockaddr)address, sizeof(struct sockaddr_in));
    char command[PAYLOAD_BUFFER_SIZE];
    memset(command, 0, PAYLOAD_BUFFER_SIZE);
    command[0] = '1';
    send(sock, command, PAYLOAD_BUFFER_SIZE, 0);
    char* self_str = node_to_string(&self);
    send(sock, self_str, PAYLOAD_BUFFER_SIZE, 0);
    char count[PAYLOAD_BUFFER_SIZE];
    memset(count, 0, PAYLOAD_BUFFER_SIZE);
    sprintf(count, "%ld", db->count);
    send(sock, count, PAYLOAD_BUFFER_SIZE, 0);

    int iter = array_list_iter(db);
    while(iter >= 0) {
        char* str = node_to_string_no_files(array_list_get(db, iter));
        send(sock, str, PAYLOAD_BUFFER_SIZE, 0);
        iter = array_list_next(db, iter);
    }
    close(sock);

    return 0;
}

int send_request(db_node node, char* filepath) {
    int sock = create_tcp_socket();
    connect(sock, (p_sockaddr)&node.address, sizeof(struct sockaddr_in));
    char command[PAYLOAD_BUFFER_SIZE];
    memset(command, 0, PAYLOAD_BUFFER_SIZE);
    command[0] = '0';
    send(sock, command, PAYLOAD_BUFFER_SIZE, 0);
    send(sock, filepath, PAYLOAD_BUFFER_SIZE, 0);
    char word_count[PAYLOAD_BUFFER_SIZE];
    memset(word_count, 0, PAYLOAD_BUFFER_SIZE);
    recv_until(sock, word_count, PAYLOAD_BUFFER_SIZE, 0);
    int count = atoi(word_count);
    char line[PAYLOAD_BUFFER_SIZE];
    char full_path[PAYLOAD_BUFFER_SIZE];
    memset(full_path, 0, PAYLOAD_BUFFER_SIZE);
    sprintf(full_path, "%s/%s", downloaded_folder_path, filepath);

    struct stat st;
    if (stat(downloaded_folder_path, &st) == -1) {
        mkdir(downloaded_folder_path, 0744);
    }

    printf("\nFile contents (words):\n");
    int fd = open(full_path, O_WRONLY | O_CREAT, 0744);
    while(count > 0) {
        memset(line, 0, PAYLOAD_BUFFER_SIZE);
        recv_until(sock, line, PAYLOAD_BUFFER_SIZE, 0);
        write(fd, line, strlen(line));
        printf("- %s\n", line);
        count--;
    }
    printf("Done. File saved in \"%s\"\n", full_path);

    close(fd);
    close(sock);
    return 0;
}

void *sync_sender() {
    while(1) {
        LOCK(mutex);
        int iter = array_list_iter(db);
        while(iter >= 0) {
            db_node* node = array_list_get(db, iter);
            send_sync(&node->address);
            iter = array_list_next(db, iter);
        }
        UNLOCK(mutex);
        sleep(3);
    }
}

int find_and_request_file(char* filepath) {
    int iter = array_list_iter(db);
    while(iter >= 0) {
        db_node* node = array_list_get(db, iter);

        int file_iter = array_list_iter(node->file_list);
        while(iter >= 0) {
            char* file = array_list_get(node->file_list, file_iter);
            if (strcmp(file, filepath) == 0) {
                send_request(*node, file);
                return 0;
            }
            file_iter = array_list_next(node->file_list, file_iter);
        }
        iter = array_list_next(db, iter);
    }
    return -1;
}

// When the client connects to us, this function is called in a separate thread
void *p2p_process_client(void *cldata) {
    client_data data = *((client_data *) cldata);
    char command[PAYLOAD_BUFFER_SIZE];

    recv_until(data.clsock, command, PAYLOAD_BUFFER_SIZE, 0);
    switch (command[0]) {
        case '0':
            handle_request(data.clsock);
            break;
        case '1':
            handle_sync(data.clsock);
            break;
        default:
            printf("received shit\n");
            break;
    }

    close(data.clsock);
}

// TCP server which accepts connections.
void* p2p_master_server() {
    int master_server_socket = create_tcp_socket();
    p_sockaddr_in sin = create_sockaddr(master_port);
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
