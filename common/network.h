#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>

#include <stdint.h>

int network_server_socket(char *host, int port);
int network_client_socket(char *host, int port);

int network_close(int s);

int network_accept(int s, char *client_ip, int client_ip_len, int *pport);

int network_read(int s, void *buff, uint32_t leng);
int network_write(int s, const void *buff, uint32_t leng);

int network_accept_timeout(int s, uint32_t msec_timeout);
int network_read_timeout(int s, void *buff, uint32_t leng, uint32_t msec_timeout);
int network_write_timeout(int s, const void *buff, uint32_t leng, uint32_t msec_timeout);

int network_getstatus(int s);

//set opt
int network_nonblock(int s);
int network_tcp_reuseaddr(int s);
int network_tcp_nodelay(int s);
int network_tcp_nopush(int s);
int network_tcp_push(int s);

#endif
