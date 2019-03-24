//
// Created by Vladislav on 3/14/2019.
//

#ifndef P2P_NODE_H
#define P2P_NODE_H

#define NODE_NAME_LENGTH 30
#define PING_TIMEOUT_VALUE 30
#define FILEPATH_LENGTH 256

typedef struct {
    char name[NODE_NAME_LENGTH];
    struct sockaddr_in nodeaddr;
    int pingval;
} network_node;

typedef struct {
    struct sockaddr_in claddr;
    int clsock;
} client_data;

typedef struct {
    char path[FILEPATH_LENGTH];
    size_t size;
} file_data;

typedef struct {
    char name[NODE_NAME_LENGTH];
    struct sockaddr_in address;
    p_array_list file_list;
} db_node;

typedef network_node* p_network_node;

p_network_node create_empty_node();
void print_node(p_network_node node);
p_network_node create_node(char* name, char* ip, unsigned short port);
p_network_node parse_node(char* line, char delim);
int find_char(char ch, char* str, int from, int to, int seq);
int find_char_ind(char ch, char* str, size_t len);
p_network_node add_node(p_array_list alist, char *name, struct sockaddr_in address);

int address_exists(p_array_list alist, p_sockaddr_in address);

#endif //P2P_NODE_H
