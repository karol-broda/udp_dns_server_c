#include "dns_server.h"
#include "dns_records.h"
#include "dns_parser.h"

void handle_signal(int sig) {
    log_message(LOG_INFO, "Received signal %d, shutting down...", sig);
    running = 0;
}

int init_dns_server() {
    int udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket < 0) {
        log_message(LOG_ERROR, "Socket creation error: %s", strerror(errno));
        return -1;
    }
    
    int opt = 1;
    if (setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_message(LOG_ERROR, "setsockopt error: %s", strerror(errno));
        close(udpSocket);
        return -1;
    }
    
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(config.dns_port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(udpSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        log_message(LOG_ERROR, "Bind failed: %s", strerror(errno));
        close(udpSocket);
        return -1;
    }
    
    int flags = fcntl(udpSocket, F_GETFL, 0);
    if (flags == -1) {
        log_message(LOG_ERROR, "fcntl error: %s", strerror(errno));
        close(udpSocket);
        return -1;
    }
    
    if (fcntl(udpSocket, F_SETFL, flags | O_NONBLOCK) == -1) {
        log_message(LOG_ERROR, "fcntl error: %s", strerror(errno));
        close(udpSocket);
        return -1;
    }
    
    log_message(LOG_INFO, "DNS server started on port %d", config.dns_port);
    return udpSocket;
}

void process_dns_query(int udpSocket, unsigned char *buffer, int len, 
                     struct sockaddr_in *clientAddr, socklen_t addrLen) {
    
    DNSHeader *reqHeader = (DNSHeader *)buffer;
    
    char domain[256];
    unsigned short queryType;
    char typeString[16];
    int query_len = 0;
    
    if (parseDNSQuery(buffer, len, domain, sizeof(domain), &queryType, 
                    typeString, sizeof(typeString), &query_len) != 0) {
        log_message(LOG_ERROR, "Failed to parse DNS query");
        return;
    }
    
    log_message(LOG_INFO, "Received query for domain: %s, type: %s", domain, typeString);
    
    DNSHeader resHeader;
    memset(&resHeader, 0, sizeof(DNSHeader));
    resHeader.id = reqHeader->id;
    resHeader.qr = 1;
    resHeader.opcode = reqHeader->opcode;
    resHeader.aa = 1;
    resHeader.tc = 0;
    resHeader.rd = reqHeader->rd;
    resHeader.ra = 0;
    resHeader.z = 0;
    resHeader.rcode = 0;
    resHeader.qdcount = htons(1);
    
    unsigned char response[DEFAULT_BUFFER_SIZE];
    int response_len = 0;
    
    memcpy(response, &resHeader, sizeof(DNSHeader));
    response_len += sizeof(DNSHeader);
    
    if (response_len + query_len > sizeof(response)) {
        log_message(LOG_ERROR, "Response buffer too small");
        return;
    }
    
    memcpy(response + response_len, buffer + sizeof(DNSHeader), query_len);
    response_len += query_len;
    pthread_mutex_lock(&dns_records_mutex);
    
    DNSRecord *record = resolveRecord(domain, typeString);
    
    if (record == NULL) {
        resHeader.rcode = DNS_RCODE_NXDOMAIN;
        resHeader.ancount = htons(0);
        log_message(LOG_INFO, "Resolution failed for: %s", domain);
        
        memcpy(response, &resHeader, sizeof(DNSHeader));
        
        sendto(udpSocket, response, response_len, 0, 
              (struct sockaddr *)clientAddr, addrLen);
        
        pthread_mutex_unlock(&dns_records_mutex);
        return;
    }
    
    resHeader.rcode = DNS_RCODE_NOERROR;
    resHeader.ancount = htons(record->num_values);
    resHeader.nscount = htons(0);
    resHeader.arcount = htons(0);
    
    for (int i = 0; i < record->num_values; i++) {
        if (response_len + 16 > sizeof(response)) {
            log_message(LOG_ERROR, "Response buffer too small for answer");
            break;
        }
        
        unsigned char answer_name[2];
        answer_name[0] = 0xC0;
        answer_name[1] = sizeof(DNSHeader);
        
        memcpy(response + response_len, answer_name, 2);
        response_len += 2;
        
        unsigned short ans_type;
        unsigned short ans_class = htons(1);
        
        if (strcmp(typeString, "A") == 0)
            ans_type = htons(DNS_TYPE_A);
        else if (strcmp(typeString, "AAAA") == 0)
            ans_type = htons(DNS_TYPE_AAAA);
        else if (strcmp(typeString, "CNAME") == 0)
            ans_type = htons(DNS_TYPE_CNAME);
        else if (strcmp(typeString, "MX") == 0)
            ans_type = htons(DNS_TYPE_MX);
        else if (strcmp(typeString, "NS") == 0)
            ans_type = htons(DNS_TYPE_NS);
        else if (strcmp(typeString, "TXT") == 0)
            ans_type = htons(DNS_TYPE_TXT);
        else if (strcmp(typeString, "SRV") == 0)
            ans_type = htons(DNS_TYPE_SRV);
        else
            ans_type = htons(queryType);
        
        memcpy(response + response_len, &ans_type, 2);
        response_len += 2;
        
        memcpy(response + response_len, &ans_class, 2);
        response_len += 2;
        
        unsigned int ttl = htonl(config.default_ttl);
        memcpy(response + response_len, &ttl, 4);
        response_len += 4;
        
        unsigned short rdlength;
        
        if (strcmp(typeString, "A") == 0) {
            struct in_addr addr;
            if (inet_aton(record->values[i], &addr) == 0) {
                log_message(LOG_ERROR, "Invalid IP address: %s", record->values[i]);
                continue;
            }
            
            if (response_len + 6 > sizeof(response)) {
                log_message(LOG_ERROR, "Response buffer too small for A record");
                break;
            }
            
            rdlength = htons(4);
            memcpy(response + response_len, &rdlength, 2);
            response_len += 2;

            memcpy(response + response_len, &addr.s_addr, 4);
            response_len += 4;
        } 
        else if (strcmp(typeString, "AAAA") == 0) {
            struct in6_addr addr6;
            if (inet_pton(AF_INET6, record->values[i], &addr6) != 1) {
                log_message(LOG_ERROR, "Invalid IPv6 address: %s", record->values[i]);
                continue;
            }
            
            if (response_len + 18 > sizeof(response)) {
                log_message(LOG_ERROR, "Response buffer too small for AAAA record");
                break;
            }
            
            rdlength = htons(16);
            memcpy(response + response_len, &rdlength, 2);
            response_len += 2;
            
            memcpy(response + response_len, &addr6.s6_addr, 16);
            response_len += 16;
        } 
        else if (strcmp(typeString, "CNAME") == 0 || strcmp(typeString, "NS") == 0) {
            unsigned char dns_format[256];
            int dns_len = domainToDNSFormat(record->values[i], dns_format, sizeof(dns_format));
            
            if (dns_len < 0) {
                log_message(LOG_ERROR, "Failed to encode domain name: %s", record->values[i]);
                continue;
            }
            
            if (response_len + 2 + dns_len > sizeof(response)) {
                log_message(LOG_ERROR, "Response buffer too small for domain name");
                break;
            }
            
            rdlength = htons(dns_len);
            memcpy(response + response_len, &rdlength, 2);
            response_len += 2;
            
            memcpy(response + response_len, dns_format, dns_len);
            response_len += dns_len;
        } 
        else if (strcmp(typeString, "MX") == 0) {
            unsigned short preference;
            char exchange_str[256];
            
            if (sscanf(record->values[i], "%hu %255s", &preference, exchange_str) != 2) {
                log_message(LOG_ERROR, "Invalid MX record format: %s", record->values[i]);
                continue;
            }
            
            preference = htons(preference);
            
            unsigned char exchange_dns[256];
            int exchange_len = domainToDNSFormat(exchange_str, exchange_dns, sizeof(exchange_dns));
            
            if (exchange_len < 0) {
                log_message(LOG_ERROR, "Failed to encode MX exchange: %s", exchange_str);
                continue;
            }
            
            if (response_len + 4 + exchange_len > sizeof(response)) {
                log_message(LOG_ERROR, "Response buffer too small for MX record");
                break;
            }
            
            rdlength = htons(2 + exchange_len);
            memcpy(response + response_len, &rdlength, 2);
            response_len += 2;
            
            memcpy(response + response_len, &preference, 2);
            response_len += 2;
            
            memcpy(response + response_len, exchange_dns, exchange_len);
            response_len += exchange_len;
        }
        else {
            log_message(LOG_ERROR, "Unsupported query type: %s", typeString);
            resHeader.rcode = DNS_RCODE_NOTIMP;
            resHeader.ancount = htons(0);
            
            memcpy(response, &resHeader, sizeof(DNSHeader));
            
            sendto(udpSocket, response, response_len, 0, 
                  (struct sockaddr *)clientAddr, addrLen);
            
            pthread_mutex_unlock(&dns_records_mutex);
            return;
        }
    }
    
    pthread_mutex_unlock(&dns_records_mutex);
    
    memcpy(response, &resHeader, sizeof(DNSHeader));
    
    sendto(udpSocket, response, response_len, 0, 
          (struct sockaddr *)clientAddr, addrLen);
    
    log_message(LOG_INFO, "Resolved for: %s, type: %s", domain, typeString);
    log_message(LOG_INFO, "Response sent to: %s", inet_ntoa(clientAddr->sin_addr));
}

int main(int argc, char *argv[]) {
    init_config();
    
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    log_message(LOG_INFO, "Starting DNS server...");
    log_message(LOG_INFO, "Loading DNS mappings from %s", config.mappings_file);
    
    init_dns_records();
    
    if (loadDNSMappings(config.mappings_file) != 0) {
        log_message(LOG_ERROR, "Failed to load DNS mappings");
        cleanup_dns_records();
        return 1;
    }
    
    int udpSocket = init_dns_server();
    if (udpSocket < 0) {
        log_message(LOG_ERROR, "Failed to initialize DNS server");
        cleanup_dns_records();
        return 1;
    }
    
    pthread_t mgmt_thread_id;
    if (pthread_create(&mgmt_thread_id, NULL, management_thread, NULL) != 0) {
        log_message(LOG_ERROR, "Failed to create management thread: %s", strerror(errno));
        close(udpSocket);
        cleanup_dns_records();
        return 1;
    }
    
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    unsigned char buffer[DEFAULT_BUFFER_SIZE];
    
    log_message(LOG_INFO, "DNS server running on port %d", config.dns_port);
    
    while (running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(udpSocket, &readfds);
        
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int activity = select(udpSocket + 1, &readfds, NULL, NULL, &tv);
        
        if (activity < 0 && errno != EINTR) {
            log_message(LOG_ERROR, "Select error: %s", strerror(errno));
            continue;
        }
        
        if (activity == 0) {
            continue;
        }
        
        if (FD_ISSET(udpSocket, &readfds)) {
            memset(buffer, 0, sizeof(buffer));
            int len = recvfrom(udpSocket, buffer, sizeof(buffer), 0, 
                            (struct sockaddr *)&clientAddr, &addrLen);
            
            if (len < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    log_message(LOG_ERROR, "recvfrom error: %s", strerror(errno));
                }
                continue;
            }
            
            if (len < sizeof(DNSHeader) + 5) {
                log_message(LOG_WARNING, "Received packet too small to be a valid DNS query");
                continue;
            }
            
            process_dns_query(udpSocket, buffer, len, &clientAddr, addrLen);
        }
    }
    
    close(udpSocket);
    pthread_join(mgmt_thread_id, NULL);
    cleanup_dns_records();
    
    free(config.mappings_file);
    free(config.auth_token);
    
    log_message(LOG_INFO, "DNS server shutdown complete");
    return 0;
}