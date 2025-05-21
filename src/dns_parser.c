#include "dns_parser.h"

int parseDNSQuery(const unsigned char *buffer, size_t buffer_size, 
                 char *domain, size_t domain_size, 
                 unsigned short *queryType, char *typeString, size_t typeString_size, 
                 int *query_len)
{
    if (buffer == NULL || domain == NULL || queryType == NULL || 
        typeString == NULL || query_len == NULL) {
        return -1;
    }

    int i = 12;
    int j = 0;

    if (i >= buffer_size) {
        return -1;
    }

    while (i < buffer_size && buffer[i] != 0 && j < domain_size - 1)
    {
        int len = buffer[i];
        i++;
        
        if (len > 63) {
            return -1;
        }
        
        if (i + len > buffer_size) {
            return -1;
        }
        
        for (int k = 0; k < len && j < domain_size - 2; k++)
        {
            domain[j++] = buffer[i++];
        }
        
        if (j < domain_size - 1) {
            domain[j++] = '.';
        }
    }
    
    if (j > 0 && j < domain_size) {
        domain[--j] = '\0';
    } else {
        return -1;
    }

    i++;

    if (i + 4 > buffer_size) {
        return -1;
    }

    *queryType = (buffer[i] << 8) | buffer[i + 1];
    i += 2;

    unsigned short qclass = (buffer[i] << 8) | buffer[i + 1];
    i += 2;

    *query_len = i - 12;

    switch (*queryType)
    {
    case DNS_TYPE_A:
        snprintf(typeString, typeString_size, "A");
        break;
    case DNS_TYPE_AAAA:
        snprintf(typeString, typeString_size, "AAAA");
        break;
    case DNS_TYPE_NS:
        snprintf(typeString, typeString_size, "NS");
        break;
    case DNS_TYPE_CNAME:
        snprintf(typeString, typeString_size, "CNAME");
        break;
    case DNS_TYPE_MX:
        snprintf(typeString, typeString_size, "MX");
        break;
    case DNS_TYPE_TXT:
        snprintf(typeString, typeString_size, "TXT");
        break;
    case DNS_TYPE_SRV:
        snprintf(typeString, typeString_size, "SRV");
        break;
    default:
        snprintf(typeString, typeString_size, "TYPE%d", *queryType);
    }

    return 0;
}

int domainToDNSFormat(const char *domain, unsigned char *dns_format, size_t max_size)
{
    if (domain == NULL || dns_format == NULL || max_size < 2) {
        return -1;
    }

    int offset = 0;
    int label_start = 0;
    size_t len = strlen(domain);
    
    for (size_t i = 0; i <= len; i++) {
        if (i == len || domain[i] == '.') {
            int label_len = i - label_start;
            
            if (label_len <= 0 || label_len > 63) {
                return -1;
            }
            
            if (offset + 1 + label_len >= max_size) {
                return -1;
            }
            
            dns_format[offset++] = label_len;
            
            for (int j = 0; j < label_len; j++) {
                dns_format[offset++] = domain[label_start + j];
            }
            
            label_start = i + 1;
        }
    }
    
    if (offset < max_size) {
        dns_format[offset++] = 0;
        return offset;
    }
    
    return -1;
}

const char *getRecordTypeString(unsigned short type)
{
    switch (type) {
        case DNS_TYPE_A:     return "A";
        case DNS_TYPE_NS:    return "NS";
        case DNS_TYPE_CNAME: return "CNAME";
        case DNS_TYPE_MX:    return "MX";
        case DNS_TYPE_TXT:   return "TXT";
        case DNS_TYPE_AAAA:  return "AAAA";
        case DNS_TYPE_SRV:   return "SRV";
        default:             return "UNKNOWN";
    }
}