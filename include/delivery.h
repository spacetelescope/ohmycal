/// @file delivery.h

#ifndef STASIS_DELIVERY_H
#define STASIS_DELIVERY_H

#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/utsname.h>
#include "core.h"

#define DELIVERY_PLATFORM_MAX 4
#define DELIVERY_PLATFORM_MAXLEN 65
#define DELIVERY_PLATFORM 0
#define DELIVERY_PLATFORM_CONDA_SUBDIR 1
#define DELIVERY_PLATFORM_CONDA_INSTALLER 2
#define DELIVERY_PLATFORM_RELEASE 3

#define DELIVERY_REWRITE_SPEC_STAGE_1 0
#define DELIVERY_REWRITE_SPEC_STAGE_2 1

#define INSTALL_PKG_CONDA 1 << 1            ///< Toggle conda package installation
#define INSTALL_PKG_CONDA_DEFERRED 1 << 2   ///< Toggle deferred conda package installation
#define INSTALL_PKG_PIP 1 << 3              ///< Toggle pip package installation
#define INSTALL_PKG_PIP_DEFERRED 1 << 4     ///< Toggle deferred package installation from source

#define DEFER_CONDA 0                       ///< Build conda packages
#define DEFER_PIP 1                         ///< Build python packages

struct Content {
    unsigned type;
    char *filename;
    char *data;
};

/*! \struct Delivery
 *  \brief A structure describing a full delivery object
 */
struct Delivery {
    /*! \struct STASIS_INI_FP
     * \brief Container for INIFILE handles
    */
    struct STASIS_INI_FP {
        struct INIFILE *delivery;
        struct INIFILE *cfg;
        struct INIFILE *mission;
        char *delivery_path;
        char *cfg_path;
        char *mission_path;
    } _stasis_ini_fp;

    /*! \struct System
     * \brief System information
    */
    struct System {
        char *arch;
        ///< System CPU architecture ident
        char **platform;
        ///< System platform name
    } system;
    /*! \struct Storage
     * \brief Storage paths
     */
    struct Storage {
        char *root;                     ///< Top-level storage area
        char *tmpdir;                   ///< Temporary storage area (within root)
        char *output_dir;               ///< Base path to where all artifacts are stored
        char *delivery_dir;             ///< Delivery artifact output directory
        char *cfgdump_dir;              ///< Base path to where input configuration dumps are stored
        char *tools_dir;                ///< Tools storage
        char *mission_dir;              ///< Mission data storage
        char *package_dir;              ///< Base path to where all packages are stored
        char *results_dir;              ///< Base path to where test results are stored
        char *meta_dir;                 ///< Base path to where metadata records are stored
        char *conda_install_prefix;     ///< Path to install Conda
        char *conda_artifact_dir;       ///< Base path to store compiled conda packages
        char *conda_staging_dir;        ///< Base path to copy compiled conda packages
        char *conda_staging_url;        ///< URL to access compiled conda packages
        char *docker_artifact_dir;      ///< Base path to store saved docker images
        char *wheel_artifact_dir;       ///< Base path to store compiled wheel packages (Unused)
        char *wheel_staging_dir;        ///< Base path to copy compiled wheel packages (Unused)
        char *wheel_staging_url;        ///< URL to access compiled wheel packages (Unused)
        char *build_dir;                ///< Base path to store source code and recipes
        char *build_recipes_dir;        ///< Path to store conda recipes
        char *build_sources_dir;        ///< Path to store source code
        char *build_testing_dir;        ///< Path to store test data (Unused)
        char *build_docker_dir;         ///< Path to store docker build script
    } storage;

    /*! \struct Meta
     * \brief Metadata related to the delivery
     */
    struct Meta {
        char *name;                ///< delivery name
        char *version;             ///< delivery version
        int rc;                    ///< build iteration
        char *python;              ///< version of python to use
        char *python_compact;      ///< shortened python identifier
        char *based_on;            ///< URL to previous final configuration
        char *mission;             ///< hst, jwst, roman
        char *codename;            ///< HST uses codenames
        bool final;                ///< is this a final release?
    } meta;

    /*! \struct Info
     * \brief Release information (name & datetime)
     */
    struct Info {
        char *release_name;            ///< The fully combined release string
        char *build_name;
        char *build_number;
        struct tm *time_info;          ///< Delivery time structure
        time_t time_now;               ///< Time stamp for when STASIS execution started
        char *time_str_epoch;          ///< String representation of Unix epoch
    } info;

    /*! \struct Conda
     * \brief Conda configuration
     *
     * This includes lists describing packages to be delivered
     */
    struct Conda {
        char *installer_baseurl;                ///< URL describing where Conda will be downloaded from
        char *installer_name;                   ///< Name of installer (Miniconda3, Miniforge3, etc)
        char *installer_version;                ///< Version of installer
        char *installer_platform;               ///< Platform/OS target of installer
        char *installer_arch;                   ///< CPU architecture target of installer
        char *installer_path;                   ///< Absolute path of installer on-disk
        char *tool_version;                     ///< Installed version of conda
        char *tool_build_version;               ///< Installed version of "build" package
        struct StrList *conda_packages;         ///< Conda packages to deliver
        struct StrList *conda_packages_defer;   ///< Conda recipes to be built for delivery
        struct StrList *pip_packages;           ///< Python packages to install (pip)
        struct StrList *pip_packages_defer;     ///< Python packages to be built for delivery
        struct StrList *wheels_packages;        ///< Wheel packages built for delivery
    } conda;

    /*! \struct Runtime
     *  \brief Global runtime variables
     */
    struct Runtime {
        RuntimeEnv *environ;        ///< Environment variables
    } runtime;

    /*! \struct Test
     * \brief Test information
     */
    struct Test {
        char *name;                     ///< Name of package
        char *version;                  ///< Version of package
        char *repository;               ///< Git repository of package
        char *script;                   ///< Commands to execute
        char *build_recipe;             ///< Conda recipe to build (optional)
        char *repository_info_ref;      ///< Git commit hash
        char *repository_info_tag;      ///< Git tag (first parent)
        struct StrList *repository_remove_tags;   ///< Git tags to remove (to fix duplicate commit tags)
        struct Runtime runtime;         ///< Environment variables specific to the test context
    } tests[1000]; ///< An array of tests

    struct Deploy {
        struct JFRT_Auth jfrog_auth;

        struct JFrog {
            struct StrList *files;
            struct JFRT_Upload upload_ctx;
            char *repo;
            char *dest;
        } jfrog[1000];

        struct Docker {
            struct DockerCapabilities capabilities;
            char *image_compression;
            char *dockerfile;
            char *registry;
            char *test_script;
            struct StrList *build_args;
            struct StrList *tags;
        } docker;
    } deploy;

    struct Rule {
        struct INIFILE *_handle;
        bool enable_final;          ///< true=allow rc value replacement, false=keep rc value even if final release
        char *release_fmt;          ///< Release format string
        char *build_name_fmt;       ///< Build name format string
        char *build_number_fmt;     ///< Build number format string
        struct Content content[1000];
    } rules;
};

/**
 * Initializes a Deliver structure
 * @param ctx pointer to Delivery context
 * @param ini pointer to INIFILE describing a delivery
 * @param cfg pointer to INIFILE describing extra configuration data
 * @return `0` on success
 * @return Non-zero on error
 */
int delivery_init(struct Delivery *ctx);

/**
 * Free memory allocated by delivery_init()
 * @param ctx pointer to Delivery context
 */
void delivery_free(struct Delivery *ctx);

/**
 * Print Delivery metadata
 * @param ctx pointer to Delivery context
 */
void delivery_meta_show(struct Delivery *ctx);

/**
 * Print Delivery conda configuration
 * @param ctx pointer to Delivery context
 */
void delivery_conda_show(struct Delivery *ctx);

/**
 * Print Delivery tests
 * @param ctx pointer to Delivery context
 */
void delivery_tests_show(struct Delivery *ctx);

/**
 * Print Delivery initial runtime environment
 * @param ctx  pointner to Delivery context
 */
void delivery_runtime_show(struct Delivery *ctx);

/**
 * Build Conda recipes associated with the Delivery
 * @param ctx pointer to Delivery context
 * @return 0 on success
 * @return Non-zero on error
 */
int delivery_build_recipes(struct Delivery *ctx);

/**
 * Produce a list of wheels built for the Delivery (Unused)
 * @param ctx pointer to Delivery context
 * @return pointer to StrList
 * @return NULL on error
 */
struct StrList *delivery_build_wheels(struct Delivery *ctx);

/**
 * Copy wheel packages to artifact storage
 * @param ctx pointer to Delivery context
 * @return 0 on success
 * @return Non-zero on error
 */
int delivery_index_wheel_artifacts(struct Delivery *ctx);

/**
 * Generate a header block that is applied to delivery artifacts
 * @param ctx pointer to Delivery context
 * @return header on success
 * @return NULL on error
 */
char *delivery_get_release_header(struct Delivery *ctx);

/**
 * Finalizes a delivery artifact for distribution
 * @param ctx poitner to Delivery context
 * @param filename path to delivery artifact (Conda YAML file)
 */
void delivery_rewrite_spec(struct Delivery *ctx, char *filename, unsigned stage);

/**
 * Copy compiled wheels to artifact storage
 * @param ctx pointer to Delivery context
 * @return 0 on success
 * @return Non-zero on error
 */
int delivery_copy_wheel_artifacts(struct Delivery *ctx);

/**
 * Copy built Conda packages to artifact storage
 * @param ctx poitner to Delivery context
 * @return 0 on success
 * @return Non-zero on error
 */
int delivery_copy_conda_artifacts(struct Delivery *ctx);

/**
 * Retrieve Conda installer
 * @param installer_url URL to installation script
 */
int delivery_get_installer(struct Delivery *ctx, char *installer_url);

/**
 * Generate URL based on Delivery context
 * @param ctx pointer to Delivery context
 * @param result pointer to char
 * @return in result
 */
void delivery_get_installer_url(struct Delivery *ctx, char *result);

/**
 * Install packages based on Delivery context
 * @param ctx pointer to Delivery context
 * @param conda_install_dir path to install Conda
 * @param env_name name of Conda environment to create
 * @param type INSTALL_PKG_CONDA
 * @param type INSTALL_PKG_CONDA_DEFERRED
 * @param type INSTALL_PKG_PIP
 * @param type INSTALL_PKG_PIP_DEFERRED
 * @param manifest pointer to array of StrList (package list(s))
 */
int delivery_install_packages(struct Delivery *ctx, char *conda_install_dir, char *env_name, int type, struct StrList *manifest[]);

/**
 * Update "conda index" on Conda artifact storage
 * @param ctx pointer to Delivery context
 * @return 0 on success
 * @return Non-zero on error
 */
int delivery_index_conda_artifacts(struct Delivery *ctx);

/**
 * Execute Delivery test array
 * @param ctx pointer to Delivery context
 */
void delivery_tests_run(struct Delivery *ctx);

/**
 * Determine which packages are to be installed directly from conda or pip,
 * and which packages need to be built locally
 * @param ctx pointer to Delivery context
 * @param type DEFER_CONDA (filter conda packages)
 * @param type DEFER_PIP (filter python packages)
 */
void delivery_defer_packages(struct Delivery *ctx, int type);

/**
 * Configure and activate a Conda installation based on Delivery context
 * @param ctx pointer to Delivery context
 * @param conda_install_dir path to Conda installation
 */
void delivery_conda_enable(struct Delivery *ctx, char *conda_install_dir);

/**
 * Install Conda
 * @param install_script path to Conda installation script
 * @param conda_install_dir path to install Conda
 */
void delivery_install_conda(char *install_script, char *conda_install_dir);

/**
 * Generate a formatted release string
 *
 * Formatters:
 *
 * - `%%n` = Delivery Name
 * - `%%c` = Delivery Codename (HST mission, only)
 * - `%%m` = Mission
 * - `%%R` = Delivery Revision number (or "final")
 * - `%%r` = Delivery Revision number
 * - `%%v` = Delivery Version
 * - `%%P` = Python version (i.e. 3.9.1)
 * - `%%p` = Compact Python version (i.e. 3.9.1 -> 39)
 * - `%%a` = System architecture name
 * - `%%o` = System platform name
 * - `%%t` = Delivery timestamp (Unix Epoch)
 *
 * @param ctx pointer to Delivery context
 * @param dest NULL pointer to string, or initialized string
 * @param fmt release format string
 * @return 0 on success, -1 on error
 */
int delivery_format_str(struct Delivery *ctx, char **dest, const char *fmt);

// helper function
void delivery_gather_tool_versions(struct Delivery *ctx);

// helper function
int delivery_init_tmpdir(struct Delivery *ctx);

int delivery_init_artifactory(struct Delivery *ctx);

int delivery_artifact_upload(struct Delivery *ctx);

int delivery_mission_render_files(struct Delivery *ctx);

int delivery_docker(struct Delivery *ctx);

int delivery_fixup_test_results(struct Delivery *ctx);

int *bootstrap_build_info(struct Delivery *ctx);

int delivery_dump_metadata(struct Delivery *ctx);

/**
 * Determine whether a release on-disk matches the release name in use
 * @param ctx Delivery context
 * @return 0=no, 1=yes
 */
int delivery_exists(struct Delivery *ctx);

#endif //STASIS_DELIVERY_H
