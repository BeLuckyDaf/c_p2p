/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <beluckydaf@gmail.com> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return. Vladislav Smirnov
 * ----------------------------------------------------------------------------
 */

#include "sockutil.h"

int create_tcp_socket() {
    return socket(AF_INET, SOCK_STREAM, 0);
}

int create_udp_socket() {
    return socket(AF_INET, SOCK_DGRAM, 0);
}

p_sockaddr_in create_sockaddr(unsigned short port) {
    p_sockaddr_in addr = (p_sockaddr_in)malloc(sizeof(struct sockaddr_in));
    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = INADDR_ANY;
    addr->sin_port = htons(port);
    return addr;
}

int bind_socket(int sockfd, p_sockaddr_in addr) {
    return bind(sockfd, (p_sockaddr)addr, sizeof(struct sockaddr_in));
}

int set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL);
    return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}
