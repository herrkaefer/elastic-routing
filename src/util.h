/*  =========================================================================
    util - Elastic Routing: Utilities

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#ifndef __UTIL_H_INCLUDED__
#define __UTIL_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif


// ASNI escape color code. Ref: https://www.wikiwand.com/en/ANSI_escape_code
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_WHITE   "\x1b[37m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#ifdef DEBUG
    #define print_info(...)    {fprintf (stdout, \
                                ANSI_COLOR_GREEN "INFO: " ANSI_COLOR_RESET \
                                ANSI_COLOR_CYAN "%s: " ANSI_COLOR_RESET, __func__); \
                                fprintf (stdout, ##__VA_ARGS__);}
    #define print_warning(...) {fprintf (stdout, \
                                ANSI_COLOR_MAGENTA "WARNING: " ANSI_COLOR_RESET \
                                ANSI_COLOR_CYAN "%s: " ANSI_COLOR_RESET, __func__); \
                                fprintf (stdout, ##__VA_ARGS__);}
    #define print_error(...)   {fprintf (stderr, \
                                ANSI_COLOR_RED "ERROR: " ANSI_COLOR_RESET \
                                ANSI_COLOR_CYAN "%s: " ANSI_COLOR_RESET, __func__); \
                                fprintf (stderr, ##__VA_ARGS__);}
    #define print_debug(...)   {fprintf (stderr, \
                                ANSI_COLOR_YELLOW \
                                "\n>>>>>>>> Debug @func: %s @file: %s:%d >>>>>>>>\n" \
                                ANSI_COLOR_RESET, \
                                __func__, __FILE__, __LINE__); \
                                fprintf (stderr, ##__VA_ARGS__);}
#else
    #define print_info(...)
    #define print_warning(...)
    #define print_error(...)
    #define print_debug(...)
#endif


// ---------------------------------------------------------------------------

// void safe_free (void **self_p) {
//     assert (self_p);
//     if (*self_p) {
//         free (*self_p);
//         *self_p = NULL;
//     }
// }


#ifdef __cplusplus
}
#endif

#endif
