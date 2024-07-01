#include <stdlib.h>
#include <string.h>
#include "strlist.h"
#include "junitxml.h"

static void testcase_result_state_free(struct JUNIT_Testcase **testcase) {
    struct JUNIT_Testcase *tc = (*testcase);
    if (tc->tc_result_state_type == JUNIT_RESULT_STATE_FAILURE) {
        guard_free(tc->result_state.failure->message);
        guard_free(tc->result_state.failure);
    } else if (tc->tc_result_state_type == JUNIT_RESULT_STATE_SKIPPED) {
        guard_free(tc->result_state.skipped->message);
        guard_free(tc->result_state.skipped);
    }
}

static void testcase_free(struct JUNIT_Testcase **testcase) {
    struct JUNIT_Testcase *tc = (*testcase);
    guard_free(tc->name);
    guard_free(tc->message);
    guard_free(tc->classname);
    testcase_result_state_free(&tc);
    guard_free(tc);
}

void junitxml_testsuite_free(struct JUNIT_Testsuite **testsuite) {
    struct JUNIT_Testsuite *suite = (*testsuite);
    guard_free(suite->name);
    guard_free(suite->hostname);
    guard_free(suite->timestamp);
    for (size_t i = 0; i < suite->_tc_alloc; i++) {
        testcase_free(&suite->testcase[i]);
    }
    guard_free(suite);
}

static int testsuite_append_testcase(struct JUNIT_Testsuite **testsuite, struct JUNIT_Testcase *testcase) {
    struct JUNIT_Testsuite *suite = (*testsuite);
    struct JUNIT_Testcase **tmp = realloc(suite->testcase, (suite->_tc_alloc + 1 ) * sizeof(*testcase));
    if (!tmp) {
        return -1;
    } else if (tmp != suite->testcase) {
        suite->testcase = tmp;
    }
    suite->testcase[suite->_tc_inuse] = testcase;
    suite->_tc_inuse++;
    suite->_tc_alloc++;
    return 0;
}

static struct JUNIT_Failure *testcase_failure_from_attributes(struct StrList *attrs) {
    struct JUNIT_Failure *result;

    result = calloc(1, sizeof(*result));
    if(!result) {
        return NULL;
    }
    for (size_t x = 0; x < strlist_count(attrs); x += 2) {
        char *attr_name = strlist_item(attrs, x);
        char *attr_value = strlist_item(attrs, x + 1);
        if (!strcmp(attr_name, "message")) {
            result->message = strdup(attr_value);
        }
    }
    return result;
}

static struct JUNIT_Error *testcase_error_from_attributes(struct StrList *attrs) {
    struct JUNIT_Error *result;

    result = calloc(1, sizeof(*result));
    if(!result) {
        return NULL;
    }
    for (size_t x = 0; x < strlist_count(attrs); x += 2) {
        char *attr_name = strlist_item(attrs, x);
        char *attr_value = strlist_item(attrs, x + 1);
        if (!strcmp(attr_name, "message")) {
            result->message = strdup(attr_value);
        }
    }
    return result;
}

static struct JUNIT_Skipped *testcase_skipped_from_attributes(struct StrList *attrs) {
    struct JUNIT_Skipped *result;

    result = calloc(1, sizeof(*result));
    if(!result) {
        return NULL;
    }
    for (size_t x = 0; x < strlist_count(attrs); x += 2) {
        char *attr_name = strlist_item(attrs, x);
        char *attr_value = strlist_item(attrs, x + 1);
        if (!strcmp(attr_name, "message")) {
            result->message = strdup(attr_value);
        }
    }
    return result;
}

static struct JUNIT_Testcase *testcase_from_attributes(struct StrList *attrs) {
    struct JUNIT_Testcase *result;

    result = calloc(1, sizeof(*result));
    if(!result) {
        return NULL;
    }
    for (size_t x = 0; x < strlist_count(attrs); x += 2) {
        char *attr_name = strlist_item(attrs, x);
        char *attr_value = strlist_item(attrs, x + 1);
        if (!strcmp(attr_name, "name")) {
            result->name = strdup(attr_value);
        } else if (!strcmp(attr_name, "classname")) {
            result->classname = strdup(attr_value);
        } else if (!strcmp(attr_name, "time")) {
            result->time = strtof(attr_value, NULL);
        } else if (!strcmp(attr_name, "message")) {
            result->message = strdup(attr_value);
        }
    }
    return result;
}

static struct StrList *attributes_to_strlist(xmlTextReaderPtr reader) {
    struct StrList *list;
    xmlNodePtr node = xmlTextReaderCurrentNode(reader);
    if (!node) {
        return NULL;
    }

    list = strlist_init();
    if (xmlTextReaderNodeType(reader) == 1 && node->properties) {
        xmlAttr *attr = node->properties;
        while (attr && attr->name && attr->children) {
            char *attr_name = (char *) attr->name;
            char *attr_value = (char *) xmlNodeListGetString(node->doc, attr->children, 1);
            strlist_append(&list, attr_name ? attr_name : "");
            strlist_append(&list, attr_value ? attr_value : "");
            xmlFree((xmlChar *) attr_value);
            attr = attr->next;
        }
    }
    return list;
}

static int read_xml_data(xmlTextReaderPtr reader, struct JUNIT_Testsuite **testsuite) {
    const xmlChar *name;
    //const xmlChar *value;

    name = xmlTextReaderConstName(reader);
    if (!name) {
        // name could not be converted to string
        name = BAD_CAST "--";
    }
    //value = xmlTextReaderConstValue(reader);
    const char *node_name = (char *) name;
    //const char *node_value = (char *) value;
    
    struct StrList *attrs = attributes_to_strlist(reader);
    if (attrs && strlist_count(attrs)) {
        if (!strcmp(node_name, "testsuite")) {
            for (size_t x = 0; x < strlist_count(attrs); x += 2) {
                char *attr_name = strlist_item(attrs, x);
                char *attr_value = strlist_item(attrs, x + 1);
                if (!strcmp(attr_name, "name")) {
                    (*testsuite)->name = strdup(attr_value);
                } else if (!strcmp(attr_name, "errors")) {
                    (*testsuite)->errors = (int) strtol(attr_value, NULL, 10);
                } else if (!strcmp(attr_name, "failures")) {
                    (*testsuite)->failures = (int) strtol(attr_value, NULL, 0);
                } else if (!strcmp(attr_name, "skipped")) {
                    (*testsuite)->skipped = (int) strtol(attr_value, NULL, 0);
                } else if (!strcmp(attr_name, "tests")) {
                    (*testsuite)->tests = (int) strtol(attr_value, NULL, 0);
                } else if (!strcmp(attr_name, "time")) {
                    (*testsuite)->time = strtof(attr_value, NULL);
                } else if (!strcmp(attr_name, "timestamp")) {
                    (*testsuite)->timestamp = strdup(attr_value);
                } else if (!strcmp(attr_name, "hostname")) {
                    (*testsuite)->hostname = strdup(attr_value);
                }
            }
        } else if (!strcmp(node_name, "testcase")) {
            struct JUNIT_Testcase *testcase = testcase_from_attributes(attrs);
            testsuite_append_testcase(testsuite, testcase);
        } else if (!strcmp(node_name, "failure")) {
            size_t cur_tc = (*testsuite)->_tc_inuse > 0 ? (*testsuite)->_tc_inuse - 1 : (*testsuite)->_tc_inuse;
            struct JUNIT_Failure *failure = testcase_failure_from_attributes(attrs);
            (*testsuite)->testcase[cur_tc]->tc_result_state_type = JUNIT_RESULT_STATE_FAILURE;
            (*testsuite)->testcase[cur_tc]->result_state.failure = failure;
        } else if (!strcmp(node_name, "error")) {
            size_t cur_tc = (*testsuite)->_tc_inuse > 0 ? (*testsuite)->_tc_inuse - 1 : (*testsuite)->_tc_inuse;
            struct JUNIT_Error *error = testcase_error_from_attributes(attrs);
            (*testsuite)->testcase[cur_tc]->tc_result_state_type = JUNIT_RESULT_STATE_ERROR;
            (*testsuite)->testcase[cur_tc]->result_state.error = error;
        } else if (!strcmp(node_name, "skipped")) {
            size_t cur_tc = (*testsuite)->_tc_inuse > 0 ? (*testsuite)->_tc_inuse - 1 : (*testsuite)->_tc_inuse;
            struct JUNIT_Skipped *skipped = testcase_skipped_from_attributes(attrs);
            (*testsuite)->testcase[cur_tc]->tc_result_state_type = JUNIT_RESULT_STATE_SKIPPED;
            (*testsuite)->testcase[cur_tc]->result_state.skipped = skipped;
        }
    }
    guard_strlist_free(&attrs);
    return 0;
}

static int read_xml_file(const char *filename, struct JUNIT_Testsuite **testsuite) {
    xmlTextReaderPtr reader;
    int result;

    reader = xmlReaderForFile(filename, NULL, 0);
    if (!reader) {
        return -1;
    }

    result = xmlTextReaderRead(reader);
    while (result == 1) {
        read_xml_data(reader, testsuite);
        result = xmlTextReaderRead(reader);
    }

    xmlFreeTextReader(reader);
    return 0;
}

struct JUNIT_Testsuite *junitxml_testsuite_read(const char *filename) {
    struct JUNIT_Testsuite *result;

    if (access(filename, F_OK)) {
        return NULL;
    }

    result = calloc(1, sizeof(*result));
    if (!result) {
        return NULL;
    }
    read_xml_file(filename, &result);
    return result;
}