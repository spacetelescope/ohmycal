#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <getopt.h>
#include "core.h"

#define OPT_ALWAYS_UPDATE_BASE 1000
#define OPT_NO_DOCKER 1001
#define OPT_NO_ARTIFACTORY 1002
#define OPT_NO_TESTING 1003
#define OPT_OVERWRITE 1004
static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},
        {"continue-on-error", no_argument, 0, 'C'},
        {"config", required_argument, 0, 'c'},
        {"python", required_argument, 0, 'p'},
        {"verbose", no_argument, 0, 'v'},
        {"unbuffered", no_argument, 0, 'U'},
        {"update-base", no_argument, 0, OPT_ALWAYS_UPDATE_BASE},
        {"overwrite", no_argument, 0, OPT_OVERWRITE},
        {"no-docker", no_argument, 0, OPT_NO_DOCKER},
        {"no-artifactory", no_argument, 0, OPT_NO_ARTIFACTORY},
        {"no-testing", no_argument, 0, OPT_NO_TESTING},
        {0, 0, 0, 0},
};

const char *long_options_help[] = {
        "Display this usage statement",
        "Display program version",
        "Allow tests to fail",
        "Read configuration file",
        "Override version of Python in configuration",
        "Increase output verbosity",
        "Disable line buffering",
        "Update conda installation prior to STASIS environment creation",
        "Overwrite an existing release",
        "Do not build docker images",
        "Do not upload artifacts to Artifactory",
        "Do not execute test scripts",
        NULL,
};

static int get_option_max_width(struct option option[]) {
    int i = 0;
    int max = 0;
    const int indent = 4;
    while (option[i].name != 0) {
        int len = (int) strlen(option[i].name);
        if (option[i].has_arg) {
            len += indent;
        }
        if (len > max) {
            max = len;
        }
        i++;
    }
    return max;
}

static void usage(char *progname) {
    printf("usage: %s ", progname);
    printf("[-");
    for (int x = 0; long_options[x].val != 0; x++) {
        if (long_options[x].has_arg == no_argument && long_options[x].val <= 'z') {
            putchar(long_options[x].val);
        }
    }
    printf("] {DELIVERY_FILE}\n");

    int width = get_option_max_width(long_options);
    for (int x = 0; long_options[x].name != 0; x++) {
        char tmp[STASIS_NAME_MAX] = {0};
        char output[sizeof(tmp)] = {0};
        char opt_long[50] = {0};        // --? [ARG]?
        char opt_short[50] = {0};        // -? [ARG]?

        strcat(opt_long, "--");
        strcat(opt_long, long_options[x].name);
        if (long_options[x].has_arg) {
            strcat(opt_long, " ARG");
        }

        if (long_options[x].val <= 'z') {
            strcat(opt_short, "-");
            opt_short[1] = (char) long_options[x].val;
            if (long_options[x].has_arg) {
                strcat(opt_short, " ARG");
            }
        } else {
            strcat(opt_short, "  ");
        }

        sprintf(tmp, "  %%-%ds\t%%s\t\t%%s", width + 4);
        sprintf(output, tmp, opt_long, opt_short, long_options_help[x]);
        puts(output);
    }
}



static void check_system_requirements(struct Delivery *ctx) {
    const char *tools_required[] = {
        "rsync",
        NULL,
    };

    msg(STASIS_MSG_L1, "Checking system requirements\n");
    for (size_t i = 0; tools_required[i] != NULL; i++) {
        if (!find_program(tools_required[i])) {
            msg(STASIS_MSG_L2 | STASIS_MSG_ERROR, "'%s' must be installed.\n", tools_required[i]);
            exit(1);
        }
    }

    if (!globals.tmpdir && !ctx->storage.tmpdir) {
        delivery_init_tmpdir(ctx);
    }

    struct DockerCapabilities dcap;
    if (!docker_capable(&dcap)) {
        msg(STASIS_MSG_L2 | STASIS_MSG_WARN, "Docker is broken\n");
        msg(STASIS_MSG_L3, "Available: %s\n", dcap.available ? "Yes" : "No");
        msg(STASIS_MSG_L3, "Usable: %s\n", dcap.usable ? "Yes" : "No");
        msg(STASIS_MSG_L3, "Podman [Docker Emulation]: %s\n", dcap.podman ? "Yes" : "No");
        msg(STASIS_MSG_L3, "Build plugin(s): ");
        if (dcap.usable) {
            if (dcap.build & STASIS_DOCKER_BUILD) {
                printf("build ");
            }
            if (dcap.build & STASIS_DOCKER_BUILD_X) {
                printf("buildx ");
            }
            puts("");
        } else {
            printf("N/A\n");
        }

        // disable docker builds
        globals.enable_docker = false;
    }
}

int main(int argc, char *argv[]) {
    struct Delivery ctx;
    struct Process proc = {
            .f_stdout = "",
            .f_stderr = "",
            .redirect_stderr = 0,
    };
    char env_name[STASIS_NAME_MAX] = {0};
    char env_name_testing[STASIS_NAME_MAX] = {0};
    char *delivery_input = NULL;
    char *config_input = NULL;
    char installer_url[PATH_MAX];
    char python_override_version[STASIS_NAME_MAX];
    int user_disabled_docker = false;

    memset(env_name, 0, sizeof(env_name));
    memset(env_name_testing, 0, sizeof(env_name_testing));
    memset(installer_url, 0, sizeof(installer_url));
    memset(python_override_version, 0, sizeof(python_override_version));
    memset(&proc, 0, sizeof(proc));
    memset(&ctx, 0, sizeof(ctx));

    int c;
    int option_index = 0;
    while ((c = getopt_long(argc, argv, "hVCc:p:vU", long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                usage(path_basename(argv[0]));
                exit(0);
            case 'V':
                puts(VERSION);
                exit(0);
            case 'c':
                config_input = strdup(optarg);
                break;
            case 'C':
                globals.continue_on_error = true;
                break;
            case 'p':
                strcpy(python_override_version, optarg);
                break;
            case OPT_ALWAYS_UPDATE_BASE:
                globals.always_update_base_environment = true;
                break;
            case 'U':
                setenv("PYTHONUNBUFFERED", "1", 1);
                fflush(stdout);
                fflush(stderr);
                setvbuf(stdout, NULL, _IONBF, 0);
                setvbuf(stderr, NULL, _IONBF, 0);
                break;
            case 'v':
                globals.verbose = true;
                break;
            case OPT_OVERWRITE:
                globals.enable_overwrite = true;
                break;
            case OPT_NO_DOCKER:
                globals.enable_docker = false;
                user_disabled_docker = true;
                break;
            case OPT_NO_ARTIFACTORY:
                globals.enable_artifactory = false;
                break;
            case OPT_NO_TESTING:
                globals.enable_testing = false;
                break;
            case '?':
            default:
                exit(1);
        }
    }

    if (optind < argc) {
        while (optind < argc) {
            // use first positional argument
            delivery_input = argv[optind++];
            break;
        }
    }

    if (!delivery_input) {
        fprintf(stderr, "error: a DELIVERY_FILE is required\n");
        usage(path_basename(argv[0]));
        exit(1);
    }

    printf(BANNER, VERSION, AUTHOR);

    msg(STASIS_MSG_L1, "Setup\n");

    // Expose variables for use with the template engine
    // NOTE: These pointers are populated by delivery_init() so please avoid using
    // tpl_render() until then.
    tpl_register("meta.name", &ctx.meta.name);
    tpl_register("meta.version", &ctx.meta.version);
    tpl_register("meta.codename", &ctx.meta.codename);
    tpl_register("meta.mission", &ctx.meta.mission);
    tpl_register("meta.python", &ctx.meta.python);
    tpl_register("meta.python_compact", &ctx.meta.python_compact);
    tpl_register("info.time_str_epoch", &ctx.info.time_str_epoch);
    tpl_register("info.release_name", &ctx.info.release_name);
    tpl_register("info.build_name", &ctx.info.build_name);
    tpl_register("info.build_number", &ctx.info.build_number);
    tpl_register("storage.tmpdir", &ctx.storage.tmpdir);
    tpl_register("storage.output_dir", &ctx.storage.output_dir);
    tpl_register("storage.delivery_dir", &ctx.storage.delivery_dir);
    tpl_register("storage.conda_artifact_dir", &ctx.storage.conda_artifact_dir);
    tpl_register("storage.wheel_artifact_dir", &ctx.storage.wheel_artifact_dir);
    tpl_register("storage.build_sources_dir", &ctx.storage.build_sources_dir);
    tpl_register("storage.build_docker_dir", &ctx.storage.build_docker_dir);
    tpl_register("storage.results_dir", &ctx.storage.results_dir);
    tpl_register("storage.tools_dir", &ctx.storage.tools_dir);
    tpl_register("conda.installer_baseurl", &ctx.conda.installer_baseurl);
    tpl_register("conda.installer_name", &ctx.conda.installer_name);
    tpl_register("conda.installer_version", &ctx.conda.installer_version);
    tpl_register("conda.installer_arch", &ctx.conda.installer_arch);
    tpl_register("conda.installer_platform", &ctx.conda.installer_platform);
    tpl_register("deploy.jfrog.repo", &globals.jfrog.repo);
    tpl_register("deploy.jfrog.url", &globals.jfrog.url);
    tpl_register("deploy.docker.registry", &ctx.deploy.docker.registry);
    tpl_register("workaround.tox_posargs", &globals.workaround.tox_posargs);
    tpl_register("workaround.conda_reactivate", &globals.workaround.conda_reactivate);

    // Expose function(s) to the template engine
    // Prototypes can be found in template_func_proto.h
    tpl_register_func("get_github_release_notes", &get_github_release_notes_tplfunc_entrypoint, 3, NULL);
    tpl_register_func("get_github_release_notes_auto", &get_github_release_notes_auto_tplfunc_entrypoint, 1, &ctx);

    // Set up PREFIX/etc directory information
    // The user may manipulate the base directory path with STASIS_SYSCONFDIR
    // environment variable
    char stasis_sysconfdir_tmp[PATH_MAX];
    if (getenv("STASIS_SYSCONFDIR")) {
        strncpy(stasis_sysconfdir_tmp, getenv("STASIS_SYSCONFDIR"), sizeof(stasis_sysconfdir_tmp) - 1);
    } else {
        strncpy(stasis_sysconfdir_tmp, STASIS_SYSCONFDIR, sizeof(stasis_sysconfdir_tmp) - 1);
    }

    globals.sysconfdir = realpath(stasis_sysconfdir_tmp, NULL);
    if (!globals.sysconfdir) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L1, "Unable to resolve path to configuration directory: %s\n", stasis_sysconfdir_tmp);
        exit(1);
    }

    // Override Python version from command-line, if any
    if (strlen(python_override_version)) {
        guard_free(ctx.meta.python);
        ctx.meta.python = strdup(python_override_version);
        guard_free(ctx.meta.python_compact);
        ctx.meta.python_compact = to_short_version(ctx.meta.python);
    }

    if (!config_input) {
        // no configuration passed by argument. use basic config.
        char cfgfile[PATH_MAX * 2];
        sprintf(cfgfile, "%s/%s", globals.sysconfdir, "stasis.ini");
        if (!access(cfgfile, F_OK | R_OK)) {
            config_input = strdup(cfgfile);
        } else {
            msg(STASIS_MSG_WARN, "STASIS global configuration is not readable, or does not exist: %s", cfgfile);
        }
    }

    if (config_input) {
        msg(STASIS_MSG_L2, "Reading STASIS global configuration: %s\n", config_input);
        ctx._stasis_ini_fp.cfg = ini_open(config_input);
        if (!ctx._stasis_ini_fp.cfg) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "Failed to read config file: %s, %s\n", delivery_input, strerror(errno));
            exit(1);
        }
        ctx._stasis_ini_fp.cfg_path = strdup(config_input);
        guard_free(config_input);
    }

    msg(STASIS_MSG_L2, "Reading STASIS delivery configuration: %s\n", delivery_input);
    ctx._stasis_ini_fp.delivery = ini_open(delivery_input);
    if (!ctx._stasis_ini_fp.delivery) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "Failed to read delivery file: %s, %s\n", delivery_input, strerror(errno));
        exit(1);
    }
    ctx._stasis_ini_fp.delivery_path = strdup(delivery_input);

    msg(STASIS_MSG_L2, "Bootstrapping delivery context\n");
    if (bootstrap_build_info(&ctx)) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "Failed to bootstrap delivery context\n");
        exit(1);
    }

    msg(STASIS_MSG_L2, "Initializing delivery context\n");
    if (delivery_init(&ctx)) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "Failed to initialize delivery context\n");
        exit(1);
    }
    check_system_requirements(&ctx);

    msg(STASIS_MSG_L2, "Configuring JFrog CLI\n");
    if (delivery_init_artifactory(&ctx)) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "JFrog CLI configuration failed\n");
        exit(1);
    }

    runtime_apply(ctx.runtime.environ);
    strcpy(env_name, ctx.info.release_name);
    strcpy(env_name_testing, env_name);
    strcat(env_name_testing, "-test");

    msg(STASIS_MSG_L1, "Overview\n");
    delivery_meta_show(&ctx);
    delivery_conda_show(&ctx);
    delivery_tests_show(&ctx);
    if (globals.verbose) {
        //delivery_runtime_show(&ctx);
    }

    // Safety gate: Avoid clobbering a delivery unless the user wants that behavior
    if (delivery_exists(&ctx)) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L1, "Refusing to overwrite delivery: %s\nUse --overwrite to enable release clobbering.\n", ctx.info.release_name);
        exit(1);
    }

    msg(STASIS_MSG_L1, "Conda setup\n");
    delivery_get_installer_url(&ctx, installer_url);
    msg(STASIS_MSG_L2, "Downloading: %s\n", installer_url);
    if (delivery_get_installer(&ctx, installer_url)) {
        msg(STASIS_MSG_ERROR, "download failed: %s\n", installer_url);
        exit(1);
    }

    // Unlikely to occur: this should help prevent rmtree() from destroying your entire filesystem
    // if path is "/" then, die
    // or if empty string, die
    if (!strcmp(ctx.storage.conda_install_prefix, DIR_SEP) || !strlen(ctx.storage.conda_install_prefix)) {
        fprintf(stderr, "error: ctx.storage.conda_install_prefix is malformed!\n");
        exit(1);
    }

    msg(STASIS_MSG_L2, "Installing: %s\n", ctx.conda.installer_name);
    delivery_install_conda(ctx.conda.installer_path, ctx.storage.conda_install_prefix);

    msg(STASIS_MSG_L2, "Configuring: %s\n", ctx.storage.conda_install_prefix);
    delivery_conda_enable(&ctx, ctx.storage.conda_install_prefix);

    char *pathvar = NULL;
    pathvar = getenv("PATH");
    if (!pathvar) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "PATH variable is not set. Cannot continue.\n");
        exit(1);
    } else {
        char pathvar_tmp[STASIS_BUFSIZ];
        sprintf(pathvar_tmp, "%s/bin:%s", ctx.storage.conda_install_prefix, pathvar);
        setenv("PATH", pathvar_tmp, 1);
        pathvar = NULL;
    }

    msg(STASIS_MSG_L1, "Creating release environment(s)\n");
    if (ctx.meta.based_on && strlen(ctx.meta.based_on)) {
        if (conda_env_remove(env_name)) {
           msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "failed to remove release environment: %s\n", env_name_testing);
           exit(1);
        }
        msg(STASIS_MSG_L2, "Based on release: %s\n", ctx.meta.based_on);
        if (conda_env_create_from_uri(env_name, ctx.meta.based_on)) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "unable to install release environment using configuration file\n");
            exit(1);
        }

        if (conda_env_remove(env_name_testing)) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "failed to remove testing environment\n");
            exit(1);
        }
        if (conda_env_create_from_uri(env_name_testing, ctx.meta.based_on)) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "unable to install testing environment using configuration file\n");
            exit(1);
        }
    } else {
        if (conda_env_create(env_name, ctx.meta.python, NULL)) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "failed to create release environment\n");
            exit(1);
        }
        if (conda_env_create(env_name_testing, ctx.meta.python, NULL)) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "failed to create release environment\n");
            exit(1);
        }
    }

    // Activate test environment
    msg(STASIS_MSG_L1, "Activating test environment\n");
    if (conda_activate(ctx.storage.conda_install_prefix, env_name_testing)) {
        fprintf(stderr, "failed to activate test environment\n");
        exit(1);
    }

    delivery_gather_tool_versions(&ctx);
    if (!ctx.conda.tool_version) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "Could not determine conda version\n");
        exit(1);
    }
    if (!ctx.conda.tool_build_version) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "Could not determine conda-build version\n");
        exit(1);
    }

    if (pip_exec("install build")) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "'build' tool installation failed");
        exit(1);
    }

    // Execute configuration-defined tests
    if (globals.enable_testing) {
        msg(STASIS_MSG_L1, "Begin test execution\n");
        delivery_tests_run(&ctx);
        msg(STASIS_MSG_L1, "Rewriting test results\n");
        delivery_fixup_test_results(&ctx);
    } else {
        msg(STASIS_MSG_L1 | STASIS_MSG_WARN, "Test execution is disabled\n");
    }

    msg(STASIS_MSG_L1, "Generating deferred package listing\n");
    // Test succeeded so move on to producing package artifacts
    delivery_defer_packages(&ctx, DEFER_CONDA);
    delivery_defer_packages(&ctx, DEFER_PIP);

    if (ctx.conda.conda_packages_defer && strlist_count(ctx.conda.conda_packages_defer)) {
        msg(STASIS_MSG_L2, "Building Conda recipe(s)\n");
        if (delivery_build_recipes(&ctx)) {
            exit(1);
        }
        msg(STASIS_MSG_L3, "Copying artifacts\n");
        if (delivery_copy_conda_artifacts(&ctx)) {
            exit(1);
        }
        msg(STASIS_MSG_L3, "Indexing artifacts\n");
        if (delivery_index_conda_artifacts(&ctx)) {
            exit(1);
        }
    }

    if (strlist_count(ctx.conda.pip_packages_defer)) {
        if (!(ctx.conda.wheels_packages = delivery_build_wheels(&ctx))) {
            exit(1);
        }
        if (delivery_index_wheel_artifacts(&ctx)) {
            exit(1);
        }

    }

    // Populate the release environment
    msg(STASIS_MSG_L1, "Populating release environment\n");
    msg(STASIS_MSG_L2, "Installing conda packages\n");
    if (strlist_count(ctx.conda.conda_packages)) {
        if (delivery_install_packages(&ctx, ctx.storage.conda_install_prefix, env_name, INSTALL_PKG_CONDA, (struct StrList *[]) {ctx.conda.conda_packages, NULL})) {
            exit(1);
        }
    }
    if (strlist_count(ctx.conda.conda_packages_defer)) {
        msg(STASIS_MSG_L3, "Installing deferred conda packages\n");
        if (delivery_install_packages(&ctx, ctx.storage.conda_install_prefix, env_name, INSTALL_PKG_CONDA | INSTALL_PKG_CONDA_DEFERRED, (struct StrList *[]) {ctx.conda.conda_packages_defer, NULL})) {
            exit(1);
        }
    } else {
        msg(STASIS_MSG_L3, "No deferred conda packages\n");
    }

    msg(STASIS_MSG_L2, "Installing pip packages\n");
    if (strlist_count(ctx.conda.pip_packages)) {
        if (delivery_install_packages(&ctx, ctx.storage.conda_install_prefix, env_name, INSTALL_PKG_PIP, (struct StrList *[]) {ctx.conda.pip_packages, NULL})) {
            exit(1);
        }
    }

    if (strlist_count(ctx.conda.pip_packages_defer)) {
        msg(STASIS_MSG_L3, "Installing deferred pip packages\n");
        if (delivery_install_packages(&ctx, ctx.storage.conda_install_prefix, env_name, INSTALL_PKG_PIP | INSTALL_PKG_PIP_DEFERRED, (struct StrList *[]) {ctx.conda.pip_packages_defer, NULL})) {
            exit(1);
        }
    } else {
        msg(STASIS_MSG_L3, "No deferred pip packages\n");
    }

    conda_exec("list");

    msg(STASIS_MSG_L1, "Creating release\n");
    msg(STASIS_MSG_L2, "Exporting delivery configuration\n");
    if (!pushd(ctx.storage.cfgdump_dir)) {
        char filename[PATH_MAX] = {0};
        sprintf(filename, "%s.ini", ctx.info.release_name);
        FILE *spec = fopen(filename, "w+");
        if (!spec) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "failed %s\n", filename);
            exit(1);
        }
        ini_write(ctx._stasis_ini_fp.delivery, &spec, INI_WRITE_RAW);
        fclose(spec);

        memset(filename, 0, sizeof(filename));
        sprintf(filename, "%s-rendered.ini", ctx.info.release_name);
        spec = fopen(filename, "w+");
        if (!spec) {
            msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "failed %s\n", filename);
            exit(1);
        }
        ini_write(ctx._stasis_ini_fp.delivery, &spec, INI_WRITE_PRESERVE);
        fclose(spec);
        popd();
    } else {
        SYSERROR("Failed to enter directory: %s", ctx.storage.delivery_dir);
        exit(1);
    }

    msg(STASIS_MSG_L2, "Exporting %s\n", env_name_testing);
    if (conda_env_export(env_name_testing, ctx.storage.delivery_dir, env_name_testing)) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "failed %s\n", env_name_testing);
        exit(1);
    }

    msg(STASIS_MSG_L2, "Exporting %s\n", env_name);
    if (conda_env_export(env_name, ctx.storage.delivery_dir, env_name)) {
        msg(STASIS_MSG_ERROR | STASIS_MSG_L2, "failed %s\n", env_name);
        exit(1);
    }

    // Rewrite release environment output (i.e. set package origin(s) to point to the deployment server, etc.)
    char specfile[PATH_MAX];
    sprintf(specfile, "%s/%s.yml", ctx.storage.delivery_dir, env_name);
    msg(STASIS_MSG_L3, "Rewriting release spec file (stage 1): %s\n", path_basename(specfile));
    delivery_rewrite_spec(&ctx, specfile, DELIVERY_REWRITE_SPEC_STAGE_1);

    msg(STASIS_MSG_L1, "Rendering mission templates\n");
    delivery_mission_render_files(&ctx);

    int want_docker = ini_section_search(&ctx._stasis_ini_fp.delivery, INI_SEARCH_BEGINS, "deploy:docker") ? true : false;
    int want_artifactory = ini_section_search(&ctx._stasis_ini_fp.delivery, INI_SEARCH_BEGINS, "deploy:artifactory") ? true : false;

    if (want_docker) {
        if (user_disabled_docker) {
            msg(STASIS_MSG_L1 | STASIS_MSG_WARN, "Docker image building is disabled by CLI argument\n");
        } else {
            char dockerfile[PATH_MAX] = {0};
            sprintf(dockerfile, "%s/%s", ctx.storage.build_docker_dir, "Dockerfile");
            if (globals.enable_docker) {
                if (!access(dockerfile, F_OK)) {
                    msg(STASIS_MSG_L1, "Building Docker image\n");
                    if (delivery_docker(&ctx)) {
                        msg(STASIS_MSG_L1 | STASIS_MSG_ERROR, "Failed to build docker image!\n");
                        COE_CHECK_ABORT(1, "Failed to build docker image");
                    }
                } else {
                    msg(STASIS_MSG_L1 | STASIS_MSG_WARN, "Docker image building is disabled. No Dockerfile found in %s\n", ctx.storage.build_docker_dir);
                }
            } else {
                msg(STASIS_MSG_L1 | STASIS_MSG_WARN, "Docker image building is disabled. System configuration error\n");
            }
        }
    } else {
        msg(STASIS_MSG_L1 | STASIS_MSG_WARN, "Docker image building is disabled. deploy:docker is not configured\n");
    }

    msg(STASIS_MSG_L3, "Rewriting release spec file (stage 2): %s\n", path_basename(specfile));
    delivery_rewrite_spec(&ctx, specfile, DELIVERY_REWRITE_SPEC_STAGE_2);

    msg(STASIS_MSG_L1, "Dumping metadata\n");
    if (delivery_dump_metadata(&ctx)) {
        msg(STASIS_MSG_L1 | STASIS_MSG_ERROR, "Metadata dump failed\n");
    }

    if (want_artifactory) {
        if (globals.enable_artifactory) {
            msg(STASIS_MSG_L1, "Uploading artifacts\n");
            delivery_artifact_upload(&ctx);
        } else {
            msg(STASIS_MSG_L1 | STASIS_MSG_WARN, "Artifact uploading is disabled\n");
        }
    } else {
        msg(STASIS_MSG_L1 | STASIS_MSG_WARN, "Artifact uploading is disabled. deploy:artifactory is not configured\n");
    }

    msg(STASIS_MSG_L1, "Cleaning up\n");
    delivery_free(&ctx);
    globals_free();
    tpl_free();

    msg(STASIS_MSG_L1, "Done!\n");
    return 0;
}

