#ifndef DNS_RECORDS_H
#define DNS_RECORDS_H

#include "dns_server.h"
#include <pthread.h>

typedef struct dns_record
{
    char *domain;
    char *type;
    char *key;
    char **values;
    int num_values;
    UT_hash_handle hh;
} DNSRecord;

extern DNSRecord *dns_records;
extern pthread_mutex_t dns_records_mutex;

void init_dns_records(void);

void add_record_to_hash(const char *domain, const char *type, cJSON *values, const char *scope);

int loadDNSMappings(const char *filename);

DNSRecord *resolveRecord(const char *domain, const char *type);

void cleanup_dns_records(void);

int add_single_record(const char *domain, const char *type, const char *scope, const char *value);

int delete_record(const char *domain, const char *type, const char *scope);

char *get_records_list(void);

#endif