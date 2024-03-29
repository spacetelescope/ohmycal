//! @file download.h
#ifndef OMC_DOWNLOAD_H
#define OMC_DOWNLOAD_H

#include <curl/curl.h>

size_t download_writer(void *fp, size_t size, size_t nmemb, void *stream);
long download(char *url, const char *filename, char **errmsg);

#endif //OMC_DOWNLOAD_H
