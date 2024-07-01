#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "core.h"
#include "ini.h"

struct INIFILE *ini_init() {
    struct INIFILE *ini;
    ini = calloc(1, sizeof(*ini));
    ini->section_count = 0;
    return ini;
}

void ini_section_init(struct INIFILE **ini) {
    (*ini)->section = calloc((*ini)->section_count + 1, sizeof(**(*ini)->section));
}

struct INISection *ini_section_search(struct INIFILE **ini, unsigned mode, const char *value) {
    struct INISection *result = NULL;
    for (size_t i = 0; i < (*ini)->section_count; i++) {
        if ((*ini)->section[i]->key != NULL) {
            if (mode == INI_SEARCH_EXACT) {
                if (!strcmp((*ini)->section[i]->key, value)) {
                    result = (*ini)->section[i];
                    break;
                }
            } else if (mode == INI_SEARCH_BEGINS) {
                if (startswith((*ini)->section[i]->key, value)) {
                    result = (*ini)->section[i];
                    break;
                }
            } else if (mode == INI_SEARCH_SUBSTR) {
                if (strstr((*ini)->section[i]->key, value)) {
                    result = (*ini)->section[i];
                    break;
                }
            }
        }
    }
    return result;
}

int ini_data_init(struct INIFILE **ini, char *section_name) {
    struct INISection *section = ini_section_search(ini, INI_SEARCH_EXACT, section_name);
    if (section == NULL) {
        return 1;
    }
    section->data = calloc(section->data_count + 1, sizeof(**section->data));
    return 0;
}

int ini_has_key(struct INIFILE *ini, const char *section_name, const char *key) {
    if (!ini || !section_name || !key) {
        return 0;
    }
    struct INISection *section = ini_section_search(&ini, INI_SEARCH_EXACT, section_name);
    if (!section) {
        return 0;
    }
    for (size_t i = 0; i < section->data_count; i++) {
        const struct INIData *data = section->data[i];
        if (data && data->key) {
            if (!strcmp(data->key, key)) {
                return 1;
            }
        }
    }
    return 0;
}

struct INIData *ini_data_get(struct INIFILE *ini, char *section_name, char *key) {
    struct INISection *section = NULL;

    section = ini_section_search(&ini, INI_SEARCH_EXACT, section_name);
    if (!section) {
        return NULL;
    }

    for (size_t i = 0; i < section->data_count; i++) {
        if (section->data[i]->key != NULL) {
            if (!strcmp(section->data[i]->key, key)) {
                return section->data[i];
            }
        }
    }

    return NULL;
}

struct INIData *ini_getall(struct INIFILE *ini, char *section_name) {
    struct INISection *section = NULL;
    struct INIData *result = NULL;
    static size_t i = 0;

    section = ini_section_search(&ini, INI_SEARCH_EXACT, section_name);
    if (!section) {
        return NULL;
    }
    if (i == section->data_count) {
        i = 0;
        return NULL;
    }
    if (section->data_count) {
        result = section->data[i];
        i++;
    } else {
        result = NULL;
        i = 0;
    }

    return result;
}

int ini_getval(struct INIFILE *ini, char *section_name, char *key, int type, union INIVal *result) {
    char *token = NULL;
    char tbuf[STASIS_BUFSIZ];
    char *tbufp = tbuf;
    struct INIData *data;
    data = ini_data_get(ini, section_name, key);
    if (!data) {
        result->as_char_p = NULL;
        return -1;
    }
    switch (type) {
        case INIVAL_TYPE_INT:
            result->as_int = (int) strtol(data->value, NULL, 10);
            break;
        case INIVAL_TYPE_UINT:
            result->as_uint = (unsigned int) strtoul(data->value, NULL, 10);
            break;
        case INIVAL_TYPE_LONG:
            result->as_long = (long) strtol(data->value, NULL, 10);
            break;
        case INIVAL_TYPE_ULONG:
            result->as_ulong = (unsigned long) strtoul(data->value, NULL, 10);
            break;
        case INIVAL_TYPE_LLONG:
            result->as_llong = (long long) strtoll(data->value, NULL, 10);
            break;
        case INIVAL_TYPE_ULLONG:
            result->as_ullong = (unsigned long long) strtoull(data->value, NULL, 10);
            break;
        case INIVAL_TYPE_DOUBLE:
            result->as_double = (double) strtod(data->value, NULL);
            break;
        case INIVAL_TYPE_FLOAT:
            result->as_float = (float) strtod(data->value, NULL);
            break;
        case INIVAL_TYPE_STR:
            result->as_char_p = lstrip(data->value);
            break;
        case INIVAL_TYPE_STR_ARRAY:
            strcpy(tbufp, data->value);
            *data->value = '\0';
            while ((token = strsep(&tbufp, "\n")) != NULL) {
                lstrip(token);
                strcat(data->value, token);
                strcat(data->value, "\n");
            }
            result->as_char_p = data->value;
            break;
        case INIVAL_TYPE_BOOL:
            result->as_bool = false;
            if ((!strcmp(data->value, "true") || !strcmp(data->value, "True")) ||
                    (!strcmp(data->value, "yes") || !strcmp(data->value, "Yes")) ||
                    strtol(data->value, NULL, 10)) {
                result->as_bool = true;
            }
            break;
        default:
            memset(result, 0, sizeof(*result));
            break;
    }
    return 0;
}

int ini_data_append(struct INIFILE **ini, char *section_name, char *key, char *value) {
    struct INISection *section = ini_section_search(ini, INI_SEARCH_EXACT, section_name);
    if (section == NULL) {
        return 1;
    }

    struct INIData **tmp = realloc(section->data, (section->data_count + 1) * sizeof(**section->data));
    if (tmp != section->data) {
        section->data = tmp;
    } else if (!tmp) {
        return 1;
    }
    if (!ini_data_get((*ini), section_name, key)) {
        struct INIData **data = section->data;
        data[section->data_count] = calloc(1, sizeof(*data[0]));
        if (!data[section->data_count]) {
            SYSERROR("Unable to allocate %zu bytes for section data", sizeof(*data[0]));
            return -1;
        }
        data[section->data_count]->key = key ? strdup(key) : strdup("");
        if (!data[section->data_count]->key) {
            SYSERROR("Unable to allocate data key%s", "");
            return -1;
        }
        data[section->data_count]->value = strdup(value);
        if (!data[section->data_count]->value) {
            SYSERROR("Unable to allocate data value%s", "");
            return -1;
        }
        section->data_count++;
    } else {
        struct INIData *data = ini_data_get(*ini, section_name, key);
        size_t value_len_old = strlen(data->value);
        size_t value_len = strlen(value);
        size_t value_len_new = value_len_old + value_len;
        char *value_tmp = NULL;
        value_tmp = realloc(data->value, value_len_new + 2);
        if (value_tmp != data->value) {
            data->value = value_tmp;
        } else if (!value_tmp) {
            SYSERROR("Unable to increase data->value size to %zu bytes", value_len_new + 2);
            return -1;
        }
        strcat(data->value, value);
    }
    return 0;
}

int ini_setval(struct INIFILE **ini, unsigned type, char *section_name, char *key, char *value) {
    struct INISection *section = ini_section_search(ini, INI_SEARCH_EXACT, section_name);
    if (section == NULL) {
        // no section
        return -1;
    }
    if (ini_has_key(*ini, section_name, key)) {
        if (!type) {
            if (ini_data_append(ini, section_name, key, value)) {
                // append failed
                return -1;
            }
        } else {
            struct INIData *data = ini_data_get(*ini, section_name, key);
            if (data) {
                guard_free(data->value);
                data->value = strdup(value);
                if (!data->value) {
                    // allocation failed
                    return -1;
                }
            } else {
                // getting data failed
                return -1;
            }
        }
    }
    return 0;
}

int ini_section_create(struct INIFILE **ini, char *key) {
    struct INISection **tmp = realloc((*ini)->section, ((*ini)->section_count + 1) * sizeof(**(*ini)->section));
    if (!tmp) {
        return 1;
    } else if (tmp != (*ini)->section) {
        (*ini)->section = tmp;
    }

    (*ini)->section[(*ini)->section_count] = calloc(1, sizeof(*(*ini)->section[0]));
    if (!(*ini)->section[(*ini)->section_count]) {
        return -1;
    }

    (*ini)->section[(*ini)->section_count]->key = strdup(key);
    if (!(*ini)->section[(*ini)->section_count]->key) {
        return -1;
    }

    (*ini)->section_count++;
    return 0;
}

int ini_write(struct INIFILE *ini, FILE **stream, unsigned mode) {
    if (!*stream) {
        return -1;
    }
    for (size_t x = 0; x < ini->section_count; x++) {
        fprintf(*stream, "[%s]" LINE_SEP, ini->section[x]->key);
        for (size_t y = 0; y < ini->section[x]->data_count; y++) {
            char outvalue[STASIS_BUFSIZ];
            memset(outvalue, 0, sizeof(outvalue));
            if (ini->section[x]->data[y]->value) {
                char **parts = split(ini->section[x]->data[y]->value, LINE_SEP, 0);
                size_t parts_total = 0;
                for (; parts && parts[parts_total] != NULL; parts_total++);
                for (size_t p = 0; parts && parts[p] != NULL; p++) {
                    char *render = NULL;
                    if (mode == INI_WRITE_PRESERVE) {
                        render = tpl_render(parts[p]);
                    } else {
                        render = parts[p];
                    }
                    if (p == 0) {
                        sprintf(outvalue, "%s" LINE_SEP, render);
                    } else {
                        sprintf(outvalue + strlen(outvalue), "    %s" LINE_SEP, render);
                    }
                    if (mode == INI_WRITE_PRESERVE) {
                        guard_free(render);
                    }
                }
                GENERIC_ARRAY_FREE(parts);
                strip(outvalue);
                strcat(outvalue, LINE_SEP);
                fprintf(*stream, "%s=%s%s", ini->section[x]->data[y]->key, parts_total > 1 ? LINE_SEP "    " : "", outvalue);
            } else {
                fprintf(*stream, "%s=%s", ini->section[x]->data[y]->key, ini->section[x]->data[y]->value);
            }
        }
        fprintf(*stream, LINE_SEP);
    }
    return 0;
}

char *unquote(char *s) {
    if ((startswith(s, "'") && endswith(s, "'"))
        || (startswith(s, "\"") && endswith(s, "\""))) {
        memmove(s, s + 1, strlen(s));
        s[strlen(s) - 1] = '\0';
    }
    return s;
}

void ini_free(struct INIFILE **ini) {
    for (size_t section = 0; section < (*ini)->section_count; section++) {
#ifdef DEBUG
        SYSERROR("freeing section: %s", (*ini)->section[section]->key);
#endif
        for (size_t data = 0; data < (*ini)->section[section]->data_count; data++) {
            if ((*ini)->section[section]->data[data]) {
#ifdef DEBUG
                SYSERROR("freeing data key: %s", (*ini)->section[section]->data[data]->key);
#endif
                guard_free((*ini)->section[section]->data[data]->key);
#ifdef DEBUG
                SYSERROR("freeing data value: %s", (*ini)->section[section]->data[data]->value);
#endif
                guard_free((*ini)->section[section]->data[data]->value);
                guard_free((*ini)->section[section]->data[data]);
            }
        }
        guard_free((*ini)->section[section]->data);
        guard_free((*ini)->section[section]->key);
        guard_free((*ini)->section[section]);
    }
    guard_free((*ini)->section);
    guard_free((*ini));
}

struct INIFILE *ini_open(const char *filename) {
    FILE *fp;
    char line[STASIS_BUFSIZ] = {0};
    char current_section[STASIS_BUFSIZ] = {0};
    char reading_value = 0;

    struct INIFILE *ini = ini_init();
    if (ini == NULL) {
        return NULL;
    }

    ini_section_init(&ini);

    // Create an implicit section. [default] does not need to be present in the INI config
    ini_section_create(&ini, "default");
    strcpy(current_section, "default");

    // Open the configuration file for reading
    fp = fopen(filename, "r");
    if (!fp) {
        ini_free(&ini);
        ini = NULL;
        return NULL;
    }

    int multiline_data = 0;
    int no_data = 0;
    char inikey[2][255];
    char *key = inikey[0];
    char *key_last = inikey[1];
    char value[STASIS_BUFSIZ];

    memset(value, 0, sizeof(value));
    memset(inikey, 0, sizeof(inikey));

    // Read file
    for (size_t i = 0; fgets(line, sizeof(line), fp) != NULL; i++) {
        if (no_data && multiline_data) {
            if (!isempty(line)) {
                no_data = 0;
            } else {
                multiline_data = 0;
            }
            memset(value, 0, sizeof(value));
        } else {
            memset(key, 0, sizeof(inikey[0]));
        }
        // Find pointer to first comment character
        char *comment = strpbrk(line, ";#");
        if (comment) {
            if (!reading_value || line - comment == 0) {
                // Remove comment from line (standalone and inline comments)
                if (!((comment - line > 0 && (*(comment - 1) == '\\')) || (*comment - 1) == '#')) {
                    *comment = '\0';
                } else {
                    // Handle escaped comment characters. Remove the escape character '\'
                    memmove(comment - 1, comment, strlen(comment));
                    if (strlen(comment)) {
                        comment[strlen(comment) - 1] = '\0';
                    } else {
                        comment[0] = '\0';
                    }
                }
            }
        }

        // Test for section header: [string]
        if (startswith(line, "[")) {
            // The previous key is irrelevant now
            memset(key_last, 0, sizeof(inikey[1]));

            char *section_name = substring_between(line, "[]");
            if (!section_name) {
                fprintf(stderr, "error: invalid section syntax, line %zu: '%s'\n", i + 1, line);
                return NULL;
            }

            // Ignore default section because we already have an implicit one
            if (!strncmp(section_name, "default", strlen("default"))) {
                guard_free(section_name);
                continue;
            }

            // Create new named section
            strip(section_name);
            ini_section_create(&ini, section_name);

            // Record the name of the section. This is used until another section is found.
            strcpy(current_section, section_name);
            guard_free(section_name);
            continue;
        }

        // no data, skip
        if (!reading_value && isempty(line)) {
            continue;
        }

        char *operator = strchr(line, '=');

        // a value continuation line
        if (multiline_data && (startswith(line, " ") || startswith(line, "\t"))) {
            operator = NULL;
        }

        if (operator) {
            size_t key_len = operator - line;
            memset(key, 0, sizeof(inikey[0]));
            strncpy(key, line, key_len);
            lstrip(key);
            strip(key);
            memset(key_last, 0, sizeof(inikey[1]));
            strcpy(key_last, key);
            reading_value = 1;
            if (strlen(operator) > 1) {
                strcpy(value, &operator[1]);
            } else {
                strcpy(value, "");
            }
            if (isempty(value)) {
                //printf("%s is probably long raw data\n", key);
                multiline_data = 1;
                no_data = 1;
            } else {
                //printf("%s is probably short data\n", key);
                multiline_data = 0;
            }
            strip(value);
        } else {
            strcpy(key, key_last);
            strcpy(value, line);
        }

        // Store key value pair in section's data array
        if (strlen(key)) {
            lstrip(key);
            strip(key);
            unquote(value);
            lstrip(value);
            if (!multiline_data) {
                strip(value);
                reading_value = 0;
                ini_data_append(&ini, current_section, key, value);
                continue;
            }
            ini_data_append(&ini, current_section, key, value);
            reading_value = 1;
        }
    }
    fclose(fp);

    return ini;
}