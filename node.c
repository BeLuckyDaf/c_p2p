// by Vladislav Smirnov
// 10.03.2019

#include "utils/sockutil.h"
#include "utils/alist.h"
#include "node.h"

int min(int a, int b) {
    return a < b ? a : b;
}

// This simply prints the address of the node and its name
void print_node(p_network_node node) {
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &node->nodeaddr.sin_addr, buf, INET_ADDRSTRLEN);
    int port = ntohs(node->nodeaddr.sin_port);
    printf("%s:%s:%d\n", node->name, buf, port);
}

p_network_node create_empty_node() {
    p_network_node node = (p_network_node)malloc(sizeof(network_node));
    memset(node, 0, sizeof(network_node));
    return node;
}

p_network_node create_node(char* name, char* ip, unsigned short port) {
    p_network_node node = create_empty_node();
    if (inet_pton(AF_INET, ip, &node->nodeaddr.sin_addr) == 0) {
        free(node);
        printf("Could not create node, invalid address specified.\n");
        return NULL;
    }
    node->nodeaddr.sin_port = htons(port);
    memcpy(node->name, name, min(strlen(name), 29));
    node->name[min(strlen(name), 29)] = '\0';
    return node;
}

int find_char(char ch, char* str, int from, int to, int seq) {
    for (int i = from, k = seq; i < to; i++) {
        if (str[i] == ch && (--k) == 0) return i;
    }
    return -1;
}

int find_char_ind(char ch, char* str, size_t len) {
    for (int i = 0; i < len && str[i] != '\0'; i++) {
        if (str[i] == ch) return i;
    }
    return -1;
}

p_network_node add_node(p_array_list alist, char *name, struct sockaddr_in address) {
    // create node
    p_network_node new_node = create_empty_node();
    strcpy(new_node->name, name);
    new_node->nodeaddr = address;

    // add it to the list
    array_list_add(alist, new_node);

    return new_node;
}

// the line must be of format "NAME:XXX.XXX.XXX.XXX:XXXXX"
p_network_node parse_node(char* line, char delim) {
    if (find_char(delim, line, 0, strlen(line), 2) > 0) {
        int nameend = find_char(delim, line, 0, strlen(line), 1);
        char name[30];
        memcpy(name, line, min(nameend, 29));
        name[min(nameend, 29)] = '\0';
        
        int ipend = find_char(delim, line, nameend+1, strlen(line), 1);
        char ip[17];
        memcpy(ip, line+nameend+1, min(ipend-nameend-1, 16));
        ip[min(ipend-nameend-1, 16)] = '\0';

        int portend = strlen(line);
        char portstr[6];
        memcpy(portstr, line+ipend+1, min(portend-ipend-1, 5));
        portstr[min(portend-ipend-1, 5)] = '\0';

        unsigned short port = (unsigned short)atoi(portstr);
        if (port == 0) return NULL;

        printf("NAME: %s, IP: %s, PORT: %s\n", name, ip, portstr);
        return create_node(name, ip, port);
    }
    return NULL;
}
