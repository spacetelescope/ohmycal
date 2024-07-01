//! @file template.h
#ifndef STASIS_TEMPLATE_H
#define STASIS_TEMPLATE_H

#include "core.h"

/**
 * Map a text value to a pointer in memory
 *
 * @param key in-text variable name
 * @param ptr pointer to string
 */
void tpl_register(char *key, char **ptr);

/**
 * Free the template engine
 */
void tpl_free();

/**
 * Retrieve the value of a key mapped by the template engine
 * @param key string registered by `tpl_register`
 * @return a pointer to value, or NULL if the key is not present
 */
char *tpl_getval(char *key);

/**
 * Replaces occurrences of all registered key value pairs in `str`
 * @param str the text data to render
 * @return a rendered copy of `str`, or NULL.
 * The caller is responsible for free()ing memory allocated by this function
 */
char *tpl_render(char *str);

/**
 * Write tpl_render() output to a file
 * @param str the text to render
 * @param filename the output file name
 * @return 0 on success, <0 on error
 */
int tpl_render_to_file(char *str, const char *filename);

typedef int tplfunc(void *frame, void *data_out);

struct tplfunc_frame {
    char *key;                 ///< Name of the function
    tplfunc *func;             ///< Pointer to the function
    void *data_in;             ///< Pointer to internal data (can be NULL)
    int argc;                  ///< Maximum number of arguments to accept
    union {
        char **t_char_refptr;  ///< &pointer
        char *t_char_ptr;      ///< pointer
        void *t_void_ptr;      ///< pointer to void
        int *t_int_ptr;        ///< pointer to int
        unsigned *t_uint_ptr;  ///< pointer to unsigned int
        float *t_float_ptr;    ///< pointer to float
        double *t_double_ptr;  ///< pointer to double
        char t_char;           ///< type of char
        int t_int;             ///< type of int
        unsigned t_uint;       ///< type of unsigned int
        float t_float;         ///< type of float
        double t_double;       ///< type of double
    } argv[10]; // accept up to 10 arguments
};

/**
 * Register a template function
 * @param key function name to expose to "func:" interface
 * @param tplfunc_ptr pointer to function of type tplfunc
 * @param argc number of function arguments to accept
 */
void tpl_register_func(char *key, void *tplfunc_ptr, int argc, void *data_in);

/**
 * Get the function frame associated with a template function
 * @param key function name
 * @return tplfunc_frame structure
 */
struct tplfunc_frame *tpl_getfunc(char *key);

#endif //STASIS_TEMPLATE_H
