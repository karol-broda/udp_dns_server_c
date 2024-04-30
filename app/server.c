#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

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
    char *name;
    unsigned short type;
    unsigned short class;
} DNSQuestion;

int encodeDomainName(char *domain, char *buffer)
{
    char *pos = buffer;
    char *part = domain;
    while (*part)
    {
        char *dot = strchr(part, '.');
        int length = (dot) ? dot - part : strlen(part);
        *pos++ = length;
        memcpy(pos, part, length);
        pos += length;
        part += length + (dot != NULL);
    }
    *pos++ = 0;
    printf("Encoded Domain: %s\n", buffer);
    return pos - buffer;
}

int main()
{
    int udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in serverAddr = {
        .sin_family = AF_INET,
        .sin_port = htons(DNS_PORT),
        .sin_addr.s_addr = INADDR_ANY,
    };

    if (bind(udpSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("Bind failed");
        close(udpSocket);
        return 1;
    }

    printf("Server is running and listening on port %d...\n", DNS_PORT);

    char buffer[BUFFER_SIZE];
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);

    while (1)
    {
        int len = recvfrom(udpSocket, buffer, sizeof(buffer), 0, (struct sockaddr *)&clientAddr, &addrLen);
        if (len < 0)
        {
            perror("recvfrom failed");
            continue;
        }

        printf("Received a packet from %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
        DNSHeader *reqHeader = (DNSHeader *)buffer;
        printf("Transaction ID: %u\n", ntohs(reqHeader->id));

        DNSHeader resHeader = *reqHeader;

        resHeader.qr = 1;
        resHeader.ancount = 0;
        resHeader.nscount = 0;
        resHeader.arcount = 0;
        resHeader.ra = 0;

        char *queryStart = buffer + sizeof(DNSHeader);
        DNSQuestion question;
        question.name = queryStart + 1;
        question.type = ntohs(*(unsigned short *)(queryStart + strlen(question.name) + 2));
        question.class = ntohs(*(unsigned short *)(queryStart + strlen(question.name) + 4));

        printf("Question Name: %s, Type: %d, Class: %d\n", question.name, question.type, question.class);
        printf("Parsed Question Name: %s\n", question.name);

        char response[BUFFER_SIZE];
        int headerSize = sizeof(DNSHeader);
        memcpy(response, &resHeader, headerSize);
        int querySize = encodeDomainName(question.name, response + headerSize);
        *(unsigned short *)(response + headerSize + querySize) = htons(question.type);
        *(unsigned short *)(response + headerSize + querySize + 2) = htons(question.class);

        int totalSize = headerSize + querySize + 4;
        printf("Sending response...\n");
        sendto(udpSocket, response, totalSize, 0, (struct sockaddr *)&clientAddr, addrLen);
        printf("Response sent.\n");
    }

    close(udpSocket);
    return 0;
}
