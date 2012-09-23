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

    NOTICE("listen on %s:%p", host, port);
    return s;
}

/*
* 注意: 可能返回一个正在连接状态的s
*/
int network_client_socket(char *host, int port)
{
    int status;

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        ERROR("can't create socket !!!");
        return -1;
    }
    network_nonblock(s);

    struct sockaddr_in sa;
    _fill_sockaddr(&sa, host, port);

    if (connect(s, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) >= 0) {
        DEBUG("connected to %s:%d immediately", host, port);
        return s;
    }
    if (errno == EINPROGRESS) {
        DEBUG("connecting to %s:%d ... ", host, port);
        return s;
    }
    ERROR("connect to %s:%d [errno:]", host, port, errno);
    return -1;
}

int network_close(int s)
{
    DEBUG("close(%d)", s);
    close(s);
}

int network_accept(int s)
{
    int sock;

    sock = accept(s, (struct sockaddr *)NULL, 0);   //TODO: log client ip
    if (s < 0) {
        ERROR("accept error: %s\n", strerror(errno));
        return -1;
    }

    return sock;
}

int32_t network_read(int s, void *buff, uint32_t len)
{
    uint32_t recvd = 0;
    int i;

    while (recvd < len) {
        i = read(s, ((uint8_t *) buff) + recvd, len - recvd);
        if (i <= 0) {
            return i;
        }
        recvd += i;
    }

    return recvd;
}

int32_t network_write(int s, const void *buff, uint32_t len)
{
    uint32_t sent = 0;
    int i;

    while (sent < len) {
        i = write(s, ((const uint8_t *)buff) + sent, len - sent);
        if (i <= 0) {
            return i;
        }
        sent += i;
    }

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
