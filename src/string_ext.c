/*  =========================================================================
    stringext - implementation

    Copyright (c) 2016, Yang LIU <gloolar@gmail.com>
    =========================================================================
*/

#include "classes.h"


// djb2 string hash function
size_t string_hash (const char *string) {
    size_t hash = 5381;
    unsigned char *p = (unsigned char *) string;
    int c;

    while ((c = *p++) != 0)
        hash = (hash << 5) + hash + c;

    return hash;
}


int string_compare (const char *str1, const char *str2) {
    return strcmp (str1, str2);
    // int result = strcmp (string1, string2);
    // if (result < 0)
    //     return -1;
    // else if (result > 0)
    //     return 1;
    // else
    //     return 0;
}


bool string_equal (const char *str1, const char *str2) {
    return strcmp (str1, str2) == 0;
}


void string_free (char **string_p) {
    assert (string_p);
    if (*string_p) {
        free (*string_p);
        *string_p = NULL;
    }
}


char *string_duplicate (const char *string) {
    assert (string);
    char *str = (char *) malloc (sizeof (char) * (strlen (string) + 1));
    assert (str);
    strcpy (str, string);
    return str;
}


void string_print (const char *string) {
    assert (string);
    printf ("%s\n", string);
}


char *string_random_alphanum (size_t min_length, size_t max_length, rng_t *rng) {
    assert (min_length <= max_length);

    bool own_rng = false;
    if (rng == NULL) {
        rng = rng_new ();
        own_rng = true;
    }

    // static const
    char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    size_t len = (size_t) rng_random_int (rng, min_length, max_length + 1);
    char *str = (char *) malloc (sizeof (char) * (len+1));
    assert (str);
    size_t len_alphanum = strlen (alphanum);
    for (size_t index = 0; index < len; index++)
        str[index] = alphanum[rng_random_int (rng, 0, len_alphanum)];
    str[len] = '\0';

    if (own_rng)
        rng_free (&rng);
    return str;
}


size_t string_levenshtein_distance (const char *str1, const char *str2) {
    assert (str1 && str2);
    size_t x, y;
    size_t s1len = strlen (str1);
    size_t s2len = strlen (str2);
    size_t *matrix = (size_t *) malloc (sizeof (size_t) * (s2len+1) * (s1len+1));
    assert (matrix);

    matrix[0] = 0;
    for (x = 1; x <= s2len; x++)
        matrix[x*(s1len+1)] = matrix[(x-1)*(s1len+1)] + 1;
    for (y = 1; y <= s1len; y++)
        matrix[y] = matrix[y-1] + 1;
    for (x = 1; x <= s2len; x++)
        for (y = 1; y <= s1len; y++)
            matrix[x*(s1len+1)+y] =
                min3 (matrix[(x-1)*(s1len+1)+y] + 1,
                      matrix[x*(s1len+1)+(y-1)] + 1,
                      matrix[(x-1)*(s1len+1)+(y-1)] +
                        (str1[y-1] == str2[x-1] ? 0 : 1));

    size_t distance = matrix[s2len*(s1len+1) + s1len];
    free (matrix);
    return distance;
}


listx_t *string_cut_and_splice (const char *str1,
                                 const char *str2,
                                 rng_t *rng) {
    assert (str1 && str2);
    size_t len1 = strlen (str1);
    size_t len2 = strlen (str2);

    bool own_rng = false;
    if (rng == NULL) {
        rng = rng_new ();
        own_rng = true;
    }

    // cut before the following indexes
    size_t cut1 = (size_t) rng_random_int (rng, 0, len1+1);
    size_t cut2 = (size_t) rng_random_int (rng, 0, len2+1);

    size_t len1c = cut1 + len2 - cut2;
    size_t len2c = cut2 + len1 - cut1;
    char *child1 = malloc (sizeof (char) * (len1c + 1));
    assert (child1);
    char *child2 = malloc (sizeof (char) * (len2c + 1));
    assert (child2);

    strncpy (child1, str1, cut1);
    strncpy (child1 + cut1, str2 + cut2, len2 - cut2);
    child1 [len1c] = '\0';
    strncpy (child2, str2, cut2);
    strncpy (child2 + cut2, str1 + cut1, len1 - cut1);
    child2 [len2c] = '\0';

    listx_t *list = listx_new ();
    listx_append (list, child1);
    listx_append (list, child2);

    if (own_rng)
        rng_free (&rng);

    return list;
}
