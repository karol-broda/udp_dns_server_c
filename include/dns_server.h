#ifndef DNS_SERVER_H
#define DNS_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h> 
#include <ctype.h>

#include "cJSON.h"
#include "uthash.h"

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} LogLevel;

typedef struct {
    int dns_port;
    int mgmt_port;
    char *mappings_file;
    int default_ttl;
    char *auth_token;
    int verbose;
} DNSServerConfig;

extern DNSServerConfig config;
extern volatile sig_atomic_t running;

#define DEFAULT_DNS_PORT 2053
#define DEFAULT_MGMT_PORT 8053
#define DEFAULT_MAPPINGS_FILE "dns_mappings.json"
#define DEFAULT_TTL 3600
#define DEFAULT_AUTH_TOKEN "123456"
#define DEFAULT_BUFFER_SIZE 512

typedef struct
{
    unsigned short id;
    unsigned char rd : 1;
    unsigned char tc : 1;
    unsigned char aa : 1;
    unsigned char opcode : 4;
    unsigned char qr : 1;
    unsigned char rcode : 4;
    unsigned char z : 3;
    unsigned char ra : 1;
    unsigned short qdcount;
    unsigned short ancount;
    unsigned short nscount;
    unsigned short arcount;
} __attribute__((packed)) DNSHeader;

#define DNS_TYPE_A     1
#define DNS_TYPE_NS    2
#define DNS_TYPE_CNAME 5
#define DNS_TYPE_MX    15
#define DNS_TYPE_TXT   16
#define DNS_TYPE_AAAA  28
#define DNS_TYPE_SRV   33

#define DNS_RCODE_NOERROR   0
#define DNS_RCODE_FORMERR   1
#define DNS_RCODE_SERVFAIL  2
#define DNS_RCODE_NXDOMAIN  3
#define DNS_RCODE_NOTIMP    4
#define DNS_RCODE_REFUSED   5

void init_config(void);
int init_dns_server(void);
void process_dns_query(int sock_fd, unsigned char *buffer, int len, 
                      struct sockaddr_in *client_addr, socklen_t addr_len);
void *management_thread(void *arg);
void log_message(LogLevel level, const char *format, ...);

void handle_add_command(int client_fd, char *input);
void handle_delete_command(int client_fd, char *input);
void handle_list_command(int client_fd);
void handle_reload_command(int client_fd);

#endif