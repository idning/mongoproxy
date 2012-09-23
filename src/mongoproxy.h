
typedef struct mongo_proxy_cfg_s {
    char *host;
    int port;
    int is_master;
    int last_ping;
} mongo_proxy_cfg_t;
