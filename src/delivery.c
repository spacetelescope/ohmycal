#define _GNU_SOURCE

#include <fnmatch.h>
#include "core.h"

extern struct STASIS_GLOBAL globals;

static void ini_has_key_required(struct INIFILE *ini, const char *section_name, char *key) {
    int status = ini_has_key(ini, section_name, key);
    if (!status) {
        SYSERROR("%s:%s key is required but not defined", section_name, key);
        exit(1);
    }
}

static void ini_getval_required(struct INIFILE *ini, char *section_name, char *key, unsigned type, union INIVal *val) {
    int status = ini_getval(ini, section_name, key, type, val);
    if (status || isempty(val->as_char_p)) {
        SYSERROR("%s:%s value is required but not defined", section_name, key);
        exit(1);
    }
}

static void conv_int(int *x, union INIVal val) {
    *x = val.as_int;
}

static void conv_str(char **x, union INIVal val) {
    if (*x) {
        guard_free(*x);
    }
    if (val.as_char_p) {
        char *tplop = tpl_render(val.as_char_p);
        if (tplop) {
            *x = tplop;
        } else {
            *x = NULL;
        }
    } else {
        *x = NULL;
    }
}

static void conv_str_noexpand(char **x, union INIVal val) {
    if (*x) {
        guard_free(*x);
    }
    *x = strdup(val.as_char_p);
}

static void conv_strlist(struct StrList **x, char *tok, union INIVal val) {
    if (!(*x))
        (*x) = strlist_init();
    if (val.as_char_p) {
        char *tplop = tpl_render(val.as_char_p);
        if (tplop) {
            strip(tplop);
            strlist_append_tokenize((*x), tplop, tok);
            guard_free(tplop);
        }
    }
}

static void conv_bool(bool *x, union INIVal val) {
    *x = val.as_bool;
}

int delivery_init_tmpdir(struct Delivery *ctx) {
    char *tmpdir = NULL;
    char *x = NULL;
    int unusable = 0;
    errno = 0;

    x = getenv("TMPDIR");
    if (x) {
        guard_free(ctx->storage.tmpdir);
        tmpdir = strdup(x);
    } else {
        tmpdir = ctx->storage.tmpdir;
    }

    if (!tmpdir) {
        // memory error
        return -1;
    }

    // If the directory doesn't exist, create it
    if (access(tmpdir, F_OK) < 0) {
        if (mkdirs(tmpdir, 0755) < 0) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L1, "Unable to create temporary storage directory: %s (%s)\n", tmpdir, strerror(errno));
            goto l_delivery_init_tmpdir_fatal;
        }
    }

    // If we can't read, write, or execute, then die
    if (access(tmpdir, R_OK | W_OK | X_OK) < 0) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L1, "%s requires at least 0755 permissions.\n");
        goto l_delivery_init_tmpdir_fatal;
    }

    struct statvfs st;
    if (statvfs(tmpdir, &st) < 0) {
        goto l_delivery_init_tmpdir_fatal;
    }

#if defined(STASIS_OS_LINUX)
    // If we can't execute programs, or write data to the file system at all, then die
    if ((st.f_flag & ST_NOEXEC) != 0) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L1, "%s is mounted with noexec\n", tmpdir);
        goto l_delivery_init_tmpdir_fatal;
    }
#endif
    if ((st.f_flag & ST_RDONLY) != 0) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L1, "%s is mounted read-only\n", tmpdir);
        goto l_delivery_init_tmpdir_fatal;
    }

    if (!globals.tmpdir) {
        globals.tmpdir = strdup(tmpdir);
    }

    if (!ctx->storage.tmpdir) {
        ctx->storage.tmpdir = strdup(globals.tmpdir);
    }
    return unusable;

    l_delivery_init_tmpdir_fatal:
    unusable = 1;
    return unusable;
}

void delivery_free(struct Delivery *ctx) {
    guard_free(ctx->system.arch);
    GENERIC_ARRAY_FREE(ctx->system.platform);
    guard_free(ctx->meta.name);
    guard_free(ctx->meta.version);
    guard_free(ctx->meta.codename);
    guard_free(ctx->meta.mission);
    guard_free(ctx->meta.python);
    guard_free(ctx->meta.mission);
    guard_free(ctx->meta.python_compact);
    guard_free(ctx->meta.based_on);
    guard_runtime_free(ctx->runtime.environ);
    guard_free(ctx->storage.root);
    guard_free(ctx->storage.tmpdir);
    guard_free(ctx->storage.delivery_dir);
    guard_free(ctx->storage.tools_dir);
    guard_free(ctx->storage.package_dir);
    guard_free(ctx->storage.results_dir);
    guard_free(ctx->storage.output_dir);
    guard_free(ctx->storage.conda_install_prefix);
    guard_free(ctx->storage.conda_artifact_dir);
    guard_free(ctx->storage.conda_staging_dir);
    guard_free(ctx->storage.conda_staging_url);
    guard_free(ctx->storage.wheel_artifact_dir);
    guard_free(ctx->storage.wheel_staging_dir);
    guard_free(ctx->storage.wheel_staging_url);
    guard_free(ctx->storage.build_dir);
    guard_free(ctx->storage.build_recipes_dir);
    guard_free(ctx->storage.build_sources_dir);
    guard_free(ctx->storage.build_testing_dir);
    guard_free(ctx->storage.build_docker_dir);
    guard_free(ctx->storage.mission_dir);
    guard_free(ctx->storage.docker_artifact_dir);
    guard_free(ctx->storage.meta_dir);
    guard_free(ctx->storage.package_dir);
    guard_free(ctx->storage.cfgdump_dir);
    guard_free(ctx->info.time_str_epoch);
    guard_free(ctx->info.build_name);
    guard_free(ctx->info.build_number);
    guard_free(ctx->info.release_name);
    guard_free(ctx->conda.installer_baseurl);
    guard_free(ctx->conda.installer_name);
    guard_free(ctx->conda.installer_version);
    guard_free(ctx->conda.installer_platform);
    guard_free(ctx->conda.installer_arch);
    guard_free(ctx->conda.installer_path);
    guard_free(ctx->conda.tool_version);
    guard_free(ctx->conda.tool_build_version);
    guard_strlist_free(&ctx->conda.conda_packages);
    guard_strlist_free(&ctx->conda.conda_packages_defer);
    guard_strlist_free(&ctx->conda.pip_packages);
    guard_strlist_free(&ctx->conda.pip_packages_defer);
    guard_strlist_free(&ctx->conda.wheels_packages);

    for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
        guard_free(ctx->tests[i].name);
        guard_free(ctx->tests[i].version);
        guard_free(ctx->tests[i].repository);
        guard_free(ctx->tests[i].repository_info_ref);
        guard_free(ctx->tests[i].repository_info_tag);
        guard_strlist_free(&ctx->tests[i].repository_remove_tags);
        guard_free(ctx->tests[i].script);
        guard_free(ctx->tests[i].build_recipe);
        // test-specific runtime variables
        guard_runtime_free(ctx->tests[i].runtime.environ);
    }

    guard_free(ctx->rules.release_fmt);
    guard_free(ctx->rules.build_name_fmt);
    guard_free(ctx->rules.build_number_fmt);

    guard_free(ctx->deploy.docker.test_script);
    guard_free(ctx->deploy.docker.registry);
    guard_free(ctx->deploy.docker.image_compression);
    guard_strlist_free(&ctx->deploy.docker.tags);
    guard_strlist_free(&ctx->deploy.docker.build_args);

    for (size_t i = 0; i < sizeof(ctx->deploy.jfrog) / sizeof(ctx->deploy.jfrog[0]); i++) {
        guard_free(ctx->deploy.jfrog[i].repo);
        guard_free(ctx->deploy.jfrog[i].dest);
        guard_strlist_free(&ctx->deploy.jfrog[i].files);
    }

    if (ctx->_stasis_ini_fp.delivery) {
        ini_free(&ctx->_stasis_ini_fp.delivery);
    }
    guard_free(ctx->_stasis_ini_fp.delivery_path);

    if (ctx->_stasis_ini_fp.cfg) {
        // optional extras
        ini_free(&ctx->_stasis_ini_fp.cfg);
    }
    guard_free(ctx->_stasis_ini_fp.cfg_path);

    if (ctx->_stasis_ini_fp.mission) {
        ini_free(&ctx->_stasis_ini_fp.mission);
    }
    guard_free(ctx->_stasis_ini_fp.mission_path);
}

void delivery_init_dirs_stage2(struct Delivery *ctx) {
    path_store(&ctx->storage.build_recipes_dir, PATH_MAX, ctx->storage.build_dir, "recipes");
    path_store(&ctx->storage.build_sources_dir, PATH_MAX, ctx->storage.build_dir, "sources");
    path_store(&ctx->storage.build_testing_dir, PATH_MAX, ctx->storage.build_dir, "testing");
    path_store(&ctx->storage.build_docker_dir, PATH_MAX, ctx->storage.build_dir, "docker");

    path_store(&ctx->storage.delivery_dir, PATH_MAX, ctx->storage.output_dir, "delivery");
    path_store(&ctx->storage.results_dir, PATH_MAX, ctx->storage.output_dir, "results");
    path_store(&ctx->storage.package_dir, PATH_MAX, ctx->storage.output_dir, "packages");
    path_store(&ctx->storage.cfgdump_dir, PATH_MAX, ctx->storage.output_dir, "config");
    path_store(&ctx->storage.meta_dir, PATH_MAX, ctx->storage.output_dir, "meta");

    path_store(&ctx->storage.conda_artifact_dir, PATH_MAX, ctx->storage.package_dir, "conda");
    path_store(&ctx->storage.wheel_artifact_dir, PATH_MAX, ctx->storage.package_dir, "wheels");
    path_store(&ctx->storage.docker_artifact_dir, PATH_MAX, ctx->storage.package_dir, "docker");
}

void delivery_init_dirs_stage1(struct Delivery *ctx) {
    char *rootdir = getenv("STASIS_ROOT");
    if (rootdir) {
        if (isempty(rootdir)) {
            fprintf(stderr, "STASIS_ROOT is set, but empty. Please assign a file system path to this environment variable.\n");
            exit(1);
        }
        path_store(&ctx->storage.root, PATH_MAX, rootdir, ctx->info.build_name);
    } else {
        // use "stasis" in current working directory
        path_store(&ctx->storage.root, PATH_MAX, "stasis", ctx->info.build_name);
    }
    path_store(&ctx->storage.tools_dir, PATH_MAX, ctx->storage.root, "tools");
    path_store(&ctx->storage.tmpdir, PATH_MAX, ctx->storage.root, "tmp");
    if (delivery_init_tmpdir(ctx)) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L1, "Set $TMPDIR to a location other than %s\n", globals.tmpdir);
        if (globals.tmpdir)
            guard_free(globals.tmpdir);
        exit(1);
    }

    path_store(&ctx->storage.build_dir, PATH_MAX, ctx->storage.root, "build");
    path_store(&ctx->storage.output_dir, PATH_MAX, ctx->storage.root, "output");

    if (!ctx->storage.mission_dir) {
        path_store(&ctx->storage.mission_dir, PATH_MAX, globals.sysconfdir, "mission");
    }

    if (access(ctx->storage.mission_dir, F_OK)) {
        msg(STASIS_MSG_L1, "%s: %s\n", ctx->storage.mission_dir, strerror(errno));
        exit(1);
    }

    // Override installation prefix using global configuration key
    if (globals.conda_install_prefix && strlen(globals.conda_install_prefix)) {
        // user wants a specific path
        globals.conda_fresh_start = false;
        /*
        if (mkdirs(globals.conda_install_prefix, 0755)) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L1, "Unable to create directory: %s: %s\n",
                strerror(errno), globals.conda_install_prefix);
            exit(1);
        }
         */
        /*
        ctx->storage.conda_install_prefix = realpath(globals.conda_install_prefix, NULL);
        if (!ctx->storage.conda_install_prefix) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L1, "realpath(): Conda installation prefix reassignment failed\n");
            exit(1);
        }
        ctx->storage.conda_install_prefix = strdup(globals.conda_install_prefix);
         */
        path_store(&ctx->storage.conda_install_prefix, PATH_MAX, globals.conda_install_prefix, "conda");
    } else {
        // install conda under the STASIS tree
        path_store(&ctx->storage.conda_install_prefix, PATH_MAX, ctx->storage.tools_dir, "conda");
    }
}

int delivery_init_platform(struct Delivery *ctx) {
    msg(STASIS_MSG_L2, "Setting architecture\n");
    char archsuffix[20];
    struct utsname uts;
    if (uname(&uts)) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "uname() failed: %s\n", strerror(errno));
        return -1;
    }

    ctx->system.platform = calloc(DELIVERY_PLATFORM_MAX + 1, sizeof(*ctx->system.platform));
    if (!ctx->system.platform) {
        SYSERROR("Unable to allocate %d records for platform array\n", DELIVERY_PLATFORM_MAX);
        return -1;
    }
    for (size_t i = 0; i < DELIVERY_PLATFORM_MAX; i++) {
        ctx->system.platform[i] = calloc(DELIVERY_PLATFORM_MAXLEN, sizeof(*ctx->system.platform[0]));
    }

    ctx->system.arch = strdup(uts.machine);
    if (!ctx->system.arch) {
        // memory error
        return -1;
    }

    if (!strcmp(ctx->system.arch, "x86_64")) {
        strcpy(archsuffix, "64");
    } else {
        strcpy(archsuffix, ctx->system.arch);
    }

    msg(STASIS_MSG_L2, "Setting platform\n");
    strcpy(ctx->system.platform[DELIVERY_PLATFORM], uts.sysname);
    if (!strcmp(ctx->system.platform[DELIVERY_PLATFORM], "Darwin")) {
        sprintf(ctx->system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR], "osx-%s", archsuffix);
        strcpy(ctx->system.platform[DELIVERY_PLATFORM_CONDA_INSTALLER], "MacOSX");
        strcpy(ctx->system.platform[DELIVERY_PLATFORM_RELEASE], "macos");
    } else if (!strcmp(ctx->system.platform[DELIVERY_PLATFORM], "Linux")) {
        sprintf(ctx->system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR], "linux-%s", archsuffix);
        strcpy(ctx->system.platform[DELIVERY_PLATFORM_CONDA_INSTALLER], "Linux");
        strcpy(ctx->system.platform[DELIVERY_PLATFORM_RELEASE], "linux");
    } else {
        // Not explicitly supported systems
        strcpy(ctx->system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR], ctx->system.platform[DELIVERY_PLATFORM]);
        strcpy(ctx->system.platform[DELIVERY_PLATFORM_CONDA_INSTALLER], ctx->system.platform[DELIVERY_PLATFORM]);
        strcpy(ctx->system.platform[DELIVERY_PLATFORM_RELEASE], ctx->system.platform[DELIVERY_PLATFORM]);
        tolower_s(ctx->system.platform[DELIVERY_PLATFORM_RELEASE]);
    }

    long cpu_count = get_cpu_count();
    if (!cpu_count) {
        fprintf(stderr, "Unable to determine CPU count. Falling back to 1.\n");
        cpu_count = 1;
    }
    char ncpus[100] = {0};
    sprintf(ncpus, "%ld", cpu_count);

    // Declare some important bits as environment variables
    setenv("CPU_COUNT", ncpus, 1);
    setenv("STASIS_CPU_COUNT", ncpus, 1);
    setenv("STASIS_ARCH", ctx->system.arch, 1);
    setenv("STASIS_PLATFORM", ctx->system.platform[DELIVERY_PLATFORM], 1);
    setenv("STASIS_CONDA_ARCH", ctx->system.arch, 1);
    setenv("STASIS_CONDA_PLATFORM", ctx->system.platform[DELIVERY_PLATFORM_CONDA_INSTALLER], 1);
    setenv("STASIS_CONDA_PLATFORM_SUBDIR", ctx->system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR], 1);

    // Register template variables
    // These were moved out of main() because we can't take the address of system.platform[x]
    // _before_ the array has been initialized.
    tpl_register("system.arch", &ctx->system.arch);
    tpl_register("system.platform", &ctx->system.platform[DELIVERY_PLATFORM_RELEASE]);

    return 0;
}

static int populate_mission_ini(struct Delivery **ctx) {
    union INIVal val;
    struct INIFILE *ini;

    if ((*ctx)->_stasis_ini_fp.mission) {
        return 0;
    }

    // Now populate the rules
    char missionfile[PATH_MAX] = {0};
    if (getenv("STASIS_SYSCONFDIR")) {
        sprintf(missionfile, "%s/%s/%s/%s.ini",
                getenv("STASIS_SYSCONFDIR"), "mission", (*ctx)->meta.mission, (*ctx)->meta.mission);
    } else {
        sprintf(missionfile, "%s/%s/%s/%s.ini",
                globals.sysconfdir, "mission", (*ctx)->meta.mission, (*ctx)->meta.mission);
    }

    msg(STASIS_MSG_L2, "Reading mission configuration: %s\n", missionfile);
    (*ctx)->_stasis_ini_fp.mission = ini_open(missionfile);
    ini = (*ctx)->_stasis_ini_fp.mission;
    if (!ini) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "Failed to read misson configuration: %s, %s\n", missionfile, strerror(errno));
        exit(1);
    }
    (*ctx)->_stasis_ini_fp.mission_path = strdup(missionfile);

    ini_getval_required(ini, "meta", "release_fmt", INIVAL_TYPE_STR, &val);
    conv_str(&(*ctx)->rules.release_fmt, val);

    // Used for setting artifactory build info
    ini_getval_required(ini, "meta", "build_name_fmt", INIVAL_TYPE_STR, &val);
    conv_str(&(*ctx)->rules.build_name_fmt, val);

    // Used for setting artifactory build info
    ini_getval_required(ini, "meta", "build_number_fmt", INIVAL_TYPE_STR, &val);
    conv_str(&(*ctx)->rules.build_number_fmt, val);
    return 0;
}

void validate_delivery_ini(struct INIFILE *ini) {
    if (ini_section_search(&ini, INI_SEARCH_EXACT, "meta")) {
        ini_has_key_required(ini, "meta", "name");
        ini_has_key_required(ini, "meta", "version");
        ini_has_key_required(ini, "meta", "rc");
        ini_has_key_required(ini, "meta", "mission");
        ini_has_key_required(ini, "meta", "python");
    } else {
        SYSERROR("%s", "[meta] configuration section is required");
        exit(1);
    }

    if (ini_section_search(&ini, INI_SEARCH_EXACT, "conda")) {
        ini_has_key_required(ini, "conda", "installer_name");
        ini_has_key_required(ini, "conda", "installer_version");
        ini_has_key_required(ini, "conda", "installer_platform");
        ini_has_key_required(ini, "conda", "installer_arch");
    } else {
        SYSERROR("%s", "[conda] configuration section is required");
        exit(1);
    }

    for (size_t i = 0; i < ini->section_count; i++) {
        struct INISection *section = ini->section[i];
        if (section && startswith(section->key, "test:")) {
            char *name = strstr(section->key, ":");
            if (name && strlen(name) > 1) {
                name = &name[1];
            }
            ini_has_key_required(ini, section->key, "version");
            ini_has_key_required(ini, section->key, "repository");
            ini_has_key_required(ini, section->key, "script");
        }
    }

    if (ini_section_search(&ini, INI_SEARCH_EXACT, "deploy:docker")) {
        // yeah?
    }

    for (size_t i = 0; i < ini->section_count; i++) {
        struct INISection *section = ini->section[i];
        if (section && startswith(section->key, "deploy:artifactory")) {
            ini_has_key_required(ini, section->key, "files");
            ini_has_key_required(ini, section->key, "dest");
        }
    }
}

static int populate_delivery_ini(struct Delivery *ctx) {
    union INIVal val;
    struct INIFILE *ini = ctx->_stasis_ini_fp.delivery;
    struct INIData *rtdata;
    RuntimeEnv *rt;

    validate_delivery_ini(ini);
    // Populate runtime variables first they may be interpreted by other
    // keys in the configuration
    rt = runtime_copy(__environ);
    while ((rtdata = ini_getall(ini, "runtime")) != NULL) {
        char rec[STASIS_BUFSIZ];
        sprintf(rec, "%s=%s", lstrip(strip(rtdata->key)), lstrip(strip(rtdata->value)));
        runtime_set(rt, rtdata->key, rtdata->value);
    }
    runtime_apply(rt);
    ctx->runtime.environ = rt;

    ini_getval_required(ini, "meta", "mission", INIVAL_TYPE_STR, &val);
    conv_str(&ctx->meta.mission, val);

    if (!strcasecmp(ctx->meta.mission, "hst")) {
        ini_getval(ini, "meta", "codename", INIVAL_TYPE_STR, &val);
        conv_str(&ctx->meta.codename, val);
    } else {
        ctx->meta.codename = NULL;
    }

    /*
    if (!strcasecmp(ctx->meta.mission, "jwst")) {
        ini_getval(ini, "meta", "version", INIVAL_TYPE_STR, &val);
        conv_str(&ctx->meta.version, val);

    } else {
        ctx->meta.version = NULL;
    }
    */
    ini_getval(ini, "meta", "version", INIVAL_TYPE_STR, &val);
    conv_str(&ctx->meta.version, val);

    ini_getval_required(ini, "meta", "name", INIVAL_TYPE_STR, &val);
    conv_str(&ctx->meta.name, val);

    ini_getval(ini, "meta", "rc", INIVAL_TYPE_INT, &val);
    conv_int(&ctx->meta.rc, val);

    ini_getval(ini, "meta", "final", INIVAL_TYPE_BOOL, &val);
    conv_bool(&ctx->meta.final, val);

    ini_getval(ini, "meta", "based_on", INIVAL_TYPE_STR, &val);
    conv_str(&ctx->meta.based_on, val);

    if (!ctx->meta.python) {
        ini_getval(ini, "meta", "python", INIVAL_TYPE_STR, &val);
        conv_str(&ctx->meta.python, val);
        guard_free(ctx->meta.python_compact);
        ctx->meta.python_compact = to_short_version(ctx->meta.python);
    } else {
        ini_setval(&ini, INI_SETVAL_REPLACE, "meta", "python", ctx->meta.python);
    }

    ini_getval_required(ini, "conda", "installer_name", INIVAL_TYPE_STR, &val);
    conv_str(&ctx->conda.installer_name, val);

    ini_getval_required(ini, "conda", "installer_version", INIVAL_TYPE_STR, &val);
    conv_str(&ctx->conda.installer_version, val);

    ini_getval_required(ini, "conda", "installer_platform", INIVAL_TYPE_STR, &val);
    conv_str(&ctx->conda.installer_platform, val);

    ini_getval_required(ini, "conda", "installer_arch", INIVAL_TYPE_STR, &val);
    conv_str(&ctx->conda.installer_arch, val);

    ini_getval_required(ini, "conda", "installer_baseurl", INIVAL_TYPE_STR, &val);
    conv_str(&ctx->conda.installer_baseurl, val);

    ini_getval(ini, "conda", "conda_packages", INIVAL_TYPE_STR_ARRAY, &val);
    conv_strlist(&ctx->conda.conda_packages, LINE_SEP, val);

    for (size_t i = 0; i < strlist_count(ctx->conda.conda_packages); i++) {
        char *pkg = strlist_item(ctx->conda.conda_packages, i);
        if (strpbrk(pkg, ";#")) {
            strlist_remove(ctx->conda.conda_packages, i);
        }
    }

    ini_getval(ini, "conda", "pip_packages", INIVAL_TYPE_STR_ARRAY, &val);
    conv_strlist(&ctx->conda.pip_packages, LINE_SEP, val);

    for (size_t i = 0; i < strlist_count(ctx->conda.pip_packages); i++) {
        char *pkg = strlist_item(ctx->conda.pip_packages, i);
        if (strpbrk(pkg, ";#")) {
            strlist_remove(ctx->conda.pip_packages, i);
        }
    }

    // Delivery metadata consumed
    populate_mission_ini(&ctx);

    if (ctx->info.release_name) {
        guard_free(ctx->info.release_name);
        guard_free(ctx->info.build_name);
        guard_free(ctx->info.build_number);
    }

    if (delivery_format_str(ctx, &ctx->info.release_name, ctx->rules.release_fmt)) {
        fprintf(stderr, "Failed to generate release name. Format used: %s\n", ctx->rules.release_fmt);
        return -1;
    }

    if (!ctx->info.build_name) {
        delivery_format_str(ctx, &ctx->info.build_name, ctx->rules.build_name_fmt);
    }
    if (!ctx->info.build_number) {
        delivery_format_str(ctx, &ctx->info.build_number, ctx->rules.build_number_fmt);
    }

    // Best I can do to make output directories unique. Annoying.
    delivery_init_dirs_stage2(ctx);

    if (!ctx->conda.conda_packages_defer) {
        ctx->conda.conda_packages_defer = strlist_init();
    }
    if (!ctx->conda.pip_packages_defer) {
        ctx->conda.pip_packages_defer = strlist_init();
    }

    for (size_t z = 0, i = 0; i < ini->section_count; i++) {
        if (startswith(ini->section[i]->key, "test:")) {
            val.as_char_p = strchr(ini->section[i]->key, ':') + 1;
            if (val.as_char_p && isempty(val.as_char_p)) {
                return 1;
            }
            conv_str(&ctx->tests[z].name, val);

            ini_getval_required(ini, ini->section[i]->key, "version", INIVAL_TYPE_STR, &val);
            conv_str(&ctx->tests[z].version, val);

            ini_getval_required(ini, ini->section[i]->key, "repository", INIVAL_TYPE_STR, &val);
            conv_str(&ctx->tests[z].repository, val);

            ini_getval_required(ini, ini->section[i]->key, "script", INIVAL_TYPE_STR, &val);
            conv_str_noexpand(&ctx->tests[z].script, val);

            ini_getval(ini, ini->section[i]->key, "repository_remove_tags", INIVAL_TYPE_STR_ARRAY, &val);
            conv_strlist(&ctx->tests[z].repository_remove_tags, LINE_SEP, val);

            ini_getval(ini, ini->section[i]->key, "build_recipe", INIVAL_TYPE_STR, &val);
            conv_str(&ctx->tests[z].build_recipe, val);

            ini_getval(ini, ini->section[i]->key, "runtime", INIVAL_TO_LIST, &val);
            conv_strlist(&ctx->tests[z].runtime.environ, LINE_SEP, val);
            z++;
        }
    }

    for (size_t z = 0, i = 0; i < ini->section_count; i++) {
        if (startswith(ini->section[i]->key, "deploy:artifactory")) {
            // Artifactory base configuration
            ini_getval(ini, ini->section[i]->key, "workaround_parent_only", INIVAL_TYPE_BOOL, &val);
            conv_bool(&ctx->deploy.jfrog[z].upload_ctx.workaround_parent_only, val);

            ini_getval(ini, ini->section[i]->key, "exclusions", INIVAL_TYPE_STR, &val);
            conv_str(&ctx->deploy.jfrog[z].upload_ctx.exclusions, val);

            ini_getval(ini, ini->section[i]->key, "explode", INIVAL_TYPE_BOOL, &val);
            conv_bool(&ctx->deploy.jfrog[z].upload_ctx.explode, val);

            ini_getval(ini, ini->section[i]->key, "recursive", INIVAL_TYPE_BOOL, &val);
            conv_bool(&ctx->deploy.jfrog[z].upload_ctx.recursive, val);

            ini_getval(ini, ini->section[i]->key, "retries", INIVAL_TYPE_INT, &val);
            conv_int(&ctx->deploy.jfrog[z].upload_ctx.retries, val);

            ini_getval(ini, ini->section[i]->key, "retry_wait_time", INIVAL_TYPE_INT, &val);
            conv_int(&ctx->deploy.jfrog[z].upload_ctx.retry_wait_time, val);

            ini_getval(ini, ini->section[i]->key, "detailed_summary", INIVAL_TYPE_BOOL, &val);
            conv_bool(&ctx->deploy.jfrog[z].upload_ctx.detailed_summary, val);

            ini_getval(ini, ini->section[i]->key, "quiet", INIVAL_TYPE_BOOL, &val);
            conv_bool(&ctx->deploy.jfrog[z].upload_ctx.quiet, val);

            ini_getval(ini, ini->section[i]->key, "regexp", INIVAL_TYPE_BOOL, &val);
            conv_bool(&ctx->deploy.jfrog[z].upload_ctx.regexp, val);

            ini_getval(ini, ini->section[i]->key, "spec", INIVAL_TYPE_STR, &val);
            conv_str(&ctx->deploy.jfrog[z].upload_ctx.spec, val);

            ini_getval(ini, ini->section[i]->key, "flat", INIVAL_TYPE_BOOL, &val);
            conv_bool(&ctx->deploy.jfrog[z].upload_ctx.flat, val);

            ini_getval(ini, ini->section[i]->key, "repo", INIVAL_TYPE_STR, &val);
            conv_str(&ctx->deploy.jfrog[z].repo, val);

            ini_getval(ini, ini->section[i]->key, "dest", INIVAL_TYPE_STR, &val);
            conv_str(&ctx->deploy.jfrog[z].dest, val);

            ini_getval(ini, ini->section[i]->key, "files", INIVAL_TYPE_STR_ARRAY, &val);
            conv_strlist(&ctx->deploy.jfrog[z].files, LINE_SEP, val);
            z++;
        }
    }

    for (size_t i = 0; i < ini->section_count; i++) {
        if (startswith(ini->section[i]->key, "deploy:docker")) {
            ini_getval(ini, ini->section[i]->key, "registry", INIVAL_TYPE_STR, &val);
            conv_str(&ctx->deploy.docker.registry, val);

            ini_getval(ini, ini->section[i]->key, "image_compression", INIVAL_TYPE_STR, &val);
            conv_str(&ctx->deploy.docker.image_compression, val);

            ini_getval(ini, ini->section[i]->key, "test_script", INIVAL_TYPE_STR, &val);
            conv_str(&ctx->deploy.docker.test_script, val);

            ini_getval(ini, ini->section[i]->key, "build_args", INIVAL_TYPE_STR_ARRAY, &val);
            conv_strlist(&ctx->deploy.docker.build_args, LINE_SEP, val);

            ini_getval(ini, ini->section[i]->key, "tags", INIVAL_TYPE_STR_ARRAY, &val);
            conv_strlist(&ctx->deploy.docker.tags, LINE_SEP, val);
        }
    }
    return 0;
}

static int populate_delivery_cfg(struct Delivery *ctx) {
    union INIVal val;
    struct INIFILE *cfg = ctx->_stasis_ini_fp.cfg;
    if (!cfg) {
        return -1;
    }
    ini_getval(cfg, "default", "conda_staging_dir", INIVAL_TYPE_STR, &val);
    conv_str(&ctx->storage.conda_staging_dir, val);
    ini_getval(cfg, "default", "conda_staging_url", INIVAL_TYPE_STR, &val);
    conv_str(&ctx->storage.conda_staging_url, val);
    ini_getval(cfg, "default", "wheel_staging_dir", INIVAL_TYPE_STR, &val);
    conv_str(&ctx->storage.wheel_staging_dir, val);
    ini_getval(cfg, "default", "wheel_staging_url", INIVAL_TYPE_STR, &val);
    conv_str(&ctx->storage.wheel_staging_url, val);
    ini_getval(cfg, "default", "conda_fresh_start", INIVAL_TYPE_BOOL, &val);
    conv_bool(&globals.conda_fresh_start, val);
    // Below can also be toggled by command-line arguments
    if (!globals.continue_on_error) {
        ini_getval(cfg, "default", "continue_on_error", INIVAL_TYPE_BOOL, &val);
        conv_bool(&globals.continue_on_error, val);
    }
    // Below can also be toggled by command-line arguments
    if (!globals.always_update_base_environment) {
        ini_getval(cfg, "default", "always_update_base_environment", INIVAL_TYPE_BOOL, &val);
        conv_bool(&globals.always_update_base_environment, val);
    }
    ini_getval(cfg, "default", "conda_install_prefix", INIVAL_TYPE_STR, &val);
    conv_str(&globals.conda_install_prefix, val);
    ini_getval(cfg, "default", "conda_packages", INIVAL_TYPE_STR_ARRAY, &val);
    conv_strlist(&globals.conda_packages, LINE_SEP, val);
    ini_getval(cfg, "default", "pip_packages", INIVAL_TYPE_STR_ARRAY, &val);
    conv_strlist(&globals.pip_packages, LINE_SEP, val);
    // Configure jfrog cli downloader
    ini_getval(cfg, "jfrog_cli_download", "url", INIVAL_TYPE_STR, &val);
    conv_str(&globals.jfrog.jfrog_artifactory_base_url, val);
    ini_getval(cfg, "jfrog_cli_download", "product", INIVAL_TYPE_STR, &val);
    conv_str(&globals.jfrog.jfrog_artifactory_product, val);
    ini_getval(cfg, "jfrog_cli_download", "version_series", INIVAL_TYPE_STR, &val);
    conv_str(&globals.jfrog.cli_major_ver, val);
    ini_getval(cfg, "jfrog_cli_download", "version", INIVAL_TYPE_STR, &val);
    conv_str(&globals.jfrog.version, val);
    ini_getval(cfg, "jfrog_cli_download", "filename", INIVAL_TYPE_STR, &val);
    conv_str(&globals.jfrog.remote_filename, val);
    ini_getval(cfg, "deploy:artifactory", "url", INIVAL_TYPE_STR, &val);
    conv_str(&globals.jfrog.url, val);
    ini_getval(cfg, "deploy:artifactory", "repo", INIVAL_TYPE_STR, &val);
    conv_str(&globals.jfrog.repo, val);
    return 0;
}

static int populate_info(struct Delivery *ctx) {
    if (!ctx->info.time_str_epoch) {
        // Record timestamp used for release
        time(&ctx->info.time_now);
        ctx->info.time_info = localtime(&ctx->info.time_now);

        ctx->info.time_str_epoch = calloc(STASIS_TIME_STR_MAX, sizeof(*ctx->info.time_str_epoch));
        if (!ctx->info.time_str_epoch) {
            msg(STASIS_MSG_ERROR, "Unable to allocate memory for Unix epoch string\n");
            return -1;
        }
        snprintf(ctx->info.time_str_epoch, STASIS_TIME_STR_MAX - 1, "%li", ctx->info.time_now);
    }
    return 0;
}

int *bootstrap_build_info(struct Delivery *ctx) {
    struct Delivery local;
    memset(&local, 0, sizeof(local));
    local._stasis_ini_fp.cfg = ini_open(ctx->_stasis_ini_fp.cfg_path);
    local._stasis_ini_fp.delivery = ini_open(ctx->_stasis_ini_fp.delivery_path);
    delivery_init_platform(&local);
    populate_delivery_cfg(&local);
    populate_delivery_ini(&local);
    populate_info(&local);
    ctx->info.build_name = strdup(local.info.build_name);
    ctx->info.build_number = strdup(local.info.build_number);
    ctx->info.release_name = strdup(local.info.release_name);
    memcpy(&ctx->info.time_info, &local.info.time_info, sizeof(ctx->info.time_info));
    ctx->info.time_now = local.info.time_now;
    ctx->info.time_str_epoch = strdup(local.info.time_str_epoch);
    delivery_free(&local);
    return 0;
}

int delivery_init(struct Delivery *ctx) {
    populate_info(ctx);
    populate_delivery_cfg(ctx);

    // Set artifactory URL via environment variable if possible
    char *jfurl = getenv("STASIS_JF_ARTIFACTORY_URL");
    if (jfurl) {
        if (globals.jfrog.url) {
            guard_free(globals.jfrog.url);
        }
        globals.jfrog.url = strdup(jfurl);
    }

    // Set artifactory repository via environment if possible
    char *jfrepo = getenv("STASIS_JF_REPO");
    if (jfrepo) {
        if (globals.jfrog.repo) {
            guard_free(globals.jfrog.repo);
        }
        globals.jfrog.repo = strdup(jfrepo);
    }

    // Configure architecture and platform information
    delivery_init_platform(ctx);

    // Create STASIS directory structure
    delivery_init_dirs_stage1(ctx);

    char config_local[PATH_MAX];
    sprintf(config_local, "%s/%s", ctx->storage.tmpdir, "config");
    setenv("XDG_CONFIG_HOME", config_local, 1);

    char cache_local[PATH_MAX];
    sprintf(cache_local, "%s/%s", ctx->storage.tmpdir, "cache");
    setenv("XDG_CACHE_HOME", ctx->storage.tmpdir, 1);

    // add tools to PATH
    char pathvar_tmp[STASIS_BUFSIZ];
    sprintf(pathvar_tmp, "%s/bin:%s", ctx->storage.tools_dir, getenv("PATH"));
    setenv("PATH", pathvar_tmp, 1);

    // Prevent git from paginating output
    setenv("GIT_PAGER", "", 1);

    populate_delivery_ini(ctx);

    if (ctx->deploy.docker.tags) {
        for (size_t i = 0; i < strlist_count(ctx->deploy.docker.tags); i++) {
            char *item = strlist_item(ctx->deploy.docker.tags, i);
            tolower_s(item);
        }
    }

    if (ctx->deploy.docker.image_compression) {
        if (docker_validate_compression_program(ctx->deploy.docker.image_compression)) {
            SYSERROR("[deploy:docker].image_compression - invalid command / program is not installed: %s", ctx->deploy.docker.image_compression);
            return -1;
        }
    }
    return 0;
}

int delivery_format_str(struct Delivery *ctx, char **dest, const char *fmt) {
    size_t fmt_len = strlen(fmt);

    if (!*dest) {
        *dest = calloc(STASIS_NAME_MAX, sizeof(**dest));
        if (!*dest) {
            return -1;
        }
    }

    for (size_t i = 0; i < fmt_len; i++) {
        if (fmt[i] == '%' && strlen(&fmt[i])) {
            i++;
            switch (fmt[i]) {
                case 'n':   // name
                    strcat(*dest, ctx->meta.name);
                    break;
                case 'c':   // codename
                    strcat(*dest, ctx->meta.codename);
                    break;
                case 'm':   // mission
                    strcat(*dest, ctx->meta.mission);
                    break;
                case 'r':   // revision
                    sprintf(*dest + strlen(*dest), "%d", ctx->meta.rc);
                    break;
                case 'R':   // "final"-aware revision
                    if (ctx->meta.final)
                        strcat(*dest, "final");
                    else
                        sprintf(*dest + strlen(*dest), "%d", ctx->meta.rc);
                    break;
                case 'v':   // version
                    strcat(*dest, ctx->meta.version);
                    break;
                case 'P':   // python version
                    strcat(*dest, ctx->meta.python);
                    break;
                case 'p':   // python version major/minor
                    strcat(*dest, ctx->meta.python_compact);
                    break;
                case 'a':   // system architecture name
                    strcat(*dest, ctx->system.arch);
                    break;
                case 'o':   // system platform (OS) name
                    strcat(*dest, ctx->system.platform[DELIVERY_PLATFORM_RELEASE]);
                    break;
                case 't':   // unix epoch
                    sprintf(*dest + strlen(*dest), "%ld", ctx->info.time_now);
                    break;
                default:    // unknown formatter, write as-is
                    sprintf(*dest + strlen(*dest), "%c%c", fmt[i - 1], fmt[i]);
                    break;
            }
        } else {    // write non-format text
            sprintf(*dest + strlen(*dest), "%c", fmt[i]);
        }
    }
    return 0;
}

void delivery_debug_show(struct Delivery *ctx) {
    printf("\n====DEBUG====\n");
    printf("%-20s %-10s\n", "System configuration directory:", globals.sysconfdir);
    printf("%-20s %-10s\n", "Mission directory:", ctx->storage.mission_dir);
    printf("%-20s %-10s\n", "Testing enabled:", globals.enable_testing ? "Yes" : "No");
    printf("%-20s %-10s\n", "Docker image builds enabled:", globals.enable_docker ? "Yes" : "No");
    printf("%-20s %-10s\n", "Artifact uploading enabled:", globals.enable_artifactory ? "Yes" : "No");
}

void delivery_meta_show(struct Delivery *ctx) {
    if (globals.verbose) {
        delivery_debug_show(ctx);
    }

    printf("\n====DELIVERY====\n");
    printf("%-20s %-10s\n", "Target Python:", ctx->meta.python);
    printf("%-20s %-10s\n", "Name:", ctx->meta.name);
    printf("%-20s %-10s\n", "Mission:", ctx->meta.mission);
    if (ctx->meta.codename) {
        printf("%-20s %-10s\n", "Codename:", ctx->meta.codename);
    }
    if (ctx->meta.version) {
        printf("%-20s %-10s\n", "Version", ctx->meta.version);
    }
    if (!ctx->meta.final) {
        printf("%-20s %-10d\n", "RC Level:", ctx->meta.rc);
    }
    printf("%-20s %-10s\n", "Final Release:", ctx->meta.final ? "Yes" : "No");
    printf("%-20s %-10s\n", "Based On:", ctx->meta.based_on ? ctx->meta.based_on : "New");
}

void delivery_conda_show(struct Delivery *ctx) {
    printf("\n====CONDA====\n");
    printf("%-20s %-10s\n", "Prefix:", ctx->storage.conda_install_prefix);

    puts("Native Packages:");
    if (strlist_count(ctx->conda.conda_packages)) {
        for (size_t i = 0; i < strlist_count(ctx->conda.conda_packages); i++) {
            char *token = strlist_item(ctx->conda.conda_packages, i);
            if (isempty(token) || isblank(*token) || startswith(token, "-")) {
                continue;
            }
            printf("%21s%s\n", "", token);
        }
    } else {
       printf("%21s%s\n", "", "N/A");
    }

    puts("Python Packages:");
    if (strlist_count(ctx->conda.pip_packages)) {
        for (size_t i = 0; i < strlist_count(ctx->conda.pip_packages); i++) {
            char *token = strlist_item(ctx->conda.pip_packages, i);
            if (isempty(token) || isblank(*token) || startswith(token, "-")) {
                continue;
            }
            printf("%21s%s\n", "", token);
        }
    } else {
        printf("%21s%s\n", "", "N/A");
    }
}

void delivery_tests_show(struct Delivery *ctx) {
    printf("\n====TESTS====\n");
    for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
        if (!ctx->tests[i].name) {
            continue;
        }
        printf("%-20s %-20s %s\n", ctx->tests[i].name,
               ctx->tests[i].version,
               ctx->tests[i].repository);
    }
}

void delivery_runtime_show(struct Delivery *ctx) {
    printf("\n====RUNTIME====\n");
    struct StrList *rt = NULL;
    rt = strlist_copy(ctx->runtime.environ);
    if (!rt) {
        // no data
        return;
    }
    strlist_sort(rt, STASIS_SORT_ALPHA);
    size_t total = strlist_count(rt);
    for (size_t i = 0; i < total; i++) {
        char *item = strlist_item(rt, i);
        if (!item) {
            // not supposed to occur
            msg(STASIS_MSG_WARN | STASIS_MSG_L1, "Encountered unexpected NULL at record %zu of %zu of runtime array.\n", i);
            return;
        }
        printf("%s\n", item);
    }
}

int delivery_build_recipes(struct Delivery *ctx) {
    for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
        char *recipe_dir = NULL;
        if (ctx->tests[i].build_recipe) { // build a conda recipe
            int recipe_type;
            int status;
            if (recipe_clone(ctx->storage.build_recipes_dir, ctx->tests[i].build_recipe, NULL, &recipe_dir)) {
                fprintf(stderr, "Encountered an issue while cloning recipe for: %s\n", ctx->tests[i].name);
                return -1;
            }
            recipe_type = recipe_get_type(recipe_dir);
            pushd(recipe_dir);
            {
                if (RECIPE_TYPE_ASTROCONDA == recipe_type) {
                    pushd(path_basename(ctx->tests[i].repository));
                } else if (RECIPE_TYPE_CONDA_FORGE == recipe_type) {
                    pushd("recipe");
                }

                char recipe_version[100];
                char recipe_buildno[100];
                char recipe_git_url[PATH_MAX];
                char recipe_git_rev[PATH_MAX];

                //sprintf(recipe_version, "{%% set version = GIT_DESCRIBE_TAG ~ \".dev\" ~ GIT_DESCRIBE_NUMBER ~ \"+\" ~ GIT_DESCRIBE_HASH %%}");
                //sprintf(recipe_git_url, "  git_url: %s", ctx->tests[i].repository);
                //sprintf(recipe_git_rev, "  git_rev: %s", ctx->tests[i].version);
                // TODO: Conditionally download archives if github.com is the origin. Else, use raw git_* keys ^^^
                sprintf(recipe_version, "{%% set version = \"%s\" %%}", ctx->tests[i].repository_info_tag ? ctx->tests[i].repository_info_tag : ctx->tests[i].version);
                sprintf(recipe_git_url, "  url: %s/archive/refs/tags/{{ version }}.tar.gz", ctx->tests[i].repository);
                strcpy(recipe_git_rev, "");
                sprintf(recipe_buildno, "  number: 0");

                unsigned flags = REPLACE_TRUNCATE_AFTER_MATCH;
                //file_replace_text("meta.yaml", "{% set version = ", recipe_version);
                if (ctx->meta.final) {
                    sprintf(recipe_version, "{%% set version = \"%s\" %%}", ctx->tests[i].version);
                    // TODO: replace sha256 of tagged archive
                    // TODO: leave the recipe unchanged otherwise. in theory this should produce the same conda package hash as conda forge.
                    // For now, remove the sha256 requirement
                    file_replace_text("meta.yaml", "sha256:", "\n", flags);
                } else {
                    file_replace_text("meta.yaml", "{% set version = ", recipe_version, flags);
                    file_replace_text("meta.yaml", "  url:", recipe_git_url, flags);
                    //file_replace_text("meta.yaml", "sha256:", recipe_git_rev);
                    file_replace_text("meta.yaml", "  sha256:", "\n", flags);
                    file_replace_text("meta.yaml", "  number:", recipe_buildno, flags);
                }

                char command[PATH_MAX];
                if (RECIPE_TYPE_CONDA_FORGE == recipe_type) {
                    char arch[STASIS_NAME_MAX] = {0};
                    char platform[STASIS_NAME_MAX] = {0};

                    strcpy(platform, ctx->system.platform[DELIVERY_PLATFORM]);
                    if (strstr(platform, "Darwin")) {
                        memset(platform, 0, sizeof(platform));
                        strcpy(platform, "osx");
                    }
                    tolower_s(platform);
                    if (strstr(ctx->system.arch, "arm64")) {
                        strcpy(arch, "arm64");
                    } else if (strstr(ctx->system.arch, "64")) {
                        strcpy(arch, "64");
                    } else {
                        strcat(arch, "32"); // blind guess
                    }
                    tolower_s(arch);

                    sprintf(command, "mambabuild --python=%s -m ../.ci_support/%s_%s_.yaml .",
                        ctx->meta.python, platform, arch);
                    } else {
                        sprintf(command, "mambabuild --python=%s .", ctx->meta.python);
                    }
                status = conda_exec(command);
                if (status) {
                    return -1;
                }

                if (RECIPE_TYPE_GENERIC != recipe_type) {
                    popd();
                }
                popd();
            }
        }
        if (recipe_dir) {
            guard_free(recipe_dir);
        }
    }
    return 0;
}

static int filter_repo_tags(char *repo, struct StrList *patterns) {
    int result = 0;

    if (!pushd(repo)) {
        int list_status = 0;
        char *tags_raw = shell_output("git tag -l", &list_status);
        struct StrList *tags = strlist_init();
        strlist_append_tokenize(tags, tags_raw, LINE_SEP);

        for (size_t i = 0; tags && i < strlist_count(tags); i++) {
            char *tag = strlist_item(tags, i);
            for (size_t p = 0; p < strlist_count(patterns); p++) {
                char *pattern = strlist_item(patterns, p);
                int match = fnmatch(pattern, tag, 0);
                if (!match) {
                    char cmd[PATH_MAX] = {0};
                    sprintf(cmd, "git tag -d %s", tag);
                    result += system(cmd);
                    break;
                }
            }
        }
        guard_strlist_free(&tags);
        guard_free(tags_raw);
        popd();
    } else {
        result = -1;
    }
    return result;
}

struct StrList *delivery_build_wheels(struct Delivery *ctx) {
    struct StrList *result = NULL;
    struct Process proc;
    memset(&proc, 0, sizeof(proc));

    result = strlist_init();
    if (!result) {
        perror("unable to allocate memory for string list");
        return NULL;
    }

    for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
        if (!ctx->tests[i].build_recipe && ctx->tests[i].repository) { // build from source
            char srcdir[PATH_MAX];
            char wheeldir[PATH_MAX];
            memset(srcdir, 0, sizeof(srcdir));
            memset(wheeldir, 0, sizeof(wheeldir));

            sprintf(srcdir, "%s/%s", ctx->storage.build_sources_dir, ctx->tests[i].name);
            git_clone(&proc, ctx->tests[i].repository, srcdir, ctx->tests[i].version);

            if (ctx->tests[i].repository_remove_tags && strlist_count(ctx->tests[i].repository_remove_tags)) {
                filter_repo_tags(srcdir, ctx->tests[i].repository_remove_tags);
            }

            pushd(srcdir);
            {
                char dname[NAME_MAX];
                char outdir[PATH_MAX];
                char cmd[PATH_MAX * 2];
                memset(dname, 0, sizeof(dname));
                memset(outdir, 0, sizeof(outdir));
                memset(cmd, 0, sizeof(outdir));

                strcpy(dname, ctx->tests[i].name);
                tolower_s(dname);
                sprintf(outdir, "%s/%s", ctx->storage.wheel_artifact_dir, dname);
                if (mkdirs(outdir, 0755)) {
                    fprintf(stderr, "failed to create output directory: %s\n", outdir);
                }

                sprintf(cmd, "-m build -w -o %s", outdir);
                if (python_exec(cmd)) {
                    fprintf(stderr, "failed to generate wheel package for %s-%s\n", ctx->tests[i].name, ctx->tests[i].version);
                    strlist_free(&result);
                    return NULL;
                }
                popd();
            }
        }
    }
    return result;
}

static const struct Test *requirement_from_test(struct Delivery *ctx, const char *name) {
    struct Test *result;

    result = NULL;
    for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
        if (strstr(name, ctx->tests[i].name)) {
            result = &ctx->tests[i];
            break;
        }
    }
    return result;
}

int delivery_install_packages(struct Delivery *ctx, char *conda_install_dir, char *env_name, int type, struct StrList **manifest) {
    char cmd[PATH_MAX];
    char pkgs[STASIS_BUFSIZ];
    char *env_current = getenv("CONDA_DEFAULT_ENV");

    if (env_current) {
        // The requested environment is not the current environment
        if (strcmp(env_current, env_name) != 0) {
            // Activate the requested environment
            printf("Activating: %s\n", env_name);
            conda_activate(conda_install_dir, env_name);
            runtime_replace(&ctx->runtime.environ, __environ);
        }
    }

    memset(cmd, 0, sizeof(cmd));
    memset(pkgs, 0, sizeof(pkgs));
    strcat(cmd, "install");

    typedef int (*Runner)(const char *);
    Runner runner = NULL;
    if (INSTALL_PKG_CONDA & type) {
        runner = conda_exec;
    } else if (INSTALL_PKG_PIP & type) {
        runner = pip_exec;
    }

    if (INSTALL_PKG_CONDA_DEFERRED & type) {
        strcat(cmd, " --use-local");
    } else if (INSTALL_PKG_PIP_DEFERRED & type) {
        // Don't change the baseline package set unless we're working with a
        // new build. Release candidates will need to keep packages as stable
        // as possible between releases.
        if (!ctx->meta.based_on) {
            strcat(cmd, " --upgrade");
        }
        sprintf(cmd + strlen(cmd), " --extra-index-url 'file://%s'", ctx->storage.wheel_artifact_dir);
    }

    for (size_t x = 0; manifest[x] != NULL; x++) {
        char *name = NULL;
        for (size_t p = 0; p < strlist_count(manifest[x]); p++) {
            name = strlist_item(manifest[x], p);
            strip(name);
            if (!strlen(name)) {
                continue;
            }
            if (INSTALL_PKG_PIP_DEFERRED & type) {
                const struct Test *info = requirement_from_test(ctx, name);
                if (info) {
                    sprintf(cmd + strlen(cmd), " '%s==%s'", info->name, info->version);
                } else {
                    fprintf(stderr, "Deferred package '%s' is not present in the tested package list!\n", name);
                    return -1;
                }
            } else {
                if (startswith(name, "--") || startswith(name, "-")) {
                    sprintf(cmd + strlen(cmd), " %s", name);
                } else {
                    sprintf(cmd + strlen(cmd), " '%s'", name);
                }
            }
        }
        int status = runner(cmd);
        if (status) {
            return status;
        }
    }
    return 0;
}

void delivery_get_installer_url(struct Delivery *ctx, char *result) {
    if (ctx->conda.installer_version) {
        // Use version specified by configuration file
        sprintf(result, "%s/%s-%s-%s-%s.sh", ctx->conda.installer_baseurl,
                ctx->conda.installer_name,
                ctx->conda.installer_version,
                ctx->conda.installer_platform,
                ctx->conda.installer_arch);
    } else {
        // Use latest installer
        sprintf(result, "%s/%s-%s-%s.sh", ctx->conda.installer_baseurl,
                ctx->conda.installer_name,
                ctx->conda.installer_platform,
                ctx->conda.installer_arch);
    }

}

int delivery_get_installer(struct Delivery *ctx, char *installer_url) {
    char script_path[PATH_MAX];
    char *installer = path_basename(installer_url);

    memset(script_path, 0, sizeof(script_path));
    sprintf(script_path, "%s/%s", ctx->storage.tmpdir, installer);
    if (access(script_path, F_OK)) {
        // Script doesn't exist
        int fetch_status = download(installer_url, script_path, NULL);
        if (HTTP_ERROR(fetch_status) || fetch_status < 0) {
            // download failed
            return -1;
        }
    } else {
        msg(STASIS_MSG_RESTRICT | STASIS_MSG_L3, "Skipped, installer already exists\n", script_path);
    }

    ctx->conda.installer_path = strdup(script_path);
    if (!ctx->conda.installer_path) {
        SYSERROR("Unable to duplicate script_path: '%s'", script_path);
        return -1;
    }

    return 0;
}

int delivery_copy_conda_artifacts(struct Delivery *ctx) {
    char cmd[STASIS_BUFSIZ];
    char conda_build_dir[PATH_MAX];
    char subdir[PATH_MAX];
    memset(cmd, 0, sizeof(cmd));
    memset(conda_build_dir, 0, sizeof(conda_build_dir));
    memset(subdir, 0, sizeof(subdir));

    sprintf(conda_build_dir, "%s/%s", ctx->storage.conda_install_prefix, "conda-bld");
    // One must run conda build at least once to create the "conda-bld" directory.
    // When this directory is missing there can be no build artifacts.
    if (access(conda_build_dir, F_OK) < 0) {
        msg(STASIS_MSG_RESTRICT | STASIS_MSG_WARN | STASIS_MSG_L3,
            "Skipped: 'conda build' has never been executed.\n");
        return 0;
    }

    snprintf(cmd, sizeof(cmd) - 1, "rsync -avi --progress %s/%s %s",
             conda_build_dir,
             ctx->system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR],
             ctx->storage.conda_artifact_dir);

    return system(cmd);
}

int delivery_copy_wheel_artifacts(struct Delivery *ctx) {
    char cmd[PATH_MAX];
    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, sizeof(cmd) - 1, "rsync -avi --progress %s/*/dist/*.whl %s",
             ctx->storage.build_sources_dir,
             ctx->storage.wheel_artifact_dir);
    return system(cmd);
}

int delivery_index_wheel_artifacts(struct Delivery *ctx) {
    struct dirent *rec;
    DIR *dp;
    FILE *top_fp;

    dp = opendir(ctx->storage.wheel_artifact_dir);
    if (!dp) {
        return -1;
    }

    // Generate a "dumb" local pypi index that is compatible with:
    // pip install --extra-index-url
    char top_index[PATH_MAX];
    memset(top_index, 0, sizeof(top_index));
    sprintf(top_index, "%s/index.html", ctx->storage.wheel_artifact_dir);
    top_fp = fopen(top_index, "w+");
    if (!top_fp) {
        return -2;
    }

    while ((rec = readdir(dp)) != NULL) {
        // skip directories
        if (DT_REG == rec->d_type || !strcmp(rec->d_name, "..") || !strcmp(rec->d_name, ".")) {
            continue;
        }

        FILE *bottom_fp;
        char bottom_index[PATH_MAX * 2];
        memset(bottom_index, 0, sizeof(bottom_index));
        sprintf(bottom_index, "%s/%s/index.html", ctx->storage.wheel_artifact_dir, rec->d_name);
        bottom_fp = fopen(bottom_index, "w+");
        if (!bottom_fp) {
            return -3;
        }

        if (globals.verbose) {
            printf("+ %s\n", rec->d_name);
        }
        // Add record to top level index
        fprintf(top_fp, "<a href=\"%s/\">%s</a><br/>\n", rec->d_name, rec->d_name);

        char dpath[PATH_MAX * 2];
        memset(dpath, 0, sizeof(dpath));
        sprintf(dpath, "%s/%s", ctx->storage.wheel_artifact_dir, rec->d_name);
        struct StrList *packages = listdir(dpath);
        if (!packages) {
            fclose(top_fp);
            fclose(bottom_fp);
            return -4;
        }

        for (size_t i = 0; i < strlist_count(packages); i++) {
            char *package = strlist_item(packages, i);
            if (!endswith(package, ".whl")) {
                continue;
            }
            if (globals.verbose) {
                printf("`- %s\n", package);
            }
            // Write record to bottom level index
            fprintf(bottom_fp, "<a href=\"%s\">%s</a><br/>\n", package, package);
        }
        fclose(bottom_fp);

        guard_strlist_free(&packages);
    }
    closedir(dp);
    fclose(top_fp);
    return 0;
}

void delivery_install_conda(char *install_script, char *conda_install_dir) {
    struct Process proc;
    memset(&proc, 0, sizeof(proc));

    if (globals.conda_fresh_start) {
        if (!access(conda_install_dir, F_OK)) {
            // directory exists so remove it
            if (rmtree(conda_install_dir)) {
                perror("unable to remove previous installation");
                exit(1);
            }

            // Proceed with the installation
            // -b = batch mode (non-interactive)
            char cmd[PATH_MAX] = {0};
            snprintf(cmd, sizeof(cmd) - 1, "%s %s -b -p %s",
                     find_program("bash"),
                     install_script,
                     conda_install_dir);
            if (shell_safe(&proc, cmd)) {
                fprintf(stderr, "conda installation failed\n");
                exit(1);
            }
        }
    } else {
        msg(STASIS_MSG_L3, "Conda removal disabled by configuration\n");
    }
}

void delivery_conda_enable(struct Delivery *ctx, char *conda_install_dir) {
    if (conda_activate(conda_install_dir, "base")) {
        fprintf(stderr, "conda activation failed\n");
        exit(1);
    }

    // Setting the CONDARC environment variable appears to be the only consistent
    // way to make sure the file is used. Not setting this variable leads to strange
    // behavior, especially if a conda environment is already active when STASIS is loaded.
    char rcpath[PATH_MAX];
    sprintf(rcpath, "%s/%s", conda_install_dir, ".condarc");
    setenv("CONDARC", rcpath, 1);
    if (runtime_replace(&ctx->runtime.environ, __environ)) {
        perror("unable to replace runtime environment after activating conda");
        exit(1);
    }

    conda_setup_headless();
}

void delivery_defer_packages(struct Delivery *ctx, int type) {
    struct StrList *dataptr = NULL;
    struct StrList *deferred = NULL;
    char *name = NULL;
    char cmd[PATH_MAX];

    memset(cmd, 0, sizeof(cmd));

    char mode[10];
    if (DEFER_CONDA == type) {
        dataptr = ctx->conda.conda_packages;
        deferred = ctx->conda.conda_packages_defer;
        strcpy(mode, "conda");
    } else if (DEFER_PIP == type) {
        dataptr = ctx->conda.pip_packages;
        deferred = ctx->conda.pip_packages_defer;
        strcpy(mode, "pip");
    }
    msg(STASIS_MSG_L2, "Filtering %s packages by test definition...\n", mode);

    struct StrList *filtered = NULL;
    filtered = strlist_init();
    for (size_t i = 0; i < strlist_count(dataptr); i++) {
        int ignore_pkg = 0;

        name = strlist_item(dataptr, i);
        if (!strlen(name) || isblank(*name) || isspace(*name)) {
            // no data
            continue;
        }
        msg(STASIS_MSG_L3, "package '%s': ", name);

        // Compile a list of packages that are *also* to be tested.
        char *version;
        for (size_t x = 0; x < sizeof(ctx->tests) / sizeof(ctx->tests[0]); x++) {
            version = NULL;
            if (ctx->tests[x].name) {
                if (strstr(name, ctx->tests[x].name)) {
                    version = ctx->tests[x].version;
                    ignore_pkg = 1;
                    break;
                }
            }
        }

        if (ignore_pkg) {
            char build_at[PATH_MAX];
            if (DEFER_CONDA == type) {
                sprintf(build_at, "%s=%s", name, version);
                name = build_at;
            }

            printf("BUILD FOR HOST\n");
            strlist_append(&deferred, name);
        } else {
            printf("USE EXISTING\n");
            strlist_append(&filtered, name);
        }
    }

    if (!strlist_count(deferred)) {
        msg(STASIS_MSG_WARN | STASIS_MSG_L2, "No %s packages were filtered by test definitions\n", mode);
    } else {
        if (DEFER_CONDA == type) {
            strlist_free(&ctx->conda.conda_packages);
            ctx->conda.conda_packages = strlist_copy(filtered);
        } else if (DEFER_PIP == type) {
            strlist_free(&ctx->conda.pip_packages);
            ctx->conda.pip_packages = strlist_copy(filtered);
        }
    }
    if (filtered) {
        strlist_free(&filtered);
    }
}

const char *release_header = "# delivery_name: %s\n"
                             "# delivery_fmt: %s\n"
                     "# creation_time: %s\n"
                     "# conda_ident: %s\n"
                     "# conda_build_ident: %s\n";

char *delivery_get_release_header(struct Delivery *ctx) {
    char output[STASIS_BUFSIZ];
    char stamp[100];
    strftime(stamp, sizeof(stamp) - 1, "%c", ctx->info.time_info);
    sprintf(output, release_header,
            ctx->info.release_name,
            ctx->rules.release_fmt,
            stamp,
            ctx->conda.tool_version,
            ctx->conda.tool_build_version);
    return strdup(output);
}

int delivery_dump_metadata(struct Delivery *ctx) {
    FILE *fp;
    char filename[PATH_MAX];
    sprintf(filename, "%s/meta-%s.stasis", ctx->storage.meta_dir, ctx->info.release_name);
    fp = fopen(filename, "w+");
    if (!fp) {
        return -1;
    }
    if (globals.verbose) {
        printf("%s\n", filename);
    }
    fprintf(fp, "name %s\n", ctx->meta.name);
    fprintf(fp, "version %s\n", ctx->meta.version);
    fprintf(fp, "rc %d\n", ctx->meta.rc);
    fprintf(fp, "python %s\n", ctx->meta.python);
    fprintf(fp, "python_compact %s\n", ctx->meta.python_compact);
    fprintf(fp, "mission %s\n", ctx->meta.mission);
    fprintf(fp, "codename %s\n", ctx->meta.codename ? ctx->meta.codename : "");
    fprintf(fp, "platform %s %s %s %s\n",
            ctx->system.platform[DELIVERY_PLATFORM],
            ctx->system.platform[DELIVERY_PLATFORM_CONDA_SUBDIR],
            ctx->system.platform[DELIVERY_PLATFORM_CONDA_INSTALLER],
            ctx->system.platform[DELIVERY_PLATFORM_RELEASE]);
    fprintf(fp, "arch %s\n", ctx->system.arch);
    fprintf(fp, "time %s\n", ctx->info.time_str_epoch);
    fprintf(fp, "release_fmt %s\n", ctx->rules.release_fmt);
    fprintf(fp, "release_name %s\n", ctx->info.release_name);
    fprintf(fp, "build_name_fmt %s\n", ctx->rules.build_name_fmt);
    fprintf(fp, "build_name %s\n", ctx->info.build_name);
    fprintf(fp, "build_number_fmt %s\n", ctx->rules.build_number_fmt);
    fprintf(fp, "build_number %s\n", ctx->info.build_number);
    fprintf(fp, "conda_installer_baseurl %s\n", ctx->conda.installer_baseurl);
    fprintf(fp, "conda_installer_name %s\n", ctx->conda.installer_name);
    fprintf(fp, "conda_installer_version %s\n", ctx->conda.installer_version);
    fprintf(fp, "conda_installer_platform %s\n", ctx->conda.installer_platform);
    fprintf(fp, "conda_installer_arch %s\n", ctx->conda.installer_arch);

    fclose(fp);
    return 0;
}

void delivery_rewrite_spec(struct Delivery *ctx, char *filename, unsigned stage) {
    char output[PATH_MAX];
    char *header = NULL;
    char *tempfile = NULL;
    FILE *tp = NULL;

    if (stage == DELIVERY_REWRITE_SPEC_STAGE_1) {
        header = delivery_get_release_header(ctx);
        if (!header) {
            msg(STASIS_MSG_ERROR, "failed to generate release header string\n", filename);
            exit(1);
        }
        tempfile = xmkstemp(&tp, "w+");
        if (!tempfile || !tp) {
            msg(STASIS_MSG_ERROR, "%s: unable to create temporary file\n", strerror(errno));
            exit(1);
        }
        fprintf(tp, "%s", header);

        // Read the original file
        char **contents = file_readlines(filename, 0, 0, NULL);
        if (!contents) {
            msg(STASIS_MSG_ERROR, "%s: unable to read %s", filename);
            exit(1);
        }

        // Write temporary data
        for (size_t i = 0; contents[i] != NULL; i++) {
            if (startswith(contents[i], "channels:")) {
                // Allow for additional conda channel injection
                if (ctx->conda.conda_packages_defer && strlist_count(ctx->conda.conda_packages_defer)) {
                    fprintf(tp, "%s  - @CONDA_CHANNEL@\n", contents[i]);
                    continue;
                }
            } else if (strstr(contents[i], "- pip:")) {
                if (ctx->conda.pip_packages_defer && strlist_count(ctx->conda.pip_packages_defer)) {
                    // Allow for additional pip argument injection
                    fprintf(tp, "%s      - @PIP_ARGUMENTS@\n", contents[i]);
                    continue;
                }
            } else if (startswith(contents[i], "prefix:")) {
                // Remove the prefix key
                if (strstr(contents[i], "/") || strstr(contents[i], "\\")) {
                    // path is on the same line as the key
                    continue;
                } else {
                    // path is on the next line?
                    if (contents[i + 1] && (strstr(contents[i + 1], "/") || strstr(contents[i + 1], "\\"))) {
                        i++;
                    }
                    continue;
                }
            }
            fprintf(tp, "%s", contents[i]);
        }
        GENERIC_ARRAY_FREE(contents);
        guard_free(header);
        fflush(tp);
        fclose(tp);

        // Replace the original file with our temporary data
        if (copy2(tempfile, filename, CT_PERM) < 0) {
            fprintf(stderr, "%s: could not rename '%s' to '%s'\n", strerror(errno), tempfile, filename);
            exit(1);
        }
        remove(tempfile);
        guard_free(tempfile);
    } else if (stage == DELIVERY_REWRITE_SPEC_STAGE_2) {
        // Replace "local" channel with the staging URL
        if (ctx->storage.conda_staging_url) {
            file_replace_text(filename, "@CONDA_CHANNEL@", ctx->storage.conda_staging_url, 0);
        } else if (globals.jfrog.repo) {
            sprintf(output, "%s/%s/%s/%s/packages/conda", globals.jfrog.url, globals.jfrog.repo, ctx->meta.mission, ctx->info.build_name);
            file_replace_text(filename, "@CONDA_CHANNEL@", output, 0);
        } else {
            msg(STASIS_MSG_WARN, "conda_staging_url is not configured\n", filename);
            file_replace_text(filename, "  - @CONDA_CHANNEL@", "", 0);
        }

        if (ctx->storage.wheel_staging_url) {
            file_replace_text(filename, "@PIP_ARGUMENTS@", ctx->storage.wheel_staging_url, 0);
        } else if (globals.jfrog.repo) {
            sprintf(output, "--extra-index-url %s/%s/%s/%s/packages/wheels", globals.jfrog.url, globals.jfrog.repo, ctx->meta.mission, ctx->info.build_name);
            file_replace_text(filename, "@PIP_ARGUMENTS@", output, 0);
        }
    }
}

int delivery_index_conda_artifacts(struct Delivery *ctx) {
    return conda_index(ctx->storage.conda_artifact_dir);
}

void delivery_tests_run(struct Delivery *ctx) {
    struct Process proc;
    memset(&proc, 0, sizeof(proc));

    if (!globals.workaround.conda_reactivate) {
        globals.workaround.conda_reactivate = calloc(PATH_MAX, sizeof(*globals.workaround.conda_reactivate));
    } else {
        memset(globals.workaround.conda_reactivate, 0, PATH_MAX);
    }
    snprintf(globals.workaround.conda_reactivate, PATH_MAX - 1, "\nset +x\neval `conda shell.posix reactivate`\nset -x\n");

    if (!ctx->tests[0].name) {
        msg(STASIS_MSG_WARN | STASIS_MSG_L2, "no tests are defined!\n");
    } else {
        for (size_t i = 0; i < sizeof(ctx->tests) / sizeof(ctx->tests[0]); i++) {
            if (!ctx->tests[i].name && !ctx->tests[i].repository && !ctx->tests[i].script) {
                // skip unused test records
                continue;
            }
            msg(STASIS_MSG_L2, "Executing tests for %s %s\n", ctx->tests[i].name, ctx->tests[i].version);
            if (!ctx->tests[i].script || !strlen(ctx->tests[i].script)) {
                msg(STASIS_MSG_WARN | STASIS_MSG_L3, "Nothing to do. To fix, declare a 'script' in section: [test:%s]\n",
                    ctx->tests[i].name);
                continue;
            }

            char destdir[PATH_MAX];
            sprintf(destdir, "%s/%s", ctx->storage.build_sources_dir, path_basename(ctx->tests[i].repository));

            if (!access(destdir, F_OK)) {
                msg(STASIS_MSG_L3, "Purging repository %s\n", destdir);
                if (rmtree(destdir)) {
                    COE_CHECK_ABORT(1, "Unable to remove repository\n");
                }
            }
            msg(STASIS_MSG_L3, "Cloning repository %s\n", ctx->tests[i].repository);
            if (!git_clone(&proc, ctx->tests[i].repository, destdir, ctx->tests[i].version)) {
                ctx->tests[i].repository_info_tag = strdup(git_describe(destdir));
                ctx->tests[i].repository_info_ref = strdup(git_rev_parse(destdir, "HEAD"));
            } else {
                COE_CHECK_ABORT(1, "Unable to clone repository\n");
            }

            if (ctx->tests[i].repository_remove_tags && strlist_count(ctx->tests[i].repository_remove_tags)) {
                filter_repo_tags(destdir, ctx->tests[i].repository_remove_tags);
            }

            if (pushd(destdir)) {
                COE_CHECK_ABORT(1, "Unable to enter repository directory\n");
            } else {
#if 1
                int status;
                char *cmd = calloc(strlen(ctx->tests[i].script) + STASIS_BUFSIZ, sizeof(*cmd));

                msg(STASIS_MSG_L3, "Testing %s\n", ctx->tests[i].name);
                memset(&proc, 0, sizeof(proc));

                // Apply workaround for tox positional arguments
                char *toxconf = NULL;
                if (!access("tox.ini", F_OK)) {
                    if (!fix_tox_conf("tox.ini", &toxconf)) {
                        msg(STASIS_MSG_L3, "Fixing tox positional arguments\n");
                        if (!globals.workaround.tox_posargs) {
                            globals.workaround.tox_posargs = calloc(PATH_MAX, sizeof(*globals.workaround.tox_posargs));
                        } else {
                            memset(globals.workaround.tox_posargs, 0, PATH_MAX);
                        }
                        snprintf(globals.workaround.tox_posargs, PATH_MAX - 1, "-c %s --root .", toxconf);
                    }
                }

                // enable trace mode before executing each test script

                strcpy(cmd, ctx->tests[i].script);
                char *cmd_rendered = tpl_render(cmd);
                if (cmd_rendered) {
                    if (strcmp(cmd_rendered, cmd) != 0) {
                        strcpy(cmd, cmd_rendered);
                        cmd[strlen(cmd_rendered) ? strlen(cmd_rendered) - 1 : 0] = 0;
                    }
                    guard_free(cmd_rendered);
                }

                FILE *runner_fp;
                char *runner_filename = xmkstemp(&runner_fp, "w");

                fprintf(runner_fp, "#!/bin/bash\n"
                                   "eval `conda shell.posix reactivate`\n"
                                   "set -x\n"
                                   "%s\n",
                        cmd);
                fclose(runner_fp);
                chmod(runner_filename, 0755);

                puts(cmd);
                char runner_cmd[PATH_MAX] = {0};
                sprintf(runner_cmd, "%s", runner_filename);
                status = shell(&proc, runner_cmd);
                if (status) {
                    msg(STASIS_MSG_ERROR, "Script failure: %s\n%s\n\nExit code: %d\n", ctx->tests[i].name, ctx->tests[i].script, status);
                    remove(runner_filename);
                    popd();
                    guard_free(cmd);
                    tpl_free();
                    delivery_free(ctx);
                    globals_free();
                    COE_CHECK_ABORT(1, "Test failure");
                }
                guard_free(cmd);
                remove(runner_filename);
                guard_free(runner_filename);

                if (toxconf) {
                    remove(toxconf);
                    guard_free(toxconf);
                }
                popd();
#else
                msg(STASIS_MSG_WARNING | STASIS_MSG_L3, "TESTING DISABLED BY CODE!\n");
#endif
            }
        }
    }
}

void delivery_gather_tool_versions(struct Delivery *ctx) {
    int status = 0;

    // Extract version from tool output
    ctx->conda.tool_version = shell_output("conda --version", &status);
    if (ctx->conda.tool_version)
        strip(ctx->conda.tool_version);

    ctx->conda.tool_build_version = shell_output("conda build --version", &status);
    if (ctx->conda.tool_build_version)
        strip(ctx->conda.tool_version);
}

int delivery_init_artifactory(struct Delivery *ctx) {
    int status = 0;
    char dest[PATH_MAX] = {0};
    char filepath[PATH_MAX] = {0};
    snprintf(dest, sizeof(dest) - 1, "%s/bin", ctx->storage.tools_dir);
    snprintf(filepath, sizeof(dest) - 1, "%s/bin/jf", ctx->storage.tools_dir);

    if (!access(filepath, F_OK)) {
        // already have it
        msg(STASIS_MSG_L3, "Skipped download, %s already exists\n", filepath);
        goto delivery_init_artifactory_envsetup;
    }

    char *platform = ctx->system.platform[DELIVERY_PLATFORM];
    msg(STASIS_MSG_L3, "Downloading %s for %s %s\n", globals.jfrog.remote_filename, platform, ctx->system.arch);
    if ((status = artifactory_download_cli(dest,
            globals.jfrog.jfrog_artifactory_base_url,
            globals.jfrog.jfrog_artifactory_product,
            globals.jfrog.cli_major_ver,
            globals.jfrog.version,
            platform,
            ctx->system.arch,
            globals.jfrog.remote_filename))) {
        remove(filepath);
    }

    delivery_init_artifactory_envsetup:
    // CI (ridiculously generic, why?) disables interactive prompts and progress bar output
    setenv("CI", "1", 1);

    // JFROG_CLI_HOME_DIR is where .jfrog is stored
    char path[PATH_MAX] = {0};
    snprintf(path, sizeof(path) - 1, "%s/.jfrog", ctx->storage.build_dir);
    setenv("JFROG_CLI_HOME_DIR", path, 1);

    // JFROG_CLI_TEMP_DIR is where the obvious is stored
    setenv("JFROG_CLI_TEMP_DIR", ctx->storage.tmpdir, 1);
    return status;
}

int delivery_artifact_upload(struct Delivery *ctx) {
    int status = 0;

    if (jfrt_auth_init(&ctx->deploy.jfrog_auth)) {
        fprintf(stderr, "Failed to initialize Artifactory authentication context\n");
        return -1;
    }

    for (size_t i = 0; i < sizeof(ctx->deploy.jfrog) / sizeof(*ctx->deploy.jfrog); i++) {
        if (!ctx->deploy.jfrog[i].files || !ctx->deploy.jfrog[i].dest) {
            break;
        }
        jfrt_upload_init(&ctx->deploy.jfrog[i].upload_ctx);

        if (!globals.jfrog.repo) {
            msg(STASIS_MSG_WARN, "Artifactory repository path is not configured!\n");
            fprintf(stderr, "set STASIS_JF_REPO environment variable...\nOr append to configuration file:\n\n");
            fprintf(stderr, "[deploy:artifactory]\nrepo = example/generic/repo/path\n\n");
            status++;
            break;
        } else if (!ctx->deploy.jfrog[i].repo) {
            ctx->deploy.jfrog[i].repo = strdup(globals.jfrog.repo);
        }

        if (!ctx->deploy.jfrog[i].repo || isempty(ctx->deploy.jfrog[i].repo) || !strlen(ctx->deploy.jfrog[i].repo)) {
            // Unlikely to trigger if the config parser is working correctly
            msg(STASIS_MSG_ERROR, "Artifactory repository path is empty. Cannot continue.\n");
            status++;
            break;
        }

        ctx->deploy.jfrog[i].upload_ctx.workaround_parent_only = true;
        ctx->deploy.jfrog[i].upload_ctx.build_name = ctx->info.build_name;
        ctx->deploy.jfrog[i].upload_ctx.build_number = ctx->info.build_number;

        char files[PATH_MAX];
        char dest[PATH_MAX];  // repo + remote dir

        if (jfrog_cli_rt_ping(&ctx->deploy.jfrog_auth)) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "Unable to contact artifactory server: %s\n", ctx->deploy.jfrog_auth.url);
            return -1;
        }

        if (strlist_count(ctx->deploy.jfrog[i].files)) {
            for (size_t f = 0; f < strlist_count(ctx->deploy.jfrog[i].files); f++) {
                memset(dest, 0, sizeof(dest));
                memset(files, 0, sizeof(files));
                snprintf(dest, sizeof(dest) - 1, "%s/%s", ctx->deploy.jfrog[i].repo, ctx->deploy.jfrog[i].dest);
                snprintf(files, sizeof(files) - 1, "%s", strlist_item(ctx->deploy.jfrog[i].files, f));
                status += jfrog_cli_rt_upload(&ctx->deploy.jfrog_auth, &ctx->deploy.jfrog[i].upload_ctx, files, dest);
            }
        }
    }

    if (!status && ctx->deploy.jfrog[0].files && ctx->deploy.jfrog[0].dest) {
        jfrog_cli_rt_build_collect_env(&ctx->deploy.jfrog_auth, ctx->deploy.jfrog[0].upload_ctx.build_name, ctx->deploy.jfrog[0].upload_ctx.build_number);
        jfrog_cli_rt_build_publish(&ctx->deploy.jfrog_auth, ctx->deploy.jfrog[0].upload_ctx.build_name, ctx->deploy.jfrog[0].upload_ctx.build_number);
    }

    return status;
}

int delivery_mission_render_files(struct Delivery *ctx) {
    if (!ctx->storage.mission_dir) {
        fprintf(stderr, "Mission directory is not configured. Context not initialized?\n");
        return -1;
    }
    struct Data {
        char *src;
        char *dest;
    } data;
    struct INIFILE *cfg = ctx->_stasis_ini_fp.mission;
    union INIVal val;

    memset(&data, 0, sizeof(data));
    data.src = calloc(PATH_MAX, sizeof(*data.src));
    if (!data.src) {
        perror("data.src");
        return -1;
    }

    for (size_t i = 0; i < cfg->section_count; i++) {
        char *section_name = cfg->section[i]->key;
        if (!startswith(section_name, "template:")) {
            continue;
        }
        val.as_char_p = strchr(section_name, ':') + 1;
        if (val.as_char_p && isempty(val.as_char_p)) {
            guard_free(data.src);
            return 1;
        }
        sprintf(data.src, "%s/%s/%s", ctx->storage.mission_dir, ctx->meta.mission, val.as_char_p);
        msg(STASIS_MSG_L2, "%s\n", data.src);

        ini_getval_required(cfg, section_name, "destination", INIVAL_TYPE_STR, &val);
        conv_str(&data.dest, val);

        char *contents;
        struct stat st;
        if (lstat(data.src, &st)) {
            perror(data.src);
            guard_free(data.dest);
            continue;
        }

        contents = calloc(st.st_size + 1, sizeof(*contents));
        if (!contents) {
            perror("template file contents");
            guard_free(data.dest);
            continue;
        }

        FILE *fp;
        fp = fopen(data.src, "rb");
        if (!fp) {
            perror(data.src);
            guard_free(contents);
            guard_free(data.dest);
            continue;
        }

        if (fread(contents, st.st_size, sizeof(*contents), fp) < 1) {
            perror("while reading template file");
            guard_free(contents);
            guard_free(data.dest);
            fclose(fp);
            continue;
        }
        fclose(fp);

        msg(STASIS_MSG_L3, "Writing %s\n", data.dest);
        if (tpl_render_to_file(contents, data.dest)) {
            guard_free(contents);
            guard_free(data.dest);
            continue;
        }
        guard_free(contents);
        guard_free(data.dest);
    }

    guard_free(data.src);
    return 0;
}

int delivery_docker(struct Delivery *ctx) {
    if (!docker_capable(&ctx->deploy.docker.capabilities)) {
        return -1;
    }
    char tag[STASIS_NAME_MAX];
    char args[PATH_MAX];
    int has_registry = ctx->deploy.docker.registry != NULL;
    size_t total_tags = strlist_count(ctx->deploy.docker.tags);
    size_t total_build_args = strlist_count(ctx->deploy.docker.build_args);

    if (!has_registry) {
        msg(STASIS_MSG_WARN | STASIS_MSG_L2, "No docker registry defined. You will need to manually retag the resulting image.\n");
    }

    if (!total_tags) {
        char default_tag[PATH_MAX];
        msg(STASIS_MSG_WARN | STASIS_MSG_L2, "No docker tags defined by configuration. Generating default tag(s).\n");
        // generate local tag
        memset(default_tag, 0, sizeof(default_tag));
        sprintf(default_tag, "%s:%s-py%s", ctx->meta.name, ctx->info.build_name, ctx->meta.python_compact);
        tolower_s(default_tag);

        // Add tag
        ctx->deploy.docker.tags = strlist_init();
        strlist_append(&ctx->deploy.docker.tags, default_tag);

        if (has_registry) {
            // generate tag for target registry
            memset(default_tag, 0, sizeof(default_tag));
            sprintf(default_tag, "%s/%s:%s-py%s", ctx->deploy.docker.registry, ctx->meta.name, ctx->info.build_number, ctx->meta.python_compact);
            tolower_s(default_tag);

            // Add tag
            strlist_append(&ctx->deploy.docker.tags, default_tag);
        }
        // regenerate total tag available
        total_tags = strlist_count(ctx->deploy.docker.tags);
    }

    memset(args, 0, sizeof(args));

    // Append image tags to command
    for (size_t i = 0; i < total_tags; i++) {
        char *tag_orig = strlist_item(ctx->deploy.docker.tags, i);
        strcpy(tag, tag_orig);
        docker_sanitize_tag(tag);
        sprintf(args + strlen(args), " -t \"%s\" ", tag);
    }

    // Append build arguments to command (i.e. --build-arg "key=value"
    for (size_t i = 0; i < total_build_args; i++) {
        char *build_arg = strlist_item(ctx->deploy.docker.build_args, i);
        if (!build_arg) {
            break;
        }
        sprintf(args + strlen(args), " --build-arg \"%s\" ", build_arg);
    }

    // Build the image
    char delivery_file[PATH_MAX];
    char dest[PATH_MAX];
    char rsync_cmd[PATH_MAX * 2];
    memset(delivery_file, 0, sizeof(delivery_file));
    memset(dest, 0, sizeof(dest));

    sprintf(delivery_file, "%s/%s.yml", ctx->storage.delivery_dir, ctx->info.release_name);
    if (access(delivery_file, F_OK) < 0) {
        fprintf(stderr, "docker build cannot proceed without delivery file: %s\n", delivery_file);
        return -1;
    }

    sprintf(dest, "%s/%s.yml", ctx->storage.build_docker_dir, ctx->info.release_name);
    if (copy2(delivery_file, dest, CT_PERM)) {
        fprintf(stderr, "Failed to copy delivery file to %s: %s\n", dest, strerror(errno));
        return -1;
    }

    memset(dest, 0, sizeof(dest));
    sprintf(dest, "%s/packages", ctx->storage.build_docker_dir);

    msg(STASIS_MSG_L2, "Copying conda packages\n");
    memset(rsync_cmd, 0, sizeof(rsync_cmd));
    sprintf(rsync_cmd, "rsync -avi --progress '%s' '%s'", ctx->storage.conda_artifact_dir, dest);
    if (system(rsync_cmd)) {
        fprintf(stderr, "Failed to copy conda artifacts to docker build directory\n");
        return -1;
    }

    msg(STASIS_MSG_L2, "Copying wheel packages\n");
    memset(rsync_cmd, 0, sizeof(rsync_cmd));
    sprintf(rsync_cmd, "rsync -avi --progress '%s' '%s'", ctx->storage.wheel_artifact_dir, dest);
    if (system(rsync_cmd)) {
        fprintf(stderr, "Failed to copy wheel artifactory to docker build directory\n");
    }

    if (docker_build(ctx->storage.build_docker_dir, args, ctx->deploy.docker.capabilities.build)) {
        return -1;
    }

    // Test the image
    // All tags point back to the same image so test the first one we see
    // regardless of how many are defined
    strcpy(tag, strlist_item(ctx->deploy.docker.tags, 0));
    docker_sanitize_tag(tag);

    msg(STASIS_MSG_L2, "Executing image test script for %s\n", tag);
    if (ctx->deploy.docker.test_script) {
        if (isempty(ctx->deploy.docker.test_script)) {
            msg(STASIS_MSG_L2 | STASIS_MSG_WARN, "Image test script has no content\n");
        } else {
            int state;
            if ((state = docker_script(tag, ctx->deploy.docker.test_script, 0))) {
                msg(STASIS_MSG_L2 | STASIS_MSG_ERROR, "Non-zero exit (%d) from test script. %s image archive will not be generated.\n", state >> 8, tag);
                // test failed -- don't save the image
                return -1;
            }
        }
    } else {
        msg(STASIS_MSG_L2 | STASIS_MSG_WARN, "No image test script defined\n");
    }

    // Test successful, save image
    if (docker_save(path_basename(tag), ctx->storage.docker_artifact_dir, ctx->deploy.docker.image_compression)) {
        // save failed
        return -1;
    }

    return 0;
}

int delivery_fixup_test_results(struct Delivery *ctx) {
    struct dirent *rec;
    DIR *dp;

    dp = opendir(ctx->storage.results_dir);
    if (!dp) {
        perror(ctx->storage.results_dir);
        return -1;
    }

    while ((rec = readdir(dp)) != NULL) {
        char path[PATH_MAX];
        memset(path, 0, sizeof(path));

        if (!strcmp(rec->d_name, ".") || !strcmp(rec->d_name, "..")) {
            continue;
        } else if (!endswith(rec->d_name, ".xml")) {
            continue;
        }

        sprintf(path, "%s/%s", ctx->storage.results_dir, rec->d_name);
        msg(STASIS_MSG_L2, "%s\n", rec->d_name);
        if (xml_pretty_print_in_place(path, STASIS_XML_PRETTY_PRINT_PROG, STASIS_XML_PRETTY_PRINT_ARGS)) {
            msg(STASIS_MSG_L3 | STASIS_MSG_WARN, "Failed to rewrite file '%s'\n", rec->d_name);
        }
    }

    closedir(dp);
    return 0;
}

int delivery_exists(struct Delivery *ctx) {
    // TODO: scan artifactory repo for the same information
    char release_pattern[PATH_MAX] = {0};
    sprintf(release_pattern, "*%s*", ctx->info.release_name);
    struct StrList *files = listdir(ctx->storage.delivery_dir);
    for (size_t i = 0; i < strlist_count(files); i++) {
        char *filename = strlist_item(files, i);
        int release_exists = fnmatch(release_pattern, filename, FNM_PATHNAME);
        if (!globals.enable_overwrite && !release_exists) {
            guard_strlist_free(&files);
            return 1;
        }
    }
    guard_strlist_free(&files);
    return 0;
}