#include "dns_records.h"

DNSRecord *dns_records = NULL;

void add_record_to_hash(const char *domain, const char *type, cJSON *values)
{
    printf("adding records for domain: %s, type: %s\n", domain, type);
    DNSRecord *record = malloc(sizeof(DNSRecord));
    if (!record)
    {
        perror("Failed to allocate memory for DNSRecord");
        return;
    }

    record->domain = strdup(domain);
    record->type = strdup(type);
    asprintf(&(record->key), "%s_%s", domain, type);

    record->num_values = cJSON_GetArraySize(values);
    record->values = malloc(record->num_values * sizeof(char *));
    for (int i = 0; i < record->num_values; i++)
    {
        if (strcmp(type, "MX") == 0)
        {
            cJSON *mx_entry = cJSON_GetArrayItem(values, i);
            cJSON *priority = cJSON_GetObjectItem(mx_entry, "priority");
            cJSON *value = cJSON_GetObjectItem(mx_entry, "value");
            if (priority && value && cJSON_IsNumber(priority) && cJSON_IsString(value))
            {
                char mx_record[256];
                snprintf(mx_record, sizeof(mx_record), "%d %s", priority->valueint, value->valuestring);
                record->values[i] = strdup(mx_record);
                printf("added MX record for %s: %s\n", domain, mx_record);
            }
        }
        else
        {
            cJSON *item = cJSON_GetArrayItem(values, i);
            if (cJSON_IsString(item))
            {
                record->values[i] = strdup(item->valuestring);
                printf("added value: %s\n", record->values[i]);
            }
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
        perror("failed to open dns mappings file");
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
        fprintf(stderr, "json parsing error: [%s]\n", cJSON_GetErrorPtr());
        free(data);
        exit(1);
    }

    cJSON *domain = NULL;

    cJSON_ArrayForEach(domain, json)
    {
        const char *domainName = domain->string;
        cJSON *types = cJSON_GetObjectItemCaseSensitive(json, domainName);
        if (!types)
        {
            printf("no types found for domain: %s\n", domainName);
            continue;
        }

        cJSON *type = NULL;
        cJSON_ArrayForEach(type, types)
        {
            const char *typeName = type->string;
            cJSON *values = cJSON_GetObjectItemCaseSensitive(types, typeName);
            if (values)
            {
                add_record_to_hash(domainName, typeName, values);
            }
            else
            {
                printf("vals not found for type: %s in domain: %s\n", typeName, domainName);
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
    asprintf(&key, "%s_%s", domain, type);
    HASH_FIND_STR(dns_records, key, record);
    free(key);

    if (record && record->num_values > 0)
    {
        printf("exact match found for domain %s and type %s\n", domain, type);
        return record;
    }

    char domain_copy[256];
    strncpy(domain_copy, domain, sizeof(domain_copy));
    domain_copy[sizeof(domain_copy) - 1] = '\0';

    char *labels[10];
    int label_count = 0;

    char *token = strtok(domain_copy, ".");
    while (token != NULL && label_count < 10)
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

        asprintf(&key, "%s_%s", wildcard_domain, type);
        HASH_FIND_STR(dns_records, key, record);
        free(key);

        if (record && record->num_values > 0)
        {
            printf("wildcard match found for domain %s and type %s\n", wildcard_domain, type);
            return record;
        }
    }

    printf("no match found for domain %s and type %s\n", domain, type);
    return NULL;
}
