#include "template_func_proto.h"

int get_github_release_notes_tplfunc_entrypoint(void *frame, void *data_out) {
    int result;
    char **output = (char **) data_out;
    struct tplfunc_frame *f = (struct tplfunc_frame *) frame;
    char *api_token = getenv("STASIS_GITHUB_TOKEN");
    if (!api_token) {
        api_token = getenv("GITHUB_TOKEN");
    }
    result = get_github_release_notes(api_token ? api_token : "anonymous",
                                      (const char *) f->argv[0].t_char_ptr,
                                      (const char *) f->argv[1].t_char_ptr,
                                      (const char *) f->argv[2].t_char_ptr,
                                      output);
    return result;
}

int get_github_release_notes_auto_tplfunc_entrypoint(void *frame, void *data_out) {
    int result = 0;
    char **output = (char **) data_out;
    struct tplfunc_frame *f = (struct tplfunc_frame *) frame;
    char *api_token = getenv("STASIS_GITHUB_TOKEN");
    if (!api_token) {
        api_token = getenv("GITHUB_TOKEN");
    }

    const struct Delivery *ctx = (struct Delivery *) f->data_in;
    struct StrList *notes_list = strlist_init();
    for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(*ctx->tests); i++) {
        // Get test context
        const struct Test *test = &ctx->tests[i];
        if (test->name && test->version && test->repository) {
            char *repository = strdup(test->repository);
            char *match = strstr(repository, "spacetelescope/");
            // Cull repository URL
            if (match) {
                replace_text(repository, "https://github.com/", "", 0);
                if (endswith(repository, ".git")) {
                    replace_text(repository, ".git", "", 0);
                }
                // Record release notes for version relative to HEAD
                // Using HEAD, GitHub returns the previous tag
                char *note = NULL;
                char h1_title[NAME_MAX] = {0};
                sprintf(h1_title, "# %s", test->name);
                strlist_append(&notes_list, h1_title);
                result += get_github_release_notes(api_token ? api_token : "anonymous",
                                                   repository,
                                                   test->version,
                                                   "HEAD",
                                                   &note);
                if (note) {
                    strlist_append(&notes_list, note);
                    guard_free(note);
                }
            }
        }
    }
    // Return all notes as a single string
    if (strlist_count(notes_list)) {
        *output = join(notes_list->data, "\n\n");
    }
    return result;
}
