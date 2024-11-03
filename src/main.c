#include "dns_server.h"
#include "dns_records.h"
#include "dns_parser.h"

int main()
{
    printf("Loading DNS mappings...\n");
    loadDNSMappings("dns_mappings.json");

    int udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket < 0)
    {
        perror("Socket creation error");
        return 1;
    }

    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    unsigned char buffer[BUFFER_SIZE];
    DNSHeader *reqHeader, resHeader;

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(DNS_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udpSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("Bind failed");
        close(udpSocket);
        return 1;
    }

    printf("DNS server started on port %d...\n", DNS_PORT);

    while (1)
    {
        int len = recvfrom(udpSocket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&clientAddr, &addrLen);
        if (len < 0)
        {
            perror("recvfrom error");
            continue;
        }

        reqHeader = (DNSHeader *)buffer;

        char domain[256];
        unsigned short queryType;
        char typeString[10];
        int query_len = 0;

        parseDNSQuery(buffer, domain, &queryType, typeString, &query_len);
        printf("Received query for domain: %s, type: %s\n", domain, typeString);

        memset(&resHeader, 0, sizeof(DNSHeader));
        resHeader.id = reqHeader->id;
        resHeader.qr = 1; // This is a response
        resHeader.opcode = reqHeader->opcode;
        resHeader.aa = 1; // Authoritative answer
        resHeader.tc = 0; // Not truncated
        resHeader.rd = reqHeader->rd; // Copy RD flag
        resHeader.ra = 0; // Recursion not available
        resHeader.z = 0; // Reserved
        resHeader.rcode = 0; // No error
        resHeader.qdcount = htons(1); // One question

        unsigned char response[BUFFER_SIZE];
        int response_len = 0;

        // Copy DNS header to response buffer
        memcpy(response, &resHeader, sizeof(DNSHeader));
        response_len += sizeof(DNSHeader);

        // Copy the question section from the request to the response
        memcpy(response + response_len, buffer + sizeof(DNSHeader), query_len);
        response_len += query_len;

        DNSRecord *record = resolveRecord(domain, typeString);
        if (!record)
        {
            resHeader.rcode = 3; // Name Error (NXDOMAIN)
            resHeader.ancount = htons(0);
            printf("Resolution failed for: %s\n", domain);

            // Update the header in the response buffer
            memcpy(response, &resHeader, sizeof(DNSHeader));

            // Send the response
            sendto(udpSocket, response, response_len, 0, (struct sockaddr *)&clientAddr, addrLen);
            continue;
        }
        else
        {
            resHeader.rcode = 0; // No error
            resHeader.ancount = htons(record->num_values);
            resHeader.nscount = htons(0);
            resHeader.arcount = htons(0);

            // Build the answer section
            for (int i = 0; i < record->num_values; i++)
            {
                // Answer Name: use pointer to the name in the question section
                unsigned char answer_name[2];
                answer_name[0] = 0xC0; // Name pointer
                answer_name[1] = sizeof(DNSHeader); // Offset to the domain name in the question section

                memcpy(response + response_len, answer_name, 2);
                response_len += 2;

                // Type and Class
                unsigned short ans_type;
                unsigned short ans_class = htons(1); // IN

                if (strcmp(typeString, "A") == 0)
                    ans_type = htons(1);
                else if (strcmp(typeString, "AAAA") == 0)
                    ans_type = htons(28);
                else if (strcmp(typeString, "CNAME") == 0)
                    ans_type = htons(5);
                else if (strcmp(typeString, "MX") == 0)
                    ans_type = htons(15);
                else if (strcmp(typeString, "NS") == 0)
                    ans_type = htons(2);
                else
                    ans_type = htons(queryType);

                memcpy(response + response_len, &ans_type, 2);
                response_len += 2;

                memcpy(response + response_len, &ans_class, 2);
                response_len += 2;

                // TTL
                unsigned int ttl = htonl(3600);
                memcpy(response + response_len, &ttl, 4);
                response_len += 4;

                // Now handle RDLENGTH and RDATA
                unsigned short rdlength;

                if (strcmp(typeString, "A") == 0)
                {
                    struct in_addr addr;
                    if (inet_aton(record->values[i], &addr) == 0)
                    {
                        printf("Invalid IP address: %s\n", record->values[i]);
                        continue;
                    }
                    rdlength = htons(4);
                    memcpy(response + response_len, &rdlength, 2);
                    response_len += 2;
                    memcpy(response + response_len, &addr.s_addr, 4);
                    response_len += 4;
                }
                else if (strcmp(typeString, "AAAA") == 0)
                {
                    struct in6_addr addr6;
                    if (inet_pton(AF_INET6, record->values[i], &addr6) != 1)
                    {
                        printf("Invalid IPv6 address: %s\n", record->values[i]);
                        continue;
                    }
                    rdlength = htons(16);
                    memcpy(response + response_len, &rdlength, 2);
                    response_len += 2;
                    memcpy(response + response_len, &addr6.s6_addr, 16);
                    response_len += 16;
                }
                else if (strcmp(typeString, "CNAME") == 0 || strcmp(typeString, "NS") == 0)
                {
                    // RDATA is the domain name in DNS format
                    unsigned char name[256];
                    int name_len = 0;
                    char *token;
                    char *name_copy = strdup(record->values[i]);
                    token = strtok(name_copy, ".");
                    while (token != NULL)
                    {
                        size_t label_len = strlen(token);
                        name[name_len++] = label_len;
                        memcpy(name + name_len, token, label_len);
                        name_len += label_len;
                        token = strtok(NULL, ".");
                    }
                    name[name_len++] = 0; // Null terminator
                    free(name_copy);

                    rdlength = htons(name_len);
                    memcpy(response + response_len, &rdlength, 2);
                    response_len += 2;
                    memcpy(response + response_len, name, name_len);
                    response_len += name_len;
                }
                else if (strcmp(typeString, "MX") == 0)
                {
                    unsigned short preference;
                    char exchange_str[256];

                    // Parse preference and exchange from record->values[i]
                    sscanf(record->values[i], "%hu %s", &preference, exchange_str);

                    preference = htons(preference);

                    unsigned char exchange[256];
                    int exchange_len = 0;
                    char *token;
                    char *exchange_copy = strdup(exchange_str);
                    token = strtok(exchange_copy, ".");
                    while (token != NULL)
                    {
                        size_t label_len = strlen(token);
                        exchange[exchange_len++] = label_len;
                        memcpy(exchange + exchange_len, token, label_len);
                        exchange_len += label_len;
                        token = strtok(NULL, ".");
                    }
                    exchange[exchange_len++] = 0; // Null terminator
                    free(exchange_copy);

                    rdlength = htons(2 + exchange_len);
                    memcpy(response + response_len, &rdlength, 2);
                    response_len += 2;
                    memcpy(response + response_len, &preference, 2);
                    response_len += 2;
                    memcpy(response + response_len, exchange, exchange_len);
                    response_len += exchange_len;
                }
                else
                {
                    printf("Unsupported query type: %s\n", typeString);
                    resHeader.rcode = 4; // Not Implemented
                    resHeader.ancount = htons(0);

                    memcpy(response, &resHeader, sizeof(DNSHeader));

                    sendto(udpSocket, response, response_len, 0, (struct sockaddr *)&clientAddr, addrLen);
                    break;
                }
            }

            memcpy(response, &resHeader, sizeof(DNSHeader));

            sendto(udpSocket, response, response_len, 0, (struct sockaddr *)&clientAddr, addrLen);
            printf("Resolved for: %s, type: %s\n", domain, typeString);
            printf("Response sent to: %s\n", inet_ntoa(clientAddr.sin_addr));
        }
    }

    close(udpSocket);
    return 0;
}
