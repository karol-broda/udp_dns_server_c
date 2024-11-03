#include "dns_parser.h"

void parseDNSQuery(const unsigned char *buffer, char *domain, unsigned short *queryType, char *typeString, int *query_len)
{
    int i = 12; 
    int j = 0;

    while (buffer[i] != 0)
    {
        int len = buffer[i];
        i++;
        for (int k = 0; k < len; k++)
        {
            domain[j++] = buffer[i++];
        }
        domain[j++] = '.';
    }
    domain[--j] = '\0'; 

    i++; 

    *queryType = (buffer[i] << 8) | buffer[i + 1];
    i += 2; 

    unsigned short qclass = (buffer[i] << 8) | buffer[i + 1];
    i += 2;

    *query_len = i - 12;

    switch (*queryType)
    {
    case 1:
        strcpy(typeString, "A");
        break;
    case 28:
        strcpy(typeString, "AAAA");
        break;
    case 2:
        strcpy(typeString, "NS");
        break;
    case 5:
        strcpy(typeString, "CNAME");
        break;
    case 15:
        strcpy(typeString, "MX");
        break;
    default:
        sprintf(typeString, "TYPE%d", *queryType);
    }
}
