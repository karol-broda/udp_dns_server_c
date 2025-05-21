#ifndef DNS_PARSER_H
#define DNS_PARSER_H

#include "dns_server.h"

int parseDNSQuery(const unsigned char *buffer, size_t buffer_size, 
                 char *domain, size_t domain_size, 
                 unsigned short *queryType, char *typeString, size_t typeString_size, 
                 int *query_len);

int domainToDNSFormat(const char *domain, unsigned char *dns_format, size_t max_size);

const char *getRecordTypeString(unsigned short type);

#endif