#include "testing.h"

#define BOILERPLATE_TEST_STRLIST_AS_TYPE(FUNC, TYPE) \
do { \
    struct StrList *list; \
    list = strlist_init(); \
    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) { \
        strlist_append(&list, (char *) tc[i].data); \
        result = FUNC(list, i);                      \
        if (!strlist_errno) { \
            STASIS_ASSERT(result == (TYPE) tc[i].expected, "unexpected numeric result"); \
        } \
    } \
    guard_strlist_free(&list); \
} while (0)

void test_strlist_init() {
    struct StrList *list;
    list = strlist_init();
    STASIS_ASSERT(list != NULL, "list should not be NULL");
    STASIS_ASSERT(list->num_alloc == 1, "fresh list should have one record allocated");
    STASIS_ASSERT(list->num_inuse == 0, "fresh list should have no records in use");
    guard_strlist_free(&list);
}

void test_strlist_free() {
    struct StrList *list;
    list = strlist_init();
    strlist_free(&list);
    // Not entirely sure what to assert here. On failure, it'll probably segfault.
}

void test_strlist_append() {
    struct testcase {
        const char *data;
        const size_t expected_in_use;
        const char *expected;
    };

    struct testcase tc[] = {
            {.data = "I have a pencil box.", .expected_in_use = 1},
            {.data = "A rectangular pencil box.", .expected_in_use = 2},
            {.data = "If I need a pencil I know where to get one.", .expected_in_use = 3},
    };

    struct StrList *list;
    list = strlist_init();
    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        strlist_append(&list, (char *) tc[i].data);
        STASIS_ASSERT(list->num_inuse == tc[i].expected_in_use, "incorrect number of records in use");
        STASIS_ASSERT(list->num_alloc == tc[i].expected_in_use + 1, "incorrect number of records allocated");
        STASIS_ASSERT(strcmp(strlist_item(list, i), tc[i].data) == 0, "value was appended incorrectly. data mismatch.");
    }
    guard_strlist_free(&list);
}

void test_strlist_append_many_records() {
    struct StrList *list;
    list = strlist_init();
    size_t maxrec = 10240;
    const char *data = "There are many strings but this one is mine.";
    for (size_t i = 0; i < maxrec; i++) {
        strlist_append(&list, (char *) data);
    }
    STASIS_ASSERT(strlist_count(list) == maxrec, "record count mismatch");
    guard_strlist_free(&list);
}

void test_strlist_set() {
    struct StrList *list;
    list = strlist_init();
    size_t maxrec = 100;
    const char *data = "There are many strings but this one is mine.";
    const char *replacement_short = "A shorter string.";
    const char *replacement_long = "A longer string ensures the array grows correctly.";
    int set_resize_long_all_ok = 0;
    int set_resize_short_all_ok = 0;

    for (size_t i = 0; i < maxrec; i++) {
        strlist_append(&list, (char *) data);
    }

    for (size_t i = 0; i < maxrec; i++) {
        int result;
        strlist_set(&list, i, (char *) replacement_long);
        result = strcmp(strlist_item(list, i), replacement_long) == 0;
        set_resize_long_all_ok += result;
    }
    STASIS_ASSERT(set_resize_long_all_ok == (int) maxrec, "Replacing record with longer strings failed");

    for (size_t i = 0; i < maxrec; i++) {
        int result;
        strlist_set(&list, i, (char *) replacement_short);
        result = strcmp(strlist_item(list, i), replacement_short) == 0;
        set_resize_short_all_ok += result;
    }
    STASIS_ASSERT(set_resize_short_all_ok == (int) maxrec, "Replacing record with shorter strings failed");

    STASIS_ASSERT(strlist_count(list) == maxrec, "record count mismatch");
    guard_strlist_free(&list);
}

void test_strlist_append_file() {
    struct testcase {
        const char *origin;
        const char **expected;
    };
    const char *expected[] = {
            "Do not delete this file.\n",
            "Do not delete this file.\n",
            "Do not delete this file.\n",
            "Do not delete this file.\n",
            NULL,
    };
    const char *local_filename = "test_strlist_append_file.txt";

    struct testcase tc[] = {
            {.origin = "https://ssb.stsci.edu/jhunk/stasis_test/test_strlist_append_file_from_remote.txt", .expected = expected},
            {.origin = local_filename, .expected = expected},
    };

    FILE *fp;
    fp = fopen(local_filename, "w");
    if (!fp) {
        STASIS_ASSERT(false, "unable to open local file for writing");
    }
    for (size_t i = 0; expected[i] != NULL; i++) {
        fprintf(fp, "%s", expected[i]);
    }
    fclose(fp);

    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        struct StrList *list;
        list = strlist_init();
        if (!list) {
            STASIS_ASSERT(false, "failed to create list");
            return;
        }
        strlist_append_file(list, (char *) tc[i].origin, NULL);
        for (size_t z = 0; z < strlist_count(list); z++) {
            const char *left;
            const char *right;
            left = strlist_item(list, z);
            right = expected[z];
            STASIS_ASSERT(strcmp(left, right) == 0, "file content is different than expected");
        }
        STASIS_ASSERT(strcmp_array((const char **) list->data, expected) == 0, "file contents does not match expected values");
        guard_strlist_free(&list);
    }
}

void test_strlist_append_strlist() {
    struct StrList *left;
    struct StrList *right;
    left = strlist_init();
    right = strlist_init();

    strlist_append(&right, "A");
    strlist_append(&right, "B");
    strlist_append(&right, "C");
    strlist_append_strlist(left, right);
    STASIS_ASSERT(strlist_cmp(left, right) == 0, "left and right should be identical");
    strlist_append_strlist(left, right);
    STASIS_ASSERT(strlist_cmp(left, right) == 1, "left and right should be different");
    STASIS_ASSERT(strlist_count(left) > strlist_count(right), "left should be larger than right");
    guard_strlist_free(&left);
    guard_strlist_free(&right);
}

void test_strlist_append_array() {
    const char *data[] = {
            "Appending",
            "Some",
            "Data",
            NULL,
    };
    struct StrList *list;
    list = strlist_init();
    strlist_append_array(list, (char **) data);
    size_t result = 0;
    for (size_t i = 0; i < strlist_count(list); i++) {
        char *item = strlist_item(list, i);
        result += strcmp(item, data[i]) == 0;
    }
    STASIS_ASSERT(result == strlist_count(list), "result is not identical to input");
    guard_strlist_free(&list);
}

void test_strlist_append_tokenize() {
    const char *data = "This is a test.\nWe will split this string\ninto three list records\n";
    size_t line_count = num_chars(data, '\n');
    struct StrList *list;
    list = strlist_init();
    strlist_append_tokenize(list, (char *) data, "\n");
    // note: the trailing '\n' in the data is parsed as a token
    STASIS_ASSERT(line_count + 1 == strlist_count(list), "unexpected number of lines in array");
    int trailing_item_is_empty = isempty(strlist_item(list, strlist_count(list) - 1));
    STASIS_ASSERT(trailing_item_is_empty, "trailing record should be an empty string");
    guard_strlist_free(&list);
}

void test_strlist_copy() {
    struct StrList *list = strlist_init();
    struct StrList *list_copy;

    strlist_append(&list, "Make a copy");
    strlist_append(&list, "of this data");
    strlist_append(&list, "1:1, please");

    STASIS_ASSERT(strlist_copy(NULL) == NULL, "NULL argument should return NULL");

    list_copy = strlist_copy(list);
    STASIS_ASSERT(strlist_cmp(list, list_copy) == 0, "list was copied incorrectly");
    guard_strlist_free(&list);
    guard_strlist_free(&list_copy);
}

void test_strlist_remove() {
    const char *data[] = {
        "keep this",
        "remove this",
        "keep this",
        "remove this",
        "keep this",
        "remove this",
    };
    struct StrList *list;
    list = strlist_init();
    size_t data_size = sizeof(data) / sizeof(*data);
    for (size_t i = 0; i < data_size; i++) {
        strlist_append(&list, (char *) data[i]);
    }

    size_t len_before = strlist_count(list);
    size_t removed = 0;
    for (size_t i = 0; i < len_before; i++) {
        char *item = strlist_item(list, i);
        if (startswith(item, "remove")) {
            strlist_remove(list, i);
            removed++;
        }
    }
    size_t len_after = strlist_count(list);
    STASIS_ASSERT(len_before == data_size, "list length before modification is incorrect. new records?");
    STASIS_ASSERT(len_after == (len_before - removed), "list length after modification is incorrect. not enough records removed.");
    
    guard_strlist_free(&list);
}

void test_strlist_cmp() {
    struct StrList *left;
    struct StrList *right;

    left = strlist_init();
    right = strlist_init();
    STASIS_ASSERT(strlist_cmp(NULL, NULL) == -1, "NULL first argument should return -1");
    STASIS_ASSERT(strlist_cmp(left, NULL) == -2, "NULL second argument should return -2");
    STASIS_ASSERT(strlist_cmp(left, right) == 0, "should be the same");
    strlist_append(&left, "left");
    STASIS_ASSERT(strlist_cmp(left, right) == 1, "should return 1");
    strlist_append(&right, "right");
    STASIS_ASSERT(strlist_cmp(left, right) == 1, "should return 1");
    strlist_append(&left, "left again");
    strlist_append(&left, "left again");
    strlist_append(&left, "left again");
    STASIS_ASSERT(strlist_cmp(left, right) == 1, "should return 1");
    STASIS_ASSERT(strlist_cmp(right, left) == 1, "should return 1");
    guard_strlist_free(&left);
    guard_strlist_free(&right);
}

void test_strlist_reverse() {
    const char *data[] = {
        "a", "b", "c", "d", NULL
    };
    const char *expected[] = {
        "d", "c", "b", "a", NULL
    };
    struct StrList *list;
    list = strlist_init();
    strlist_append_array(list, (char **) data);

    strlist_reverse(list);
    int result = 0;
    for (size_t i = 0; i < strlist_count(list); i++) {
        char *item = strlist_item(list, i);
        result += strcmp(item, expected[i]) == 0;
    }
    STASIS_ASSERT(result == (int) strlist_count(list), "alphabetic sort failed");
    guard_strlist_free(&list);
}

void test_strlist_count() {
    struct StrList *list;
    list = strlist_init();
    strlist_append(&list, "abc");
    strlist_append(&list, "123");
    strlist_append(&list, "def");
    strlist_append(&list, "456");
    STASIS_ASSERT(strlist_count(NULL) == 0, "NULL input should produce a zero result");
    STASIS_ASSERT(strlist_count(list) == 4, "incorrect record count");

    guard_strlist_free(&list);
}

void test_strlist_sort_alphabetic() {
    const char *data[] = {
        "b", "c", "d", "a", NULL
    };
    const char *expected[] = {
        "a", "b", "c", "d", NULL
    };
    struct StrList *list;
    list = strlist_init();
    strlist_append_array(list, (char **) data);

    strlist_sort(list, STASIS_SORT_ALPHA);
    int result = 0;
    for (size_t i = 0; i < strlist_count(list); i++) {
        char *item = strlist_item(list, i);
        result += strcmp(item, expected[i]) == 0;
    }
    STASIS_ASSERT(result == (int) strlist_count(list), "alphabetic sort failed");
    guard_strlist_free(&list);
}

void test_strlist_sort_len_ascending() {
    const char *data[] = {
        "bb", "ccc", "dddd", "a", NULL
    };
    const char *expected[] = {
        "a", "bb", "ccc", "dddd", NULL
    };
    struct StrList *list;
    list = strlist_init();
    strlist_append_array(list, (char **) data);

    strlist_sort(list, STASIS_SORT_LEN_ASCENDING);
    int result = 0;
    for (size_t i = 0; i < strlist_count(list); i++) {
        char *item = strlist_item(list, i);
        result += strcmp(item, expected[i]) == 0;
    }
    STASIS_ASSERT(result == (int) strlist_count(list), "ascending sort failed");
    guard_strlist_free(&list);
}

void test_strlist_sort_len_descending() {
    const char *data[] = {
        "bb", "ccc", "dddd", "a", NULL
    };
    const char *expected[] = {
        "dddd", "ccc", "bb", "a", NULL
    };
    struct StrList *list;
    list = strlist_init();
    strlist_append_array(list, (char **) data);

    strlist_sort(list, STASIS_SORT_LEN_DESCENDING);
    int result = 0;
    for (size_t i = 0; i < strlist_count(list); i++) {
        char *item = strlist_item(list, i);
        result += strcmp(item, expected[i]) == 0;
    }
    STASIS_ASSERT(result == (int) strlist_count(list), "descending sort failed");
    guard_strlist_free(&list);
}

void test_strlist_item_as_str() {
    struct StrList *list;
    list = strlist_init();
    strlist_append(&list, "hello");
    STASIS_ASSERT(strcmp(strlist_item_as_str(list, 0), "hello") == 0, "unexpected string result");
    guard_strlist_free(&list);
}

void test_strlist_item_as_char() {
    char result = 0;
    struct testcase {
        const char *data;
        const char expected;
    };
    struct testcase tc[] = {
            {.data = "-129", .expected = 127}, // rollover
            {.data = "-128", .expected = -128},
            {.data = "-1", .expected = -1},
            {.data = "1", .expected = 1},
            {.data = "127", .expected = 127},
            {.data = "128", .expected = -128}, // rollover
            {.data = "abc", .expected = 0}, // error
    };

    BOILERPLATE_TEST_STRLIST_AS_TYPE(strlist_item_as_char, char);
}

void test_strlist_item_as_uchar() {
    int result;
    struct testcase {
        const char *data;
        const unsigned char expected;
    };
    struct testcase tc[] = {
        {.data = "-1", .expected = 255},
        {.data = "1", .expected = 1},
        {.data = "255", .expected = 255},
        {.data = "abc", .expected = 0}, // error
    };

    BOILERPLATE_TEST_STRLIST_AS_TYPE(strlist_item_as_uchar, unsigned char);
}

void test_strlist_item_as_short() {
    int result;
    struct testcase {
        const char *data;
        const short expected;
    };
    struct testcase tc[] = {
            {.data = "-32769", .expected = 32767}, // rollover
            {.data = "-32768", .expected = -32768},
            {.data = "-1", .expected = -1},
            {.data = "1", .expected = 1},
            {.data = "32767", .expected = 32767},
            {.data = "32768", .expected = -32768}, // rollover
            {.data = "abc", .expected = 0}, // error
    };

    BOILERPLATE_TEST_STRLIST_AS_TYPE(strlist_item_as_short, short);
}

void test_strlist_item_as_ushort() {
    int result;
    struct testcase {
        const char *data;
        const unsigned short expected;
    };
    struct testcase tc[] = {
            {.data = "-1", .expected = 65535},
            {.data = "1", .expected = 1},
            {.data = "65535", .expected = 65535},
            {.data = "65536", .expected = 0}, // rollover
            {.data = "abc", .expected = 0}, // error
    };

    BOILERPLATE_TEST_STRLIST_AS_TYPE(strlist_item_as_ushort, unsigned short);
}

// From here on the values are different between architectures. Do very basic tests.
void test_strlist_item_as_int() {
    int result;
    struct testcase {
        const char *data;
        const int expected;
    };
    struct testcase tc[] = {
            {.data = "-1", .expected = -1},
            {.data = "1", .expected = 1},
            {.data = "abc", .expected = 0}, // error
    };

    BOILERPLATE_TEST_STRLIST_AS_TYPE(strlist_item_as_int, int);
}

void test_strlist_item_as_uint() {
    unsigned int result;
    struct testcase {
        const char *data;
        const unsigned int expected;
    };
    struct testcase tc[] = {
            {.data = "-1", .expected = UINT_MAX},
            {.data = "1", .expected = 1},
            {.data = "abc", .expected = 0}, // error
    };

    BOILERPLATE_TEST_STRLIST_AS_TYPE(strlist_item_as_uint, unsigned int);
}

void test_strlist_item_as_long() {
    long result;
    struct testcase {
        const char *data;
        const long expected;
    };
    struct testcase tc[] = {
            {.data = "-1", .expected = -1},
            {.data = "1", .expected = 1},
            {.data = "abc", .expected = 0}, // error
    };

    BOILERPLATE_TEST_STRLIST_AS_TYPE(strlist_item_as_long, long);
}

void test_strlist_item_as_ulong() {
    unsigned long result;
    struct testcase {
        const char *data;
        const unsigned long expected;
    };
    struct testcase tc[] = {
            {.data = "-1", .expected = ULONG_MAX},
            {.data = "1", .expected = 1},
            {.data = "abc", .expected = 0}, // error
    };

    BOILERPLATE_TEST_STRLIST_AS_TYPE(strlist_item_as_ulong, unsigned long);
}

void test_strlist_item_as_long_long() {
    long long result;
    struct testcase {
        const char *data;
        const long long expected;
    };
    struct testcase tc[] = {
            {.data = "-1", .expected = -1},
            {.data = "1", .expected = 1},
            {.data = "abc", .expected = 0}, // error
    };

    BOILERPLATE_TEST_STRLIST_AS_TYPE(strlist_item_as_long_long, long long);
}

void test_strlist_item_as_ulong_long() {
    unsigned long long result;
    struct testcase {
        const char *data;
        const unsigned long long expected;
    };
    struct testcase tc[] = {
            {.data = "-1", .expected = ULLONG_MAX},
            {.data = "1", .expected = 1},
            {.data = "abc", .expected = 0}, // error
    };

    BOILERPLATE_TEST_STRLIST_AS_TYPE(strlist_item_as_ulong_long, unsigned long long);
}

void test_strlist_item_as_float() {
    float result;
    struct testcase {
        const char *data;
        const float expected;
    };
    struct testcase tc[] = {
            {.data = "1.0", .expected = 1.0f},
    };

    BOILERPLATE_TEST_STRLIST_AS_TYPE(strlist_item_as_float, float);
}

void test_strlist_item_as_double() {
    double result;
    struct testcase {
        const char *data;
        const double expected;
    };
    struct testcase tc[] = {
            {.data = "1.0", .expected = 1.0f},
            {.data = "abc", .expected = 0}, // error
    };

    BOILERPLATE_TEST_STRLIST_AS_TYPE(strlist_item_as_float, double);
}

void test_strlist_item_as_long_double() {
    long double result;
    struct testcase {
        const char *data;
        const long double expected;
    };
    struct testcase tc[] = {
            {.data = "1.0", .expected = 1.0f},
            {.data = "abc", .expected = 0}, // error
    };

    BOILERPLATE_TEST_STRLIST_AS_TYPE(strlist_item_as_float, long double);
}

int main(int argc, char *argv[]) {
    STASIS_TEST_BEGIN_MAIN();
    STASIS_TEST_FUNC *tests[] = {
        test_strlist_init,
        test_strlist_free,
        test_strlist_append,
        test_strlist_append_many_records,
        test_strlist_set,
        test_strlist_append_file,
        test_strlist_append_strlist,
        test_strlist_append_tokenize,
        test_strlist_append_array,
        test_strlist_copy,
        test_strlist_remove,
        test_strlist_cmp,
        test_strlist_sort_alphabetic,
        test_strlist_sort_len_ascending,
        test_strlist_sort_len_descending,
        test_strlist_reverse,
        test_strlist_count,
        test_strlist_item_as_str,
        test_strlist_item_as_char,
        test_strlist_item_as_uchar,
        test_strlist_item_as_short,
        test_strlist_item_as_ushort,
        test_strlist_item_as_int,
        test_strlist_item_as_uint,
        test_strlist_item_as_long,
        test_strlist_item_as_ulong,
        test_strlist_item_as_long_long,
        test_strlist_item_as_ulong_long,
        test_strlist_item_as_float,
        test_strlist_item_as_double,
        test_strlist_item_as_long_double,
    };
    STASIS_TEST_RUN(tests);
    STASIS_TEST_END_MAIN();
}
