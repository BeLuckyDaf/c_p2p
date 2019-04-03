/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <beluckydaf@gmail.com> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return. Vladislav Smirnov
 * ----------------------------------------------------------------------------
 */

#ifndef _SOCKUTIL_H_
#define _SOCKUTIL_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

typedef struct sockaddr_in* p_sockaddr_in;
typedef struct sockaddr* p_sockaddr;

int create_tcp_socket();
int create_udp_socket();
p_sockaddr_in create_sockaddr(unsigned short port);
int bind_socket(int sockfd, p_sockaddr_in addr);
int set_nonblocking(int sockfd);

#endif // _SOCKUTIL_H_