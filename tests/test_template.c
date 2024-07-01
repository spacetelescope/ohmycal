#include "testing.h"

extern void tpl_reset();
extern struct tpl_item *tpl_pool[];
extern unsigned tpl_pool_used;
extern unsigned tpl_pool_func_used;


static int adder(struct tplfunc_frame *frame, void *result) {
    int a = (int) strtol(frame->argv[0].t_char_ptr, NULL, 10);
    int b = (int) strtol(frame->argv[1].t_char_ptr, NULL, 10);
    char **ptr = (char **) result;
    *ptr = calloc(100, sizeof(*ptr));
    sprintf(*ptr, "%d", a + b);
    return 0;
}

static int subtractor(struct tplfunc_frame *frame, void *result) {
    int a = (int) strtol(frame->argv[0].t_char_ptr, NULL, 10);
    int b = (int) strtol(frame->argv[1].t_char_ptr, NULL, 10);
    char **ptr = (char **) result;
    *ptr = calloc(100, sizeof(*ptr));
    sprintf(*ptr, "%d", a - b);
    return 0;
}

static int multiplier(struct tplfunc_frame *frame, void *result) {
    int a = (int) strtol(frame->argv[0].t_char_ptr, NULL, 10);
    int b = (int) strtol(frame->argv[1].t_char_ptr, NULL, 10);
    char **ptr = (char **) result;
    *ptr = calloc(100, sizeof(*ptr));
    sprintf(*ptr, "%d", a * b);
    return 0;
}

static int divider(struct tplfunc_frame *frame, void *result) {
    int a = (int) strtol(frame->argv[0].t_char_ptr, NULL, 10);
    int b = (int) strtol(frame->argv[1].t_char_ptr, NULL, 10);
    char **ptr = (char **) result;
    *ptr = calloc(100, sizeof(*ptr));
    sprintf(*ptr, "%d", a / b);
    return 0;
}

void test_tpl_workflow() {
    char *data = strdup("Hello world!");
    tpl_reset();
    tpl_register("hello_message", &data);

    STASIS_ASSERT(strcmp(tpl_render("I said, \"{{ hello_message }}\""), "I said, \"Hello world!\"") == 0, "stored value in key is incorrect");
    setenv("HELLO", "Hello environment!", 1);
    STASIS_ASSERT(strcmp(tpl_render("{{ env:HELLO }}"), "Hello environment!") == 0, "environment variable content mismatch");
    unsetenv("HELLO");
    guard_free(data);
}

void test_tpl_register() {
    char *data = strdup("Hello world!");
    tpl_reset();
    unsigned used_before_register = tpl_pool_used;
    tpl_register("hello_message", &data);

    STASIS_ASSERT(tpl_pool_used == (used_before_register + 1), "tpl_register did not increment allocation counter");
    STASIS_ASSERT(tpl_pool[used_before_register] != NULL, "register did not allocate a tpl_item record in the pool");
    free(data);
}

void test_tpl_register_func() {
    tpl_reset();
    struct testcase {
        const char *key;
        int argc;
        void *func;
    };
    struct testcase tc[] = {
            {.key = "add", .argc = 2, .func = &adder},
            {.key = "sub", .argc = 2, .func = &subtractor},
            {.key = "mul", .argc = 2, .func = &multiplier},
            {.key = "div", .argc = 2, .func = &divider},
    };
    tpl_register_func("add", tc[0].func, tc[0].argc, NULL);
    tpl_register_func("sub", tc[1].func, tc[1].argc, NULL);
    tpl_register_func("mul", tc[2].func, tc[2].argc, NULL);
    tpl_register_func("div", tc[3].func, tc[3].argc, NULL);
    STASIS_ASSERT(tpl_pool_func_used == sizeof(tc) / sizeof(*tc), "unexpected function pool used");

    char *result = NULL;
    result = tpl_render("{{ func:add(0,3) }}");
    STASIS_ASSERT(result != NULL && strcmp(result, "3") == 0, "Answer was not 3");
    result = tpl_render("{{ func:add(1,2) }}");
    STASIS_ASSERT(result != NULL && strcmp(result, "3") == 0, "Answer was not 3");
    result = tpl_render("{{ func:sub(6,3) }}");
    STASIS_ASSERT(result != NULL && strcmp(result, "3") == 0, "Answer was not 3");
    result = tpl_render("{{ func:sub(4,1) }}");
    STASIS_ASSERT(result != NULL && strcmp(result, "3") == 0, "Answer was not 3");
    result = tpl_render("{{ func:mul(1,   3) }}");
    STASIS_ASSERT(result != NULL && strcmp(result, "3") == 0, "Answer was not 3");
    result = tpl_render("{{ func:div(6,2) }}");
    STASIS_ASSERT(result != NULL && strcmp(result, "3") == 0, "Answer was not 3");
    result = tpl_render("{{ func:div(3,1) }}");
    STASIS_ASSERT(result != NULL && strcmp(result, "3") == 0, "Answer was not 3");
}

int main(int argc, char *argv[]) {
    STASIS_TEST_BEGIN_MAIN();
    STASIS_TEST_FUNC *tests[] = {
        test_tpl_workflow,
        test_tpl_register_func,
        test_tpl_register,
    };
    STASIS_TEST_RUN(tests);
    STASIS_TEST_END_MAIN();
}