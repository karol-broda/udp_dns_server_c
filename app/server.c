#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "cJSON/cJSON.h"
#include "uthash/uthash.h"

#define DNS_PORT 2053
#define BUFFER_SIZE 1024

typedef struct
{
    unsigned short id;
    unsigned char rd : 1;
    unsigned char tc : 1;
    unsigned char aa : 1;
    unsigned char opcode : 4;
    unsigned char qr : 1;
    unsigned char rcode : 4;
    unsigned char cd : 1;
    unsigned char ad : 1;
    unsigned char z : 1;
    unsigned char ra : 1;
    unsigned short qdcount;
    unsigned short ancount;
    unsigned short nscount;
    unsigned short arcount;
} DNSHeader;

typedef struct
{
    unsigned short type;
    unsigned short class;
} DNSQuestionFooter;

typedef struct
{
    unsigned short type;
    unsigned short class;
    unsigned int ttl;
    unsigned short data_len;
} DNSRecordHeader;

typedef struct dns_record
{
    char *domain;
    char *type;
    char **values;
    int num_values;
    UT_hash_handle hh;
} DNSRecord;

DNSRecord *dns_records = NULL;

void add_record_to_hash(const char *domain, const char *type, cJSON *values)
{
    printf("Adding records for domain: %s, type: %s\n", domain, type);
    DNSRecord *record = malloc(sizeof(DNSRecord));
    if (!record)
    {
        perror("allocate memory dnsrecord");
        return;
    }

    record->domain = strdup(domain);
    record->type = strdup(type);

    if (strcmp(type, "MX") == 0)
    {
        record->num_values = cJSON_GetArraySize(values);
        record->values = malloc(record->num_values * sizeof(char *));
        for (int i = 0; i < record->num_values; i++)
        {
            cJSON *mx_entry = cJSON_GetArrayItem(values, i);
            cJSON *priority = cJSON_GetObjectItem(mx_entry, "priority");
            cJSON *value = cJSON_GetObjectItem(mx_entry, "value");
            if (priority && value && priority->type == cJSON_Number && value->type == cJSON_String)
            {
                char mx_record[256];
                snprintf(mx_record, sizeof(mx_record), "%d %s", priority->valueint, value->valuestring);
                record->values[i] = strdup(mx_record);
                printf("added MX record for %s: %s\n", domain, mx_record);
            }
        }
    }
    else
    {
        record->num_values = cJSON_GetArraySize(values);
        record->values = malloc(record->num_values * sizeof(char *));
        for (int i = 0; i < record->num_values; i++)
        {
            cJSON *item = cJSON_GetArrayItem(values, i);
            if (item->valuestring)
            {
                record->values[i] = strdup(item->valuestring);
                printf("added value: %s\n", record->values[i]);
            }
        }
    }

    HASH_ADD_KEYPTR(hh, dns_records, record->domain, strlen(record->domain), record);
    printf("added %s record for %s with %d values\n", type, domain, record->num_values);
}

void loadDNSMappings(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        perror("file connot be opened");
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *data = malloc(length + 1);
    if (!data)
    {
        perror("memory allocation er");
        fclose(file);
        exit(1);
    }

    fread(data, 1, length, file);
    fclose(file);
    data[length] = '\0';

    cJSON *json = cJSON_Parse(data);
    if (!json)
    {
        fprintf(stderr, "error: [%s]\n", cJSON_GetErrorPtr());
        free(data);
        exit(1);
    }

    cJSON *domain = NULL;
    cJSON *types = NULL;
    cJSON *type = NULL;
    cJSON *values = NULL;
    const char *domainName = NULL;
    const char *typeName = NULL;

    cJSON_ArrayForEach(domain, json)
    {
        domainName = domain->string;
        types = cJSON_GetObjectItemCaseSensitive(json, domainName);
        if (!types)
        {
            printf("no types found for domain: %s\n", domainName);
            continue;
        }

        cJSON_ArrayForEach(type, types)
        {
            typeName = type->string;
            values = cJSON_GetObjectItemCaseSensitive(types, typeName);
            if (values)
            {
                add_record_to_hash(domainName, typeName, values);
            }
            else
            {
                printf("not found for type: %s in domain: %s\n", typeName, domainName);
            }
        }
    }

    free(data);
    cJSON_Delete(json);
}

const char *resolveRecord(const char *domain, const char *type)
{
    DNSRecord *record = NULL;
    HASH_FIND_STR(dns_records, domain, record);
    if (record)
    {
        printf("record found for domain %s\n", domain);
        while (record != NULL)
        {
            printf("check type: %s against query type: %s\n", record->type, type);
            if (strcmp(record->type, type) == 0 && record->num_values > 0)
            {
                printf("match record found: %s\n", record->values[0]);
                return record->values[0];
            }
            record = record->hh.next;
        }
    }
    printf("no match found for domain %s and type %s\n", domain, type);
    return NULL;
}

void parseDNSQuery(const unsigned char *buffer, char *domain, unsigned short *queryType, char *typeString)
{
    int i = 12;
    int j = 0;

    while (buffer[i])
    {
        int len = buffer[i++];
        for (int k = 0; k < len; k++)
        {
            domain[j++] = buffer[i++];
        }
        domain[j++] = '.';
    }
    domain[--j] = '\0';

    i++;
    *queryType = (buffer[i] << 8) | buffer[i + 1];
    printf("parsed domain: %s, type: %d\n", domain, *queryType);

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

void printDNSRecords()
{
    DNSRecord *record, *tmp;
    HASH_ITER(hh, dns_records, record, tmp)
    {
        printf("domain: %s, type: %s\n", record->domain, record->type);
        for (int i = 0; i < record->num_values; i++)
        {
            printf("val %d: %s\n", i + 1, record->values[i]);
        }
        printf("\n");
    }
}

int main()
{
    printf("load JSON\n");
    loadDNSMappings("app/dns_mappings.json");
    printDNSRecords();

    int udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    char buffer[BUFFER_SIZE];
    DNSHeader *reqHeader, resHeader;

    if (udpSocket < 0)
    {
        perror("socket creation err");
        return 1;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(DNS_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udpSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("bind fail");
        close(udpSocket);
        return 1;
    }

    printf("dns server started on port %d...\n", DNS_PORT);

    while (1)
    {
        int len = recvfrom(udpSocket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&clientAddr, &addrLen);
        if (len < 0)
        {
            perror("recvfrom err");
            continue;
        }

        reqHeader = (DNSHeader *)buffer;
        memcpy(&resHeader, reqHeader, sizeof(DNSHeader));
        resHeader.qr = 1;
        resHeader.aa = 1;
        resHeader.ra = 0;
        resHeader.rcode = 0;

        char domain[256];
        unsigned short queryType;
        char typeString[10];

        parseDNSQuery((const unsigned char *)buffer, domain, &queryType, typeString);
        printf("domain: %s\n", domain);

        char type[10];
        sprintf(type, "%d", queryType);

        const char *resolvedData = resolveRecord(domain, typeString);
        if (!resolvedData)
        {
            resHeader.rcode = 3;
            resHeader.ancount = 0;
            printf("resolution failed for: %s\n", domain);
        }
        else
        {
            DNSRecordHeader recordHeader;
            recordHeader.type = htons(1);
            recordHeader.class = htons(1);
            recordHeader.ttl = htonl(3600);
            recordHeader.data_len = htons(4);

            int headerSize = sizeof(DNSHeader);
            int questionSize = sizeof(DNSHeader) + strlen(domain) + 1 + sizeof(DNSQuestionFooter);
            int recordSize = sizeof(DNSRecordHeader) + ntohs(recordHeader.data_len);
            char response[headerSize + questionSize + recordSize];
            memcpy(response, &resHeader, headerSize);
            memcpy(response + headerSize, buffer + sizeof(DNSHeader), questionSize - sizeof(DNSHeader));
            memcpy(response + headerSize + questionSize - sizeof(DNSHeader), &recordHeader, sizeof(DNSRecordHeader));
            memcpy(response + headerSize + questionSize - sizeof(DNSHeader) + sizeof(DNSRecordHeader), resolvedData, ntohs(recordHeader.data_len));

            sendto(udpSocket, response, sizeof(response), 0, (struct sockaddr *)&clientAddr, addrLen);
            printf("resolved for: %s, IP: %s\n", domain, resolvedData);
            printf("response sent to: %s\n", inet_ntoa(clientAddr.sin_addr));
        }
    }

    close(udpSocket);
    return 0;
}
