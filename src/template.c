//
// Created by jhunk on 12/17/23.
//

#include "template.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>


struct tpl_item {
    char *key;
    char **ptr;
};
struct tpl_item *tpl_pool[1024] = {0};
unsigned tpl_pool_used = 0;
struct tplfunc_frame *tpl_pool_func[1024] = {0};
unsigned tpl_pool_func_used = 0;

extern void tpl_reset() {
    tpl_free();
    tpl_pool_used = 0;
    tpl_pool_func_used = 0;
}

void tpl_register_func(char *key, void *tplfunc_ptr, int argc, void *data_in) {
    struct tplfunc_frame *frame = calloc(1, sizeof(*frame));
    frame->key = strdup(key);
    frame->argc = argc;
    frame->func = tplfunc_ptr;
    frame->data_in = data_in;

    tpl_pool_func[tpl_pool_func_used] = frame;
    tpl_pool_func_used++;
}

int tpl_key_exists(char *key) {
    for (size_t i = 0; i < tpl_pool_used; i++) {
        if (tpl_pool[i]->key) {
            if (!strcmp(tpl_pool[i]->key, key)) {
                return true;
            }
        }
    }
    return false;
}

void tpl_register(char *key, char **ptr) {
    struct tpl_item *item = NULL;
    int replacing = 0;

    if (tpl_key_exists(key)) {
        for (size_t i = 0; i < tpl_pool_used; i++) {
            if (tpl_pool[i]->key) {
                if (!strcmp(tpl_pool[i]->key, key)) {
                    item = tpl_pool[i];
                    break;
                }
            }
        }
        replacing = 1;
    } else {
        item = calloc(1, sizeof(*item));
        item->key = strdup(key);
    }

    if (!item) {
        SYSERROR("unable to register tpl_item for %s", key);
        exit(1);
    }

    item->ptr = ptr;
    if (!replacing) {
        tpl_pool[tpl_pool_used] = item;
        tpl_pool_used++;
    }
}

void tpl_free() {
    for (unsigned i = 0; i < tpl_pool_used; i++) {
        struct tpl_item *item = tpl_pool[i];
        if (item) {
            if (item->key) {
#ifdef DEBUG
                SYSERROR("freeing template item key: %s", item->key);
#endif
                guard_free(item->key);
            }
#ifdef DEBUG
            SYSERROR("freeing template item: %p", item);
#endif
            item->ptr = NULL;
        }
        guard_free(item);
    }
    for (unsigned i = 0; i < tpl_pool_func_used; i++) {
        struct tplfunc_frame *item = tpl_pool_func[i];
        guard_free(item);
    }
}

char *tpl_getval(char *key) {
    char *result = NULL;
    for (size_t i = 0; i < tpl_pool_used; i++) {
        if (tpl_pool[i]->key) {
            if (!strcmp(tpl_pool[i]->key, key)) {
                result = *tpl_pool[i]->ptr;
                break;
            }
        }
    }
    return result;
}

struct tplfunc_frame *tpl_getfunc(char *key) {
    struct tplfunc_frame *result = NULL;
    for (size_t i = 0; i < tpl_pool_func_used; i++) {
        if (tpl_pool_func[i]->key) {
            if (!strcmp(tpl_pool_func[i]->key, key)) {
                result = tpl_pool_func[i];
                break;
            }
        }
    }
    return result;
}

static int grow(size_t z, size_t *output_bytes, char **output) {
    if (z >= *output_bytes) {
        size_t new_size = *output_bytes + z + 1;
#ifdef DEBUG
        fprintf(stderr, "template output buffer new size: %zu\n", new_size);
#endif
        char *tmp = realloc(*output, new_size);
        if (!tmp) {
            perror("realloc failed");
            return -1;
        } else if (tmp != *output) {
            *output = tmp;
        }
        *output_bytes = new_size;
    }
    return 0;
}

char *tpl_render(char *str) {
    if (!str) {
        return NULL;
    } else if (!strlen(str)) {
        return strdup("");
    }
    size_t output_bytes = 1024 + strlen(str); // TODO: Is grow working correctly?
    char *output = NULL;
    char *b_close = NULL;
    char *pos = NULL;
    pos = str;

    output = calloc(output_bytes, sizeof(*output));
    if (!output) {
        perror("unable to allocate output buffer");
        return NULL;
    }

    for (size_t off = 0, z = 0; off < strlen(str); off++) {
        char key[255] = {0};
        char *value = NULL;

        memset(key, 0, sizeof(key));
        grow(z, &output_bytes, &output);
        // At opening brace
        if (!strncmp(&pos[off], "{{", 2)) {
            // Scan until key is reached
            while (!isalnum(pos[off])) {
                off++;
            }

            // Read key name
            size_t key_len = 0;
            while (isalnum(pos[off]) || pos[off] != '}') {
                if (isspace(pos[off]) || isblank(pos[off])) {
                    // skip whitespace in key
                    off++;
                    continue;
                }
                key[key_len] = pos[off];
                key_len++;
                off++;
            }

            char *type_stop = NULL;
            type_stop = strchr(key, ':');

            int do_env = 0;
            int do_func = 0;
            if (type_stop) {
                if (!strncmp(key, "env", type_stop - key)) {
                    do_env = 1;
                } else if (!strncmp(key, "func", type_stop - key)) {
                    do_func = 1;
                }
            }

            // Find closing brace
            b_close = strstr(&pos[off], "}}");
            if (!b_close) {
                fprintf(stderr, "error while templating '%s'\n\nunbalanced brace at position %zu\n", str, z);
                return NULL;
            } else {
                // Jump past closing brace
                off = ((b_close + 2) - pos);
            }

            if (do_env) { // {{ env:VAR }}
                char *k = type_stop + 1;
                size_t klen = strlen(k);
                memmove(key, k, klen);
                key[klen] = 0;
                char *env_val = getenv(key);
                value = strdup(env_val ? env_val : "");
            } else if (do_func) { // {{ func:NAME(a, ...) }}
                char func_name_temp[STASIS_NAME_MAX] = {0};
                strcpy(func_name_temp, type_stop + 1);
                char *param_begin = strchr(func_name_temp, '(');
                if (!param_begin) {
                    fprintf(stderr, "At position %zu in %s\nfunction name must be followed by a '('\n", off, key);
                    guard_free(output);
                    return NULL;
                }
                *param_begin = 0;
                param_begin++;
                char *param_end = strrchr(param_begin, ')');
                if (!param_end) {
                    fprintf(stderr, "At position %zu in %s\nfunction arguments must be closed with a ')'\n", off, key);
                    guard_free(output);
                    return NULL;
                }
                *param_end = 0;
                char *k = func_name_temp;
                char **params = split(param_begin, ",", 0);
                int params_count;
                for (params_count = 0; params[params_count] != NULL; params_count++);

                struct tplfunc_frame *frame = tpl_getfunc(k);
                if (params_count > frame->argc || params_count < frame->argc) {
                    fprintf(stderr, "At position %zu in %s\nIncorrect number of arguments for function: %s (expected %d, got %d)\n", off, key, frame->key, frame->argc, params_count);
                    value = strdup("");
                } else {
                    for (size_t p = 0; p < sizeof(frame->argv) / sizeof(*frame->argv) && params[p] != NULL; p++) {
                        lstrip(params[p]);
                        strip(params[p]);
                        frame->argv[p].t_char_ptr = params[p];
                    }
                    char *func_result = NULL;
                    int func_status = 0;
                    if ((func_status = frame->func(frame, &func_result))) {
                        fprintf(stderr, "%s returned non-zero status: %d\n", frame->key, func_status);
                    }
                    value = strdup(func_result ? func_result : "");
                    guard_free(func_result);
                }
                GENERIC_ARRAY_FREE(params);
            } else {
                // Read replacement value
                value = strdup(tpl_getval(key) ? tpl_getval(key) : "");
            }
        }

        if (value) {
            // Set output iterator to end of replacement value
            z += strlen(value);

            // Append replacement value
            grow(z, &output_bytes, &output);
            strcat(output, value);
            guard_free(value);
            output[z] = 0;
        }

#ifdef DEBUG
        fprintf(stderr, "z=%zu, output_bytes=%zu\n", z, output_bytes);
#endif
        output[z] = pos[off];
        z++;
    }
#ifdef DEBUG
    fprintf(stderr, "template output length: %zu\n", strlen(output));
    fprintf(stderr, "template output bytes: %zu\n", output_bytes);
#endif
    return output;
}

int tpl_render_to_file(char *str, const char *filename) {
    char *result;
    FILE *fp;

    // Render the input string
    result = tpl_render(str);
    if (!result) {
        return -1;
    }

    // Open the destination file for writing
    fp = fopen(filename, "w+");
    if (!fp) {
        guard_free(result);
        return -1;
    }

    // Write rendered string to file
    fprintf(fp, "%s", result);
    fclose(fp);

    guard_free(result);
    return 0;
}