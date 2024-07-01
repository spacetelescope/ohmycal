#ifndef STASIS_GITHUB_H
#define STASIS_GITHUB_H

#include <curl/curl.h>

#define STASIS_GITHUB_API_VERSION "2022-11-28"

int get_github_release_notes(const char *api_token, const char *repo, const char *tag, const char *target_commitish, char **output);

#endif //STASIS_GITHUB_H