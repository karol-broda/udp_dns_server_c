#ifndef DNS_PARSER_H
#define DNS_PARSER_H

#include "dns_server.h"

void parseDNSQuery(const unsigned char *buffer, char *domain, unsigned short *queryType, char *typeString, int *query_len);
#endif 