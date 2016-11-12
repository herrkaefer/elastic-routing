/*  =========================================================================
    stringext - extension of string type

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>

    This file is part of the Elastic Routing Project.
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#ifndef __STRING_EXT_H_INCLUDED__
#define __STRING_EXT_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif


#define streq(s1,s2)    (!strcmp ((s1), (s2)))
#define strneq(s1,s2)   (strcmp ((s1), (s2)))

#define UUID_STR_LEN 32


// String hash function
size_t string_hash (const char *string);

// Compare two strings
int string_compare (const char *str1, const char *str2);

// If two strings are equal
bool string_equal (const char *str1, const char *str2);

// Free a string
void string_free (char **string_p);

// Duplicate a string
char *string_duplicate (const char *string);

// Print a string
void string_print (const char *string);

// Generate a random string consisting of alpha-numeric characters with
// specific length range.
// Provide your rng or set it to NULL to use a inner one.
char *string_random_alphanum (size_t min_length, size_t max_length, rng_t *rng);

// String Levenshtein distance.
// Modified from:
// https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Levenshtein_distance#C
size_t string_levenshtein_distance (const char *str1, const char *str2);

// String cut and splice crossover
// str1:   +++++++|++++++++++++
// str2:   **********|*************
// ======>
// child1: +++++++|*************
// child2: **********|++++++++++++
listx_t *string_cut_and_splice (const char *str1, const char *str2, rng_t *rng);


#ifdef __cplusplus
}
#endif

#endif
