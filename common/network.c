/*
 * file   : test_ptrace.c
 * author : ning
 * date   : 2012-01-13 16:10:27
 */

#include "network.h"
#include "log.h"

static int _fill_sockaddr(struct sockaddr_in *sa, char *host, int port)
{
    memset(sa, 0, sizeof(struct sockaddr_in));
    sa->sin_family = AF_INET;
    /*sa.sin_addr.s_addr = INADDR_ANY; */
    sa->sin_addr.s_addr = inet_addr(host);
    sa->sin_port = htons(port);
    return 0;
}

int network_server_socket(char *host, int port)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        ERROR("can't create socket !!!");
        return -1;
    }
    network_nonblock(s);
    /*tcpnodelay(s); */
    network_tcp_reuseaddr(s);

    struct sockaddr_in listen_sa;
    _fill_sockaddr(&listen_sa, host, port);

    if (bind(s, (struct sockaddr *)&listen_sa, sizeof(struct sockaddr_in)) < 0) {
        ERROR("can't bind on %s:%d", host, port);
        return -1;
    }

    if (listen(s, 100) < 0) {
        ERROR("can't listen on %s:%d", host, port);
        return -1;
    }

    TRACE("listen on %s:%d", host, port);
    return s;
}

/*
* 注意: 可能返回一个正在连接状态的s
*/
int network_client_socket(char *host, int port)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);

    if (s < 0) {
        ERROR("can't create socket !!!");
        return -1;
    }
    network_nonblock(s);

    struct sockaddr_in sa;
    _fill_sockaddr(&sa, host, port);

    if (connect(s, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) >= 0) {
        DEBUG("connected to %s:%d immediately, got [fd:%d]", host, port, s);
        return s;
    }
    if (errno == EINPROGRESS) {
        DEBUG("connecting to %s:%d ... [fd:%d]", host, port, s);
        return s;
    }
    ERROR("connect to %s:%d [errno:%d(%s)]", host, port, errno, strerror(errno));
    return -1;
}

int network_close(int s)
{
    DEBUG("close(%d)", s);
    return close(s);
}

/*
 * 返回client_fd
 * client_ip
 * client_port
 * */
int network_accept(int s, char *client_ip, int client_ip_len, int *pport)
{
    int client_fd;
    struct sockaddr_in client_sa;
    socklen_t sa_len = sizeof(client_sa);

    client_fd = accept(s, (struct sockaddr *)&client_sa, &sa_len);
    if (client_fd < 0) {
        ERROR("accept error: %d: %s\n", errno, strerror(errno));
        return -1;
    }

    network_nonblock(client_fd);

    if (client_ip && client_ip_len) {
        strncpy(client_ip, inet_ntoa(client_sa.sin_addr), client_ip_len);
    }
    if (pport) {
        *pport = (int)ntohs(client_sa.sin_port);
    }
    return client_fd;
}

int32_t network_read(int s, void *buff, uint32_t len)
{
    uint32_t recvd = 0;
    int i;

    while (recvd < len) {
        i = read(s, ((uint8_t *) buff) + recvd, len - recvd);
        if (i <= 0) {
            /*DEBUG("[fd:%d] read return %d [errno:%d(%s)]", s, i, errno, strerror(errno));*/
            if (errno == EAGAIN) {  // we will return success
                break;
            }
            ERROR("oops, read from [fd:%d] failed: [errno:%d(%s)]", s, errno, strerror(errno));
            return -1;
        }
        recvd += i;
    }

    DEBUG("[fd:%d] %d bytes readed", s, recvd);
    return recvd;
}

int32_t network_write(int s, const void *buff, uint32_t len)
{
    uint32_t sent = 0;
    int i;

    while (sent < len) {
        i = write(s, ((const uint8_t *)buff) + sent, len - sent);
        if (i <= 0) {
            DEBUG("[fd:%d] write return %d [errno:%d(%s)]", s, i, errno, strerror(errno));
            return i;
        }
        sent += i;
    }
    DEBUG("[fd:%d] %d bytes sent", s, sent);
    return sent;
}

/* . */

int network_tcp_nopush(int s)
{
    int cork = 1;
    return setsockopt(s, IPPROTO_TCP, TCP_CORK, (const void *)&cork, sizeof(int));
}

int network_tcp_push(int s)
{
    int cork = 0;
    return setsockopt(s, IPPROTO_TCP, TCP_CORK, (const void *)&cork, sizeof(int));
}

int network_tcp_reuseaddr(int s)
{
    int yes = 1;
    return setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(int));
}

int network_tcp_nodelay(int s)
{
    int yes = 1;
    return setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *)&yes, sizeof(int));
}

int network_nonblock(int s)
{
    return fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
}
