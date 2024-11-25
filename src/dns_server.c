#include "dns_records.h"

DNSRecord *dns_records = NULL;

void add_record_to_hash(const char *domain, const char *type, cJSON *values, const char *scope)
{
    printf("adding records for domain: %s, type: %s\n", domain, type);
    DNSRecord *record = malloc(sizeof(DNSRecord));
    if (!record)
    {
        perror("failed to allocate memory for DNSRecord");
        return;
    }

    record->domain = strdup(domain);
    record->type = strdup(type);
    asprintf(&(record->key), "%s_%s_%s", scope, domain, type);

    record->num_values = cJSON_GetArraySize(values);
    record->values = malloc(record->num_values * sizeof(char *));
    for (int i = 0; i < record->num_values; i++)
    {
        cJSON *item = cJSON_GetArrayItem(values, i);
        if (strcmp(type, "MX") == 0 && cJSON_IsObject(item))
        {
            cJSON *priority = cJSON_GetObjectItem(item, "priority");
            cJSON *value = cJSON_GetObjectItem(item, "value");
            if (priority && value && cJSON_IsNumber(priority) && cJSON_IsString(value))
            {
                char mx_record[256];
                snprintf(mx_record, sizeof(mx_record), "%d %s", priority->valueint, value->valuestring);
                record->values[i] = strdup(mx_record);
            }
        }
        else if (cJSON_IsString(item))
        {
            record->values[i] = strdup(item->valuestring);
        }
    }

    HASH_ADD_KEYPTR(hh, dns_records, record->key, strlen(record->key), record);
    printf("added %s record for %s with %d values\n", type, domain, record->num_values);
}

void loadDNSMappings(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        perror("failed to open DNS mappings file");
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *data = malloc(length + 1);
    if (!data)
    {
        perror("memory allocation error");
        fclose(file);
        exit(1);
    }

    fread(data, 1, length, file);
    fclose(file);
    data[length] = '\0';

    cJSON *json = cJSON_Parse(data);
    if (!json)
    {
        fprintf(stderr, "JSON parsing error: [%s]\n", cJSON_GetErrorPtr());
        free(data);
        exit(1);
    }

    cJSON *domains = cJSON_GetObjectItem(json, "domains");
    if (!domains || !cJSON_IsObject(domains))
    {
        fprintf(stderr, "invalid schema: 'domains' key missing or not an object\n");
        free(data);
        cJSON_Delete(json);
        exit(1);
    }

    cJSON *domain = NULL;
    cJSON_ArrayForEach(domain, domains)
    {
        const char *domainName = domain->string;

        cJSON *records = cJSON_GetObjectItem(domain, "records");
        if (records)
        {
            int has_cname = 0;
            int has_other_records = 0;

            cJSON *type = NULL;
            cJSON_ArrayForEach(type, records)
            {
                const char *typeName = type->string;
                cJSON *values = cJSON_GetObjectItem(records, typeName);
                if (values)
                {
                    if (strcmp(typeName, "CNAME") == 0)
                    {
                        has_cname = 1;
                    }
                    else
                    {
                        has_other_records = 1;
                    }

                    add_record_to_hash(domainName, typeName, values, "base");
                }
            }

            if (has_cname && has_other_records)
            {
                fprintf(stderr, "Configuration error: %s has CNAME and other records simultaneously.\n", domainName);
                free(data);
                cJSON_Delete(json);
                exit(1);
            }
        }

        cJSON *wildcards = cJSON_GetObjectItem(domain, "wildcards");
        if (wildcards)
        {
            cJSON *wildcard_records = cJSON_GetObjectItem(wildcards, "records");
            if (wildcard_records)
            {
                cJSON *type = NULL;
                cJSON_ArrayForEach(type, wildcard_records)
                {
                    const char *typeName = type->string;
                    cJSON *values = cJSON_GetObjectItem(wildcard_records, typeName);
                    if (values)
                    {
                        char wildcardDomain[512];
                        snprintf(wildcardDomain, sizeof(wildcardDomain), "*.%s", domainName);
                        add_record_to_hash(wildcardDomain, typeName, values, "wildcard");
                    }
                }
            }
        }

        cJSON *subdomains = cJSON_GetObjectItem(domain, "subdomains");
        if (subdomains)
        {
            cJSON *subdomain = NULL;
            cJSON_ArrayForEach(subdomain, subdomains)
            {
                const char *subdomainName = subdomain->string;

                cJSON *subdomain_records = cJSON_GetObjectItem(subdomain, "records");
                if (subdomain_records)
                {
                    cJSON *type = NULL;
                    cJSON_ArrayForEach(type, subdomain_records)
                    {
                        const char *typeName = type->string;
                        cJSON *values = cJSON_GetObjectItem(subdomain_records, typeName);
                        if (values)
                        {
                            char fullSubdomain[512];
                            snprintf(fullSubdomain, sizeof(fullSubdomain), "%s.%s", subdomainName, domainName);
                            add_record_to_hash(fullSubdomain, typeName, values, "subdomain");
                        }
                    }
                }
            }
        }
    }

    free(data);
    cJSON_Delete(json);
}

DNSRecord *resolveRecord(const char *domain, const char *type)
{
    DNSRecord *record = NULL;
    char *key;

    asprintf(&key, "base_%s_%s", domain, type);
    HASH_FIND_STR(dns_records, key, record);
    free(key);
    if (record)
    {
        printf("base match found for domain %s and type %s\n", domain, type);
        return record;
    }

    asprintf(&key, "subdomain_%s_%s", domain, type);
    HASH_FIND_STR(dns_records, key, record);
    free(key);
    if (record)
    {
        printf("subdomain match found for domain %s and type %s\n", domain, type);
        return record;
    }

    char domain_copy[256];
    strncpy(domain_copy, domain, sizeof(domain_copy));
    domain_copy[sizeof(domain_copy) - 1] = '\0';

    char *labels[10];
    int label_count = 0;

    char *token = strtok(domain_copy, ".");
    while (token && label_count < 10)
    {
        labels[label_count++] = token;
        token = strtok(NULL, ".");
    }

    for (int i = 1; i < label_count; i++)
    {
        char wildcard_domain[256] = "*";
        for (int j = i; j < label_count; j++)
        {
            strcat(wildcard_domain, ".");
            strcat(wildcard_domain, labels[j]);
        }

        asprintf(&key, "wildcard_%s_%s", wildcard_domain, type);
        HASH_FIND_STR(dns_records, key, record);
        free(key);
        if (record)
        {
            printf("wildcard match found for domain %s and type %s\n", wildcard_domain, type);
            return record;
        }
    }

    printf("no match found for domain %s and type %s\n", domain, type);
    return NULL;
}
