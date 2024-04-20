#ifndef redis_server_h
#define redis_server_h

#include "utils.h"


#define REPLICATION_ID_LEN 40
#define MAX_REPLICAS_COUNT 256

struct ServerMetadata {
    bool is_replica;
    char replication_id[REPLICATION_ID_LEN + 1];
    int replication_offset;
    char* port;
    int replicas_fd[MAX_REPLICAS_COUNT]; // Hold the socket fd of each replica
    int replica_count;
};

struct MasterMetadata {
    char* master_host;
    int master_port;
    int master_fd;
    char master_replication_id[REPLICATION_ID_LEN + 1];

};
#endif