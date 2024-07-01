#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core.h"

struct GHContent {
    char *data;
    size_t len;
};

static size_t writer(void *contents, size_t size, size_t nmemb, void *result) {
    const size_t newlen = size * nmemb;
    struct GHContent *content = (struct GHContent *) result;

    char *ptr = realloc(content->data, content->len + newlen + 1);
    if (!ptr) {
        perror("realloc failed");
        return 0;
    }

    content->data = ptr;
    memcpy(&(content->data[content->len]), contents, newlen);
    content->len += newlen;
    content->data[content->len] = 0;

    return newlen;
}

static char *unescape_lf(char *value) {
    char *seq = strstr(value, "\\n");
    while (seq != NULL) {
        size_t cur_len = strlen(seq);
        memmove(seq, seq + 1, strlen(seq) - 1);
        *seq = '\n';
        if (strlen(seq) && cur_len) {
            seq[cur_len - 1] = 0;
        }
        seq = strstr(value, "\\n");
    }
    return value;
}

int get_github_release_notes(const char *api_token, const char *repo, const char *tag, const char *target_commitish, char **output) {
    const char *field_body = "\"body\":\"";
    const char *field_message = "\"message\":\"";
    const char *endpoint_header_auth_fmt = "Authorization: Bearer %s";
    const char *endpoint_header_api_version = "X-GitHub-Api-Version: " STASIS_GITHUB_API_VERSION;
    const char *endpoint_post_fields_fmt = "{\"tag_name\":\"%s\", \"target_commitish\":\"%s\"}";
    const char *endpoint_url_fmt = "https://api.github.com/repos/%s/releases/generate-notes";
    char endpoint_header_auth[PATH_MAX] = {0};
    char endpoint_post_fields[PATH_MAX] = {0};
    char endpoint_url[PATH_MAX] = {0};
    struct curl_slist *list = NULL;
    struct GHContent content;

    CURL *curl = curl_easy_init();
    if (!curl) {
        return -1;
    }

    // Render the header data
    sprintf(endpoint_header_auth, endpoint_header_auth_fmt, api_token);
    sprintf(endpoint_post_fields, endpoint_post_fields_fmt, tag, target_commitish);
    sprintf(endpoint_url, endpoint_url_fmt, repo);

    // Begin curl configuration
    curl_easy_setopt(curl, CURLOPT_URL, endpoint_url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, endpoint_post_fields);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &content);

    // Append headers to the request
    list = curl_slist_append(list, "Accept: application/vnd.github+json");
    list = curl_slist_append(list, endpoint_header_auth);
    list = curl_slist_append(list, endpoint_header_api_version);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

    // Set the user-agent (github requires one)
    char user_agent[20] = {0};
    sprintf(user_agent, "stasis/%s", VERSION);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent);

    // Execute curl request
    memset(&content, 0, sizeof(content));
    CURLcode res;
    res = curl_easy_perform(curl);

    // Clean up
    curl_slist_free_all(list);
    curl_easy_cleanup(curl);

    if(res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        return -1;
    }

    // Replace all "\\n" literals with new line characters
    char *line = unescape_lf(content.data);
    if (line) {
        char *data_offset = NULL;
        if ((data_offset = strstr(line, field_body))) {
            // Skip past the body field
            data_offset += strlen(field_body);
            // Remove quotation mark (and trailing comma if it exists)
            int trim = 2;
            char last_char = data_offset[strlen(data_offset) - trim];
            if (last_char == ',') {
                trim++;
            }
            data_offset[strlen(data_offset) - trim] = 0;
            // Extract release notes
            *output = strdup(data_offset);
        } else if ((data_offset = strstr(line, field_message))) {
            // Skip past the message field
            data_offset += strlen(field_message);
            *(strchr(data_offset, '"')) = 0;
            fprintf(stderr, "GitHub API Error: '%s'\n", data_offset);
            fprintf(stderr, "URL: %s\n", endpoint_url);
            fprintf(stderr, "POST: %s\n", endpoint_post_fields);
            guard_free(content.data);
            return -1;
        }
    } else {
        fprintf(stderr, "Unknown error\n");
        guard_free(content.data);
        return -1;
    }

    guard_free(content.data);
    return 0;
}