#include "testing.h"

void test_to_short_version() {
    struct testcase {
        const char *data;
        const char *expected;
    };

    struct testcase tc[] = {
            {.data = "1.2.3", .expected = "123"},
            {.data = "py3.12", .expected = "py312"},
            {.data = "generic-1.2.3", .expected = "generic-123"},
            {.data = "nothing to do", .expected = "nothing to do"},
    };

    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        char *result = to_short_version(tc[i].data);
        STASIS_ASSERT_FATAL(result != NULL, "should not be NULL");
        STASIS_ASSERT(strcmp(result, tc[i].expected) == 0, "unexpected result");
        guard_free(result);
    }

}

void test_tolower_s() {
    struct testcase {
        const char *data;
        const char *expected;
    };

    struct testcase tc[] = {
        {.data = "HELLO WORLD", .expected = "hello world"},
        {.data = "Hello World", .expected = "hello world"},
        {.data = "hello world", .expected = "hello world"},
    };

    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        char input[100] = {0};
        strcpy(input, tc[i].data);
        tolower_s(input);
        STASIS_ASSERT(strcmp(input, tc[i].expected) == 0, "unexpected result");
    }
}

void test_isdigit_s() {
    struct testcase {
        const char *data;
        int expected;
    };

    struct testcase tc[] = {
        {.data = "", .expected = false},
        {.data = "1", .expected = true},
        {.data = "1234567890", .expected = true},
        {.data = " 1 ", .expected = false},
        {.data = "a number", .expected = false},
        {.data = "10 numbers", .expected = false},
        {.data = "10 10", .expected = false}
    };

    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        STASIS_ASSERT(isdigit_s(tc[i].data) == tc[i].expected, "unexpected result");
    }
}

void test_strdup_array_and_strcmp_array() {
    struct testcase {
        const char **data;
        const char **expected;
    };
    const struct testcase tc[] = {
        {.data = (const char *[]) {"abc", "123", NULL},
         .expected = (const char *[]) {"abc", "123", NULL}},
        {.data = (const char *[]) {"I", "have", "a", "pencil", "box", NULL},
         .expected = (const char *[]) {"I", "have", "a", "pencil", "box", NULL}},
    };

    STASIS_ASSERT(strdup_array(NULL) == NULL, "returned non-NULL on NULL argument");
    for (size_t outer = 0; outer < sizeof(tc) / sizeof(*tc); outer++) {
        char **result = strdup_array((char **) tc[outer].data);
        STASIS_ASSERT(strcmp_array((const char **) result, tc[outer].expected) == 0, "array members were different");
    }

    const struct testcase tc_bad[] = {
            {.data = (const char *[]) {"ab", "123", NULL},
                    .expected = (const char *[]) {"abc", "123", NULL}},
            {.data = (const char *[]) {"have", "a", "pencil", "box", NULL},
                    .expected = (const char *[]) {"I", "have", "a", "pencil", "box", NULL}},
    };

    STASIS_ASSERT(strcmp_array(tc[0].data, NULL) > 0, "right argument is NULL, expected positive return");
    STASIS_ASSERT(strcmp_array(NULL, tc[0].data) < 0, "left argument is NULL, expected negative return");
    STASIS_ASSERT(strcmp_array(NULL, NULL) == 0, "left and right arguments are NULL, expected zero return");
    for (size_t outer = 0; outer < sizeof(tc_bad) / sizeof(*tc_bad); outer++) {
        char **result = strdup_array((char **) tc_bad[outer].data);
        STASIS_ASSERT(strcmp_array((const char **) result, tc_bad[outer].expected) != 0, "array members were identical");
    }
}

void test_strchrdel() {
    struct testcase {
        const char *data;
        const char *input;
        const char *expected;
    };
    const struct testcase tc[] = {
            {.data ="aaaabbbbcccc", .input = "ac", .expected = "bbbb"},
            {.data = "1I 2have 3a 4pencil 5box.", .input = "1245", .expected = "I have 3a pencil box."},
            {.data = "\v\v\vI\t\f  ha\tve   a\t    pen\tcil     b\tox.", .input = " \f\t\v", "Ihaveapencilbox."},
    };

    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        char *data = strdup(tc[i].data);
        strchrdel(data, tc[i].input);
        STASIS_ASSERT(strcmp(data, tc[i].expected) == 0, "wrong status for condition");
        guard_free(data);
    }
}

void test_endswith() {
    struct testcase {
        const char *data;
        const char *input;
        const int expected;
    };
    struct testcase tc[] = {
            {.data = "I have a pencil box.", .input = ".", .expected = true},
            {.data = "I have a pencil box.", .input = "box.", .expected = true},
            {.data = "I have a pencil box.", .input = "pencil box.", .expected = true},
            {.data = "I have a pencil box.", .input = "a pencil box.", .expected = true},
            {.data = "I have a pencil box.", .input = "have a pencil box.", .expected = true},
            {.data = "I have a pencil box.", .input = "I have a pencil box.", .expected = true},
            {.data = ".    ", .input = ".", .expected = false},
            {.data = "I have a pencil box.", .input = "pencil box", .expected = false},
            {.data = NULL, .input = "test", .expected = false},
            {.data = "I have a pencil box", .input = NULL, .expected = false},
            {.data = NULL, .input = NULL, .expected = false},
    };
    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        int result = endswith(tc[i].data, tc[i].input);
        STASIS_ASSERT(result == tc[i].expected, "wrong status for condition");
    }
}

void test_startswith() {
    struct testcase {
        const char *data;
        const char *input;
        const int expected;
    };
    struct testcase tc[] = {
            {.data = "I have a pencil box.", .input = "I", .expected = true},
            {.data = "I have a pencil box.", .input = "I have", .expected = true},
            {.data = "I have a pencil box.", .input = "I have a", .expected = true},
            {.data = "I have a pencil box.", .input = "I have a pencil", .expected = true},
            {.data = "I have a pencil box.", .input = "I have a pencil box", .expected = true},
            {.data = "I have a pencil box.", .input = "I have a pencil box.", .expected = true},
            {.data = "    I have a pencil box.", .input = "I have", .expected = false},
            {.data = "I have a pencil box.", .input = "pencil box", .expected = false},
            {.data = NULL, .input = "test", .expected = false},
            {.data = "I have a pencil box", .input = NULL, .expected = false},
            {.data = NULL, .input = NULL, .expected = false},
    };
    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        int result = startswith(tc[i].data, tc[i].input);
        STASIS_ASSERT(result == tc[i].expected, "wrong status for condition");
    }
}

void test_num_chars() {
    struct testcase {
        const char *data;
        const char input;
        const int expected;
    };
    struct testcase tc[] = {
            {.data = "         ", .input = ' ', .expected = 9},
            {.data = "1 1 1 1 1", .input = '1', .expected = 5},
            {.data = "abc\t\ndef\nabc\ndef\n", .input = 'c', .expected = 2},
            {.data = "abc\t\ndef\nabc\ndef\n", .input = '\t', .expected = 1},
    };
    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        STASIS_ASSERT(num_chars(tc[i].data, tc[i].input) == tc[i].expected, "incorrect number of characters detected");
    }
}

void test_split() {
    struct testcase {
            const char *data;
            const size_t max_split;
            const char *delim;
            const char **expected;
    };
    struct testcase tc[] = {
            {.data = "a/b/c/d/e", .delim = "/", .max_split = 0, .expected = (const char *[]) {"a", "b", "c", "d", "e", NULL}},
            {.data = "a/b/c/d/e", .delim = "/", .max_split = 1, .expected = (const char *[]) {"a", "b/c/d/e", NULL}},
            {.data = "a/b/c/d/e", .delim = "/", .max_split = 2, .expected = (const char *[]) {"a", "b", "c/d/e", NULL}},
            {.data = "a/b/c/d/e", .delim = "/", .max_split = 3, .expected = (const char *[]) {"a", "b", "c", "d/e", NULL}},
            {.data = "a/b/c/d/e", .delim = "/", .max_split = 4, .expected = (const char *[]) {"a", "b", "c", "d", "e", NULL}},
            {.data = "multiple words split n times", .delim = " ", .max_split = 0, .expected = (const char *[]) {"multiple", "words", "split", "n", "times", NULL}},
            {.data = "multiple words split n times", .delim = " ", .max_split = 1, .expected = (const char *[]) {"multiple", "words split n times", NULL}},
            {.data = NULL, .delim = NULL, NULL},
    };
    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        char **result;
        result = split(tc[i].data, tc[i].delim, tc[i].max_split);
        STASIS_ASSERT(strcmp_array((const char **) result, tc[i].expected) == 0, "Split failed");
        GENERIC_ARRAY_FREE(result);
    }
}

void test_join() {
    struct testcase {
        const char **data;
        const char *delim;
        const char *expected;
    };
    struct testcase tc[] = {
            {.data = (const char *[]) {"a", "b", "c", "d", "e", NULL}, .delim = "", .expected = "abcde"},
            {.data = (const char *[]) {"a", NULL}, .delim = "", .expected = "a"},
            {.data = (const char *[]) {"a", "word", "or", "two", NULL}, .delim = "/", .expected = "a/word/or/two"},
            {.data = NULL, .delim = NULL, .expected = ""},
    };
    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        char *result;
        result = join((char **) tc[i].data, tc[i].delim);
        STASIS_ASSERT(strcmp(result ? result : "", tc[i].expected) == 0, "failed to join array");
        guard_free(result);
    }
}

void test_join_ex() {
    struct testcase {
        const char *delim;
        const char *expected;
    };
    struct testcase tc[] = {
            {.delim = "", .expected = "abcde"},
            {.delim = "/", .expected = "a/b/c/d/e"},
            {.delim = "\n", .expected = "a\nb\nc\nd\ne"},
            {.delim = "\n\n", .expected = "a\n\nb\n\nc\n\nd\n\ne"},
    };
    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        char *result;
        result = join_ex(tc[i].delim, "a", "b", "c", "d", "e", NULL);
        STASIS_ASSERT(strcmp(result ? result : "", tc[i].expected) == 0, "failed to join array");
        guard_free(result);
    }
}

void test_substring_between() {
    struct testcase {
        const char *data;
        const char *delim;
        const char *expected;
    };
    struct testcase tc[] = {
            {.data = "I like {{adjective}} spaceships", .delim = "{{}}", .expected = "adjective"},
            {.data = "I like {adjective} spaceships", .delim = "{}", .expected = "adjective"},
            {.data = "one = 'two'", .delim = "''", .expected = "two"},
            {.data = "I like {adjective> spaceships", .delim = "{}", .expected = ""}, // no match
            {.data = NULL, .delim = NULL, .expected = ""}, // both arguments NULL
            {.data = "nothing!", .delim = NULL, .expected = ""}, // null delim
            {.data = "", .delim = "{}", .expected = ""}, // no match
            {.data = NULL, .delim = "nothing!", .expected = ""}, // null sptr
            {.data = "()", .delim = "(", .expected = ""}, // delim not divisible by 2
            {.data = "()", .delim = "( )", .expected = ""}, // delim not divisible by 2
            {.data = "nothing () here", .delim = "()", .expected = ""}, // nothing exists between delimiters
    };
    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        char *result = substring_between(tc[i].data, tc[i].delim);
        STASIS_ASSERT(strcmp(result ? result : "", tc[i].expected) == 0, "unable to extract substring");
        guard_free(result);
    }
}

void test_strdeldup() {
    struct testcase {
        char **data;
        const char **expected;
    };
    struct testcase tc[] = {
        {.data = NULL, .expected = NULL},
        {.data = (char *[]) {"a", "a", "a", "b", "b", "b", "c", "c", "c", NULL}, .expected = (const char *[]) {"a", "b", "c", NULL}},
        {.data = (char *[]) {"a", "b", "c", "a", "b", "c", "a", "b", "c", NULL}, .expected = (const char *[]) {"a", "b", "c", NULL}},
        {.data = (char *[]) {"apple", "banana", "coconut", NULL}, .expected = (const char *[]) {"apple", "banana", "coconut", NULL}},
        {.data = (char *[]) {"apple", "banana", "apple", "coconut", NULL}, .expected = (const char *[]) {"apple", "banana", "coconut", NULL}},
    };
    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        char **result = strdeldup(tc[i].data);
        STASIS_ASSERT(strcmp_array((const char **) result, tc[i].expected) == 0, "incorrect number of duplicates removed");
        GENERIC_ARRAY_FREE(result);
    }
}

void test_strsort() {

}

void test_strstr_array() {

}

void test_lstrip() {
    struct testcase {
        const char *data;
        const char *expected;
    };
    struct testcase tc[] = {
        {.data = "I am a string.\n", .expected = "I am a string.\n"}, // left strip only
        {.data = "I am a string.\n", .expected = "I am a string.\n"},
        {.data = "\t\t\t\tI am a string.\n", .expected = "I am a string.\n"},
        {.data = "    I    am a string.\n", .expected = "I    am a string.\n"},
    };

    STASIS_ASSERT(lstrip(NULL) == NULL, "incorrect return type");
    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        char *buf = calloc(255, sizeof(*buf));
        char *result;
        strcpy(buf, tc[i].data);
        result = lstrip(buf);
        STASIS_ASSERT(strcmp(result ? result : "", tc[i].expected) == 0, "incorrect strip-from-left");
        guard_free(buf);
    }
}

void test_strip() {
    struct testcase {
        const char *data;
        const char *expected;
    };
    struct testcase tc[] = {
            {.data = "  I am a string.\n", .expected = "  I am a string."}, // right strip only
            {.data = "\tI am a string.\n\t", .expected = "\tI am a string."},
            {.data = "\t\t\t\tI am a string.\n", .expected = "\t\t\t\tI am a string."},
            {.data = "I am a string.\n\n\n\n", .expected = "I am a string."},
            {.data = "", .expected = ""},
            {.data = " ", .expected = ""},
    };

    STASIS_ASSERT(strip(NULL) == NULL, "incorrect return type");
    for (size_t i = 0; i < sizeof(tc) / sizeof(*tc); i++) {
        char *buf = calloc(255, sizeof(*buf));
        char *result;
        strcpy(buf, tc[i].data);
        result = strip(buf);
        STASIS_ASSERT(strcmp(result ? result : "", tc[i].expected) == 0, "incorrect strip-from-right");
        guard_free(buf);
    }
}

void test_isempty() {

}

int main(int argc, char *argv[]) {
    STASIS_TEST_BEGIN_MAIN();
    STASIS_TEST_FUNC *tests[] = {
            test_to_short_version,
            test_tolower_s,
            test_isdigit_s,
            test_strdup_array_and_strcmp_array,
            test_strchrdel,
            test_strdeldup,
            test_lstrip,
            test_strip,
            test_num_chars,
            test_startswith,
            test_endswith,
            test_split,
            test_join,
            test_join_ex,
            test_substring_between,
    };
    STASIS_TEST_RUN(tests);
    STASIS_TEST_END_MAIN();
}
