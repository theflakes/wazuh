/*
 * Label data operations
 * Copyright (C) 2015-2020, Wazuh Inc.
 * February 27, 2017.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include "shared.h"
#include "wazuh_db/wdb.h"

/* Append a new label into an array of (size) labels at the moment of inserting. Returns the new pointer. */
wlabel_t* labels_add(wlabel_t *labels, size_t * size, const char *key, const char *value, label_flags_t flags, int overwrite) {
    size_t i;

    if (overwrite) {
        for (i = 0; labels && labels[i].key; i++) {
            if (!strcmp(labels[i].key, key)) {
                break;
            }
        }
    } else {
        i = *size;
    }

    if (!labels || i == *size) {
        os_realloc(labels, (*size + 2) * sizeof(wlabel_t), labels);
        labels[(*size)++].key = strdup(key);
        memset(labels + *size, 0, sizeof(wlabel_t));
    } else if (labels) {
        free(labels[i].value);
    }

    labels[i].value = strdup(value);
    labels[i].flags.hidden = flags.hidden;
    labels[i].flags.system = flags.system;
    return labels;
}

/* Search for a key at a label array and get the value, or NULL if no such key found. */
char* labels_get(const wlabel_t *labels, const char *key) {
    int i;

    if (!labels) {
        return NULL;
    }

    for (i = 0; labels[i].key; i++) {
        if (!strcmp(labels[i].key, key)) {
            return labels[i].value;
        }
    }

    return NULL;
}

/* Free label array */
void labels_free(wlabel_t *labels) {
    int i;

    if (labels) {
        for (i = 0; labels[i].key != NULL; i++) {
            free(labels[i].key);
            free(labels[i].value);
        }

        free(labels);
    }
}

/* Format label array into string. Return 0 on success or -1 on error. */
int labels_format(const wlabel_t *labels, char *str, size_t size) {
    int i;
    size_t z = 0, l = 0;

    for (i = 0; labels[i].key != NULL; i++) {
        l = (size_t)snprintf(str + z, size - z, "%s%s\"%s\":%s\n",
            labels[i].flags.system ? "#" : "",
            labels[i].flags.hidden ? "!" : "",
            labels[i].key,
            labels[i].value);

        if (z + l >= size) {
            snprintf(str + z, 50, "%s\n", "Not all labels are being shown in this message");
            return -1;
        }

        z += l;
    }

    return 0;
}

/*
 * Parse labels from Wazuh DB - global.db - labels table.
 * Returns pointer to new null-key terminated array.
 * If there are no labels for the agent, returns NULL.
 * Free resources with labels_free().
 */
wlabel_t* labels_parse(int agent_id) {
    cJSON *json_labels = NULL;
    cJSON *json_row_it = NULL;
    char * str_key = NULL;
    char * str_value = NULL;
    char * key_start = NULL;
    char * key_end = NULL;
    wlabel_t *labels;
    label_flags_t flags;
    size_t size = 0;

    json_labels = wdb_get_agent_labels(agent_id);

    if (json_labels && json_labels->child) {
        os_calloc(1, sizeof(wlabel_t), labels);

        /*
        "key1":value1\n
        !"key2":value2\n
        #"key3":_value3\n
        */

        os_calloc(OS_BUFFER_SIZE, sizeof(char), str_key);
        os_calloc(OS_BUFFER_SIZE, sizeof(char), str_value);

        cJSON_ArrayForEach (json_row_it, json_labels) {
            key_start = NULL;
            key_end = NULL;
            os_strdup(cJSON_GetObjectItem(json_row_it, "key")->valuestring, str_key);
            os_strdup(cJSON_GetObjectItem(json_row_it, "value")->valuestring, str_value);

            switch (*str_key) {
            case '!': // Hidden labels
                if (str_key[1] == '\"') {
                    flags.hidden = 1;
                    flags.system = 0;
                    key_start = str_key + 2;
                } else {
                    continue;
                }

                break;
            case '#': // Internal labels
                if (str_key[1] == '\"') {
                    flags.system = 1;
                    flags.hidden = 0;
                    key_start = str_key + 2;
                } else {
                    continue;
                }

                break;
            case '\"':
                flags.hidden = flags.system = 0;
                key_start = str_key + 1;
                break;
            default:
                continue;
            }

            if (!(key_end = strstr(key_start, "\""))) {
                continue;
            }

            *key_end = '\0';

            labels = labels_add(labels, &size, key_start, str_value, flags, 0);
        }

        os_free(str_key);
        os_free(str_value);
        cJSON_Delete(json_labels);
    }

    return labels;
}

// Duplicate label array
wlabel_t * labels_dup(const wlabel_t * labels) {
    wlabel_t * copy = NULL;
    int i;

    if (!labels) {
        return NULL;
    }

    os_malloc(sizeof(wlabel_t), copy);

    for (i = 0; labels[i].key; i++) {
        os_realloc(copy, sizeof(wlabel_t) * (i + 2), copy);
        os_strdup(labels[i].key, copy[i].key);
        os_strdup(labels[i].value, copy[i].value);
        copy[i].flags = labels[i].flags;
    }

    copy[i].key = copy[i].value = NULL;
    return copy;
}
