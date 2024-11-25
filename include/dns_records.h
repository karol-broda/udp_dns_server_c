#ifndef DNS_RECORDS_H
#define DNS_RECORDS_H

#include "dns_server.h"

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

void add_record_to_hash(const char *domain, const char *type, cJSON *values, const char *scope);
void loadDNSMappings(const char *filename);
DNSRecord *resolveRecord(const char *domain, const char *type);

#endif