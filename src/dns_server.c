#include "dns_records.h"
#include <stdarg.h>

DNSRecord *dns_records = NULL;
pthread_mutex_t dns_records_mutex = PTHREAD_MUTEX_INITIALIZER;
DNSServerConfig config;
volatile sig_atomic_t running = 1;

void init_config(void)
{
    config.dns_port = DEFAULT_DNS_PORT;
    config.mgmt_port = DEFAULT_MGMT_PORT;
    config.mappings_file = strdup(DEFAULT_MAPPINGS_FILE);
    config.default_ttl = DEFAULT_TTL;
    config.auth_token = strdup(DEFAULT_AUTH_TOKEN);
    config.verbose = 0;
}

void init_dns_records(void)
{
    pthread_mutex_init(&dns_records_mutex, NULL);
    dns_records = NULL;
}

void cleanup_dns_records(void)
{
    pthread_mutex_lock(&dns_records_mutex);
    
    DNSRecord *current, *tmp;
    HASH_ITER(hh, dns_records, current, tmp) {
        HASH_DEL(dns_records, current);
        
        if (current->domain) free(current->domain);
        if (current->type) free(current->type);
        if (current->key) free(current->key);
        
        if (current->values) {
            for (int i = 0; i < current->num_values; i++) {
                if (current->values[i]) free(current->values[i]);
            }
            free(current->values);
        }
        
        free(current);
    }
    
    pthread_mutex_unlock(&dns_records_mutex);
    pthread_mutex_destroy(&dns_records_mutex);
}

void free_dns_record(DNSRecord *record) {
    if (record == NULL) {
        return;
    }
    
    for (int i = 0; i < record->num_values; i++) {
        if (record->values != NULL && record->values[i] != NULL) {
            free(record->values[i]);
        }
    }
    
    if (record->values != NULL) free(record->values);
    if (record->key != NULL) free(record->key);
    if (record->type != NULL) free(record->type);
    if (record->domain != NULL) free(record->domain);
    free(record);
}

DNSRecord* create_dns_record(const char *domain, const char *type, const char *scope) {
    if (domain == NULL || type == NULL || scope == NULL) {
        log_message(LOG_ERROR, "create_dns_record: null parameters provided");
        return NULL;
    }

    DNSRecord *record = (DNSRecord *)malloc(sizeof(DNSRecord));
    if (record == NULL) {
        log_message(LOG_ERROR, "failed to allocate memory for dns record: %s", strerror(errno));
        return NULL;
    }
    
    record->domain = NULL;
    record->type = NULL;
    record->key = NULL;
    record->values = NULL;
    record->num_values = 0;
    
    record->domain = strdup(domain);
    if (record->domain == NULL) {
        log_message(LOG_ERROR, "failed to duplicate domain: %s", strerror(errno));
        free_dns_record(record);
        return NULL;
    }
    
    record->type = strdup(type);
    if (record->type == NULL) {
        log_message(LOG_ERROR, "failed to duplicate type: %s", strerror(errno));
        free_dns_record(record);
        return NULL;
    }
    
    if (asprintf(&(record->key), "%s_%s_%s", scope, domain, type) == -1) {
        log_message(LOG_ERROR, "failed to create key: %s", strerror(errno));
        free_dns_record(record);
        return NULL;
    }
    
    return record;
}

int parse_values_to_record(DNSRecord *record, cJSON *values) {
    if (record == NULL || values == NULL) {
        log_message(LOG_ERROR, "parse_values_to_record: null parameters provided");
        return 0;
    }

    record->num_values = cJSON_GetArraySize(values);
    if (record->num_values <= 0) {
        log_message(LOG_ERROR, "no values provided for %s %s", record->domain, record->type);
        return 0;
    }
    
    record->values = (char **)malloc(record->num_values * sizeof(char *));
    if (record->values == NULL) {
        log_message(LOG_ERROR, "failed to allocate memory for values: %s", strerror(errno));
        return 0;
    }
    
    for (int i = 0; i < record->num_values; i++) {
        record->values[i] = NULL;
    }
    
    for (int i = 0; i < record->num_values; i++) {
        cJSON *item = cJSON_GetArrayItem(values, i);
        if (item == NULL) {
            log_message(LOG_ERROR, "null array item at index %d", i);
            return 0;
        }
        
        if (strcmp(record->type, "MX") == 0 && cJSON_IsObject(item)) {
            cJSON *priority = cJSON_GetObjectItem(item, "priority");
            cJSON *value = cJSON_GetObjectItem(item, "value");
            
            if (priority != NULL && value != NULL && 
                cJSON_IsNumber(priority) && cJSON_IsString(value)) {
                char mx_record[256];
                snprintf(mx_record, sizeof(mx_record), "%d %s", priority->valueint, value->valuestring);
                record->values[i] = strdup(mx_record);
            } else {
                log_message(LOG_ERROR, "invalid mx record format");
                return 0;
            }
        } else if (cJSON_IsString(item)) {
            record->values[i] = strdup(item->valuestring);
        } else {
            log_message(LOG_ERROR, "invalid record value format at index %d", i);
            return 0;
        }
        
        if (record->values[i] == NULL) {
            log_message(LOG_ERROR, "failed to duplicate value: %s", strerror(errno));
            return 0;
        }
    }
    
    return 1;
}

void add_record_to_hash(const char *domain, const char *type, cJSON *values, const char *scope) {
    if (domain == NULL || type == NULL || values == NULL || scope == NULL) {
        log_message(LOG_ERROR, "add_record_to_hash: invalid parameters");
        return;
    }
    
    log_message(LOG_INFO, "adding records for domain: %s, type: %s", domain, type);
    
    DNSRecord *record = create_dns_record(domain, type, scope);
    if (record == NULL) {
        return; 
    }
    
    if (parse_values_to_record(record, values) == 0) {
        free_dns_record(record);
        return;
    }
    
    DNSRecord *existing_record = NULL;
    HASH_FIND_STR(dns_records, record->key, existing_record);
    
    if (existing_record != NULL) {
        HASH_DEL(dns_records, existing_record);
        free_dns_record(existing_record);
    }
    
    HASH_ADD_KEYPTR(hh, dns_records, record->key, strlen(record->key), record);
    log_message(LOG_INFO, "added %s record for %s with %d values", type, domain, record->num_values);
}

int loadDNSMappings(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        log_message(LOG_ERROR, "Failed to open DNS mappings file %s: %s", 
                  filename, strerror(errno));
        return -1;
    }
    
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (length <= 0) {
        log_message(LOG_ERROR, "Empty or invalid mappings file: %s", filename);
        fclose(file);
        return -1;
    }
    
    char *data = (char *)malloc(length + 1);
    if (data == NULL) {
        log_message(LOG_ERROR, "Memory allocation error: %s", strerror(errno));
        fclose(file);
        return -1;
    }
    
    size_t bytes_read = fread(data, 1, length, file);
    fclose(file);
    
    if (bytes_read != length) {
        log_message(LOG_ERROR, "Failed to read entire file: %s", strerror(errno));
        free(data);
        return -1;
    }
    
    data[length] = '\0';
    
    cJSON *json = cJSON_Parse(data);
    if (json == NULL) {
        log_message(LOG_ERROR, "JSON parsing error: %s", 
                  cJSON_GetErrorPtr() ? cJSON_GetErrorPtr() : "unknown error");
        free(data);
        return -1;
    }
    
    cJSON *domains = cJSON_GetObjectItem(json, "domains");
    if (domains == NULL || !cJSON_IsObject(domains)) {
        log_message(LOG_ERROR, "Invalid schema: 'domains' key missing or not an object");
        free(data);
        cJSON_Delete(json);
        return -1;
    }
    
    pthread_mutex_lock(&dns_records_mutex);
    
    cJSON *domain = NULL;
    cJSON_ArrayForEach(domain, domains) {
        const char *domainName = domain->string;
        
        cJSON *records = cJSON_GetObjectItem(domain, "records");
        if (records != NULL) {
            int has_cname = 0;
            int has_other_records = 0;
            
            cJSON *type = NULL;
            cJSON_ArrayForEach(type, records) {
                const char *typeName = type->string;
                cJSON *values = cJSON_GetObjectItem(records, typeName);
                
                if (values != NULL) {
                    if (strcmp(typeName, "CNAME") == 0) {
                        has_cname = 1;
                    } else {
                        has_other_records = 1;
                    }
                    
                    add_record_to_hash(domainName, typeName, values, "base");
                }
            }
            
            if (has_cname && has_other_records) {
                log_message(LOG_ERROR, "Configuration error: %s has CNAME and other records simultaneously", 
                          domainName);
                pthread_mutex_unlock(&dns_records_mutex);
                free(data);
                cJSON_Delete(json);
                return -1;
            }
        }
        
        cJSON *wildcards = cJSON_GetObjectItem(domain, "wildcards");
        if (wildcards != NULL) {
            cJSON *wildcard_records = cJSON_GetObjectItem(wildcards, "records");
            
            if (wildcard_records != NULL) {
                cJSON *type = NULL;
                cJSON_ArrayForEach(type, wildcard_records) {
                    const char *typeName = type->string;
                    cJSON *values = cJSON_GetObjectItem(wildcard_records, typeName);
                    
                    if (values != NULL) {
                        char wildcardDomain[512];
                        snprintf(wildcardDomain, sizeof(wildcardDomain), "*.%s", domainName);
                        add_record_to_hash(wildcardDomain, typeName, values, "wildcard");
                    }
                }
            }
        }
        
        cJSON *subdomains = cJSON_GetObjectItem(domain, "subdomains");
        if (subdomains != NULL) {
            cJSON *subdomain = NULL;
            cJSON_ArrayForEach(subdomain, subdomains) {
                const char *subdomainName = subdomain->string;
                
                cJSON *subdomain_records = cJSON_GetObjectItem(subdomain, "records");
                if (subdomain_records != NULL) {
                    cJSON *type = NULL;
                    cJSON_ArrayForEach(type, subdomain_records) {
                        const char *typeName = type->string;
                        cJSON *values = cJSON_GetObjectItem(subdomain_records, typeName);
                        
                        if (values != NULL) {
                            char fullSubdomain[512];
                            snprintf(fullSubdomain, sizeof(fullSubdomain), "%s.%s", 
                                   subdomainName, domainName);
                            add_record_to_hash(fullSubdomain, typeName, values, "subdomain");
                        }
                    }
                }
            }
        }
    }
    
    pthread_mutex_unlock(&dns_records_mutex);
    
    free(data);
    cJSON_Delete(json);
    return 0;
}

DNSRecord *resolveRecord(const char *domain, const char *type)
{
    if (domain == NULL || type == NULL) {
        return NULL;
    }
    
    DNSRecord *record = NULL;
    char *key;
    
    if (asprintf(&key, "base_%s_%s", domain, type) != -1) {
        HASH_FIND_STR(dns_records, key, record);
        free(key);
        
        if (record) {
            log_message(LOG_INFO, "Exact match found for domain %s and type %s", domain, type);
            return record;
        }
    }
    
    if (asprintf(&key, "subdomain_%s_%s", domain, type) != -1) {
        HASH_FIND_STR(dns_records, key, record);
        free(key);
        
        if (record) {
            log_message(LOG_INFO, "Subdomain match found for domain %s and type %s", domain, type);
            return record;
        }
    }
    
    char domain_copy[256];
    strncpy(domain_copy, domain, sizeof(domain_copy) - 1);
    domain_copy[sizeof(domain_copy) - 1] = '\0';
    
    char *labels[10] = {NULL};
    int label_count = 0;
    
    char *token = strtok(domain_copy, ".");
    while (token != NULL && label_count < 10) {
        labels[label_count++] = token;
        token = strtok(NULL, ".");
    }
    
    for (int i = 1; i < label_count; i++) {
        char wildcard_domain[256] = "*";
        for (int j = i; j < label_count; j++) {
            strcat(wildcard_domain, ".");
            strcat(wildcard_domain, labels[j]);
        }
        
        if (asprintf(&key, "wildcard_%s_%s", wildcard_domain, type) != -1) {
            HASH_FIND_STR(dns_records, key, record);
            free(key);
            
            if (record) {
                log_message(LOG_INFO, "Wildcard match found for domain %s and type %s", 
                          wildcard_domain, type);
                return record;
            }
        }
    }
    
    log_message(LOG_INFO, "No match found for domain %s and type %s", domain, type);
    return NULL;
}

int add_single_record(const char *domain, const char *type, const char *scope, const char *value)
{
    if (domain == NULL || type == NULL || scope == NULL || value == NULL) {
        return -1;
    }
    
    cJSON *values = cJSON_CreateArray();
    if (values == NULL) {
        return -1;
    }
    
    cJSON *json_value = NULL;
    
    if (strcmp(type, "MX") == 0) {
        int priority;
        char mx_value[256];
        
        if (sscanf(value, "%d %255s", &priority, mx_value) != 2) {
            cJSON_Delete(values);
            return -1;
        }
        
        cJSON *mx_obj = cJSON_CreateObject();
        if (mx_obj == NULL) {
            cJSON_Delete(values);
            return -1;
        }
        
        if (!cJSON_AddNumberToObject(mx_obj, "priority", priority) ||
            !cJSON_AddStringToObject(mx_obj, "value", mx_value)) {
            cJSON_Delete(mx_obj);
            cJSON_Delete(values);
            return -1;
        }
        
        json_value = mx_obj;
    } else {
        json_value = cJSON_CreateString(value);
        if (json_value == NULL) {
            cJSON_Delete(values);
            return -1;
        }
    }
    
    if (!cJSON_AddItemToArray(values, json_value)) {
        cJSON_Delete(json_value);
        cJSON_Delete(values);
        return -1;
    }
    
    pthread_mutex_lock(&dns_records_mutex);
    add_record_to_hash(domain, type, values, scope);
    pthread_mutex_unlock(&dns_records_mutex);
    
    cJSON_Delete(values);
    return 0;
}

int delete_record(const char *domain, const char *type, const char *scope)
{
    if (domain == NULL || type == NULL || scope == NULL) {
        return -1;
    }
    
    int result = -1;
    pthread_mutex_lock(&dns_records_mutex);
    
    DNSRecord *record = NULL;
    char *key;
    
    if (asprintf(&key, "%s_%s_%s", scope, domain, type) != -1) {
        HASH_FIND_STR(dns_records, key, record);
        
        if (record) {
            HASH_DEL(dns_records, record);
            
            free(record->domain);
            free(record->type);
            free(record->key);
            
            for (int i = 0; i < record->num_values; i++) {
                free(record->values[i]);
            }
            free(record->values);
            free(record);
            
            result = 0;
        }
        
        free(key);
    }
    
    pthread_mutex_unlock(&dns_records_mutex);
    return result;
}

char *get_records_list(void)
{
    pthread_mutex_lock(&dns_records_mutex);
    
    size_t buf_size = 1024;
    char *buffer = malloc(buf_size);
    
    if (buffer == NULL) {
        pthread_mutex_unlock(&dns_records_mutex);
        return NULL;
    }
    
    size_t offset = 0;
    int n;
    n = snprintf(buffer + offset, buf_size - offset, "Current DNS Records:\n");
    if (n >= buf_size - offset) {
        free(buffer);
        pthread_mutex_unlock(&dns_records_mutex);
        return NULL;
    }
    offset += n;
    
    DNSRecord *record, *tmp;
    HASH_ITER(hh, dns_records, record, tmp) {
        n = snprintf(buffer + offset, buf_size - offset, 
                   "Domain: %s, Type: %s, Values: ", 
                   record->domain, record->type);
        if (n >= buf_size - offset) {
            buf_size *= 2;
            char *new_buffer = realloc(buffer, buf_size);
            if (new_buffer == NULL) {
                free(buffer);
                pthread_mutex_unlock(&dns_records_mutex);
                return NULL;
            }
            buffer = new_buffer;
            
            n = snprintf(buffer + offset, buf_size - offset, 
                       "Domain: %s, Type: %s, Values: ", 
                       record->domain, record->type);
        }
        offset += n;
        
        for (int i = 0; i < record->num_values; i++) {
            const char *separator = (i < record->num_values - 1) ? ", " : "\n";
            n = snprintf(buffer + offset, buf_size - offset, 
                       "%s%s", record->values[i], separator);
            
            if (n >= buf_size - offset) {
                buf_size *= 2;
                char *new_buffer = realloc(buffer, buf_size);
                if (new_buffer == NULL) {
                    free(buffer);
                    pthread_mutex_unlock(&dns_records_mutex);
                    return NULL;
                }
                buffer = new_buffer;
                
                n = snprintf(buffer + offset, buf_size - offset, 
                           "%s%s", record->values[i], separator);
            }
            offset += n;
        }
    }
    
    pthread_mutex_unlock(&dns_records_mutex);
    return buffer;
}

void log_message(LogLevel level, const char *format, ...)
{
    if (level == LOG_DEBUG && !config.verbose) {
        return;
    }
    
    const char *level_str;
    FILE *output = stdout;
    
    switch (level) {
        case LOG_DEBUG:
            level_str = "DEBUG";
            break;
        case LOG_INFO:
            level_str = "INFO";
            break;
        case LOG_WARNING:
            level_str = "WARNING";
            output = stderr;
            break;
        case LOG_ERROR:
            level_str = "ERROR";
            output = stderr;
            break;
        default:
            level_str = "UNKNOWN";
            break;
    }
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    
    fprintf(output, "[%s] [%s] ", timestamp, level_str);
    
    va_list args;
    va_start(args, format);
    vfprintf(output, format, args);
    va_end(args);
    
    fprintf(output, "\n");
    fflush(output);
}

void handle_add_command(int client_fd, char *input) {
    char *domain = strtok(NULL, " \t\n");
    char *type = strtok(NULL, " \t\n");
    char *scope = strtok(NULL, " \t\n");
    char *value = strtok(NULL, "\n");
    
    if (domain == NULL || type == NULL || scope == NULL || value == NULL) {
        const char *response = "ERROR: Missing parameters. Usage: ADD domain type scope value\n";
        write(client_fd, response, strlen(response));
        return;
    }
    
    if (add_single_record(domain, type, scope, value) == 0) {
        const char *response = "SUCCESS: Record added\n";
        write(client_fd, response, strlen(response));
    } else {
        const char *response = "ERROR: Failed to add record\n";
        write(client_fd, response, strlen(response));
    }
}

void handle_delete_command(int client_fd, char *input) {
    char *domain = strtok(NULL, " \t\n");
    char *type = strtok(NULL, " \t\n");
    char *scope = strtok(NULL, " \t\n");
    
    if (domain == NULL || type == NULL || scope == NULL) {
        const char *response = "ERROR: Missing parameters. Usage: DELETE domain type scope\n";
        write(client_fd, response, strlen(response));
        return;
    }
    
    if (delete_record(domain, type, scope) == 0) {
        const char *response = "SUCCESS: Record deleted\n";
        write(client_fd, response, strlen(response));
    } else {
        const char *response = "ERROR: Record not found\n";
        write(client_fd, response, strlen(response));
    }
}

void handle_list_command(int client_fd) {
    char *list = get_records_list();
    
    if (list != NULL) {
        write(client_fd, list, strlen(list));
        free(list);
    } else {
        const char *response = "ERROR: Failed to generate records list\n";
        write(client_fd, response, strlen(response));
    }
}

void handle_reload_command(int client_fd) {
    pthread_mutex_lock(&dns_records_mutex);
    
    DNSRecord *record, *tmp;
    HASH_ITER(hh, dns_records, record, tmp) {
        HASH_DEL(dns_records, record);
        free(record->domain);
        free(record->type);
        free(record->key);
        for (int i = 0; i < record->num_values; i++) {
            free(record->values[i]);
        }
        free(record->values);
        free(record);
    }
    
    pthread_mutex_unlock(&dns_records_mutex);
    
    if (loadDNSMappings(config.mappings_file) == 0) {
        const char *response = "SUCCESS: Configuration reloaded\n";
        write(client_fd, response, strlen(response));
    } else {
        const char *response = "ERROR: Failed to reload configuration\n";
        write(client_fd, response, strlen(response));
    }
}

void *management_thread(void *arg) {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        log_message(LOG_ERROR, "Management socket creation failed: %s", strerror(errno));
        return NULL;
    }
    
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        log_message(LOG_ERROR, "Management setsockopt failed: %s", strerror(errno));
        close(server_fd);
        return NULL;
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(config.mgmt_port);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        log_message(LOG_ERROR, "Management interface bind failed: %s", strerror(errno));
        close(server_fd);
        return NULL;
    }
    
    if (listen(server_fd, 3) < 0) {
        log_message(LOG_ERROR, "Management interface listen failed: %s", strerror(errno));
        close(server_fd);
        return NULL;
    }
    
    log_message(LOG_INFO, "DNS Management interface listening on port %d", config.mgmt_port);
    
    while (running) {
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        
        int activity = select(server_fd + 1, &readfds, NULL, NULL, &tv);
        
        if (activity < 0 && errno != EINTR) {
            log_message(LOG_ERROR, "Select error: %s", strerror(errno));
            continue;
        }
        if (activity == 0) {
            continue;
        }
        
        if ((client_fd = accept(server_fd, (struct sockaddr *)&address, 
                               (socklen_t*)&addrlen)) < 0) {
            log_message(LOG_ERROR, "Accept failed: %s", strerror(errno));
            continue;
        }
        
        memset(buffer, 0, sizeof(buffer));
        int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            
            char *token = strtok(buffer, " \t\n");
            if (token == NULL || strcmp(token, config.auth_token) != 0) {
                const char *response = "ERROR: Authentication failed\n";
                write(client_fd, response, strlen(response));
                close(client_fd);
                continue;
            }
            
            char *cmd = strtok(NULL, " \t\n");
            if (cmd == NULL) {
                const char *response = "ERROR: No command specified\n";
                write(client_fd, response, strlen(response));
            } else if (strcasecmp(cmd, "ADD") == 0) {
                handle_add_command(client_fd, cmd);
            } else if (strcasecmp(cmd, "DELETE") == 0) {
                handle_delete_command(client_fd, cmd);
            } else if (strcasecmp(cmd, "LIST") == 0) {
                handle_list_command(client_fd);
            } else if (strcasecmp(cmd, "RELOAD") == 0) {
                handle_reload_command(client_fd);
            } else {
                const char *response = "ERROR: Unknown command\n";
                write(client_fd, response, strlen(response));
            }
        }
        
        close(client_fd);
    }
    
    close(server_fd);
    return NULL;
}