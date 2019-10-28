//
//  src/path.c
//  tbd
//
//  Created by inoahdev on 11/19/18.
//  Copyright © 2018 - 2019 inoahdev. All rights reserved.
//

#include <errno.h>
#include <stdbool.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <string.h>
#include <unistd.h>

#include "copy.h"
#include "likely.h"
#include "path.h"
#include "util.h"

static const char *current_directory = NULL;
static size_t current_directory_length = 0;

char *
path_get_absolute_path(const char *__notnull const path,
                       uint64_t path_length,
                       uint64_t *const length_out)
{
    if (ch_is_slash(path[0])) {
        return (char *)path;
    }

    if (current_directory == NULL) {
        current_directory = getcwd(NULL, 0);
        if (current_directory == NULL) {
            fprintf(stderr,
                    "Failed to retrieve current-directory, error: %s\n",
                    strerror(errno));

            exit(1);
        }

        current_directory_length = strlen(current_directory);
    }

    if (path_length == 0) {
        path_length = strlen(path);
    }

    char *const combined =
        path_append_component(current_directory,
                              current_directory_length,
                              path,
                              path_length,
                              length_out);

    return combined;
}

char *
path_append_component(const char *__notnull const path,
                      const uint64_t path_length,
                      const char *__notnull component,
                      const uint64_t component_length,
                      uint64_t *const length_out)
{
    /*
     * We prefer adding a back-slash ourselves.
     */

    const uint64_t path_copy_length = remove_end_slashes(path, path_length);

    /*
     * We prefer either writing the back-slash ourselves, or having the end of
     * the original path be a slash, so we should remove any slash in the front
     * of component.
     */

    const char *comp_iter = component;
    uint64_t comp_copy_length = component_length;

    if (ch_is_slash(component[0])) {
        /*
         * We prefer the componet to not have a front-slash, with instead the
         * path having a back-slash, or we providing the slash ourselves.
         */

        comp_iter = get_end_of_slashes(comp_iter);
        if (unlikely(comp_iter == NULL)) {
            return alloc_and_copy(path, path_copy_length);
        }

        /*
         * Update the copy-length by subtracting the "drift" the component
         * experienced.
         */

        const uint64_t drift = (uint64_t)(comp_iter - component);
        comp_copy_length -= drift;

        /*
         * Remove any back-slashes if we have any.
         */

        comp_copy_length = remove_end_slashes(comp_iter, comp_copy_length);
    }

    if (unlikely(path_copy_length == 0)) {
        return alloc_and_copy(comp_iter, comp_copy_length);
    }

    /*
     * Add one for the slash-separator between path and component.
     */

    uint64_t combined_length = path_copy_length + comp_copy_length + 1;

    /*
     * Add one to the length for the null-terminator.
     */

    char *const combined = malloc(combined_length + 1);
    if (unlikely(combined == NULL)) {
        return NULL;
    }

    char *combined_component_iter = combined + path_copy_length;

    /*
     * Write the slash-separator between the path and the component.
     */

    *combined_component_iter = '/';

    memcpy(combined, path, path_copy_length);
    memcpy(combined_component_iter + 1, comp_iter, comp_copy_length);

    if (length_out != NULL) {
        *length_out = combined_length;
    }

    combined[combined_length] = '\0';
    return combined;
}

static const char *go_to_end_of_dots(const char *__notnull const dots) {
    const char *iter = dots;
    for (char ch = *iter; ch != '\0'; ch = *(++iter)) {
        if (ch != '.') {
            return iter;
        }
    }

    return NULL;
}

char *
path_append_comp_and_ext(const char *__notnull const path,
                         const uint64_t path_length,
                         const char *__notnull const comp,
                         const uint64_t comp_length,
                         const char *__notnull const ext,
                         const uint64_t ext_length,
                         uint64_t *const len_out)
{
    /*
     * We prefer adding a back-slash ourselves.
     */

    const uint64_t path_copy_length = remove_end_slashes(path, path_length);

    /*
     * We prefer either writing the back-slash ourselves, or having the end of
     * the original path be a slash, so we should remove any slash in the front
     * of component.
     */

    const char *comp_iter = comp;
    uint64_t comp_copy_len = comp_length;

    if (ch_is_slash(comp[0])) {
        /*
         * We prefer the componet to not have a front-slash, with instead the
         * path having a back-slash, or we providing the slash ourselves.
         */

        comp_iter = get_end_of_slashes(comp_iter);
        if (unlikely(comp_iter == NULL)) {
            return alloc_and_copy(path, path_copy_length);
        }

        /*
         * Update our copy-length by subtracting the "drift" the string
         * experienced.
         */

        const uint64_t drift = (uint64_t)(comp_iter - comp);
        comp_copy_len -= drift;

        /*
         * Remove any existing slashes at the back of our component.
         */

        comp_copy_len = remove_end_slashes(comp_iter, comp_copy_len);
    }

    if (unlikely(path_copy_length == 0)) {
        return alloc_and_copy(comp_iter, comp_copy_len);
    }

    /*
     * An extension may be provided without having a row of dots in front, which
     * needs to be accounted for.
     */

    const char *ext_copy_iter = ext;
    uint64_t ext_copy_length = 0;

    ext_copy_iter = go_to_end_of_dots(ext);
    if (ext_copy_iter != NULL) {
        const uint64_t drift = (uint64_t)(ext_copy_iter - ext);
        ext_copy_length = ext_length - drift;
    }

    /*
     * Add one for the back-slash on the path.
     */

    uint64_t combined_length = path_copy_length + comp_copy_len + 1;

    /*
     * Add one for the extension-dot.
     */

    if (ext_copy_length != 0) {
        combined_length += ext_copy_length + 1;
    }

    /*
     * Add one for the null-terminator.
     */

    char *const combined = malloc(combined_length + 1);
    if (unlikely(combined == NULL)) {
        return NULL;
    }

    char *combined_component_iter = combined + path_copy_length;

    /*
     * Write the slash-separator between the path and the component.
     */

    *combined_component_iter = '/';
    combined_component_iter += 1;

    memcpy(combined, path, path_copy_length);
    memcpy(combined_component_iter, comp_iter, comp_copy_len);

    if (ext_copy_length != 0) {
        char *const combined_extension_iter =
            combined_component_iter + comp_copy_len;

        /*
         * Add one for the extension-dot.
         */

        memcpy(combined_extension_iter + 1,
               ext_copy_iter,
               ext_copy_length);

        *combined_extension_iter = '.';
    }

    if (len_out != NULL) {
        *len_out = combined_length;
    }

    combined[combined_length] = '\0';
    return combined;
}

char *
path_append_two_comp_and_ext(const char *__notnull const path,
                             const uint64_t path_len,
                             const char *__notnull const first_comp,
                             const uint64_t first_comp_len,
                             const char *__notnull const second_comp,
                             const uint64_t second_comp_len,
                             const char *__notnull const ext,
                             const uint64_t ext_length,
                             uint64_t *const length_out)
{
    /*
     * We prefer adding a back-slash ourselves.
     */

    const uint64_t path_copy_length = remove_end_slashes(path, path_len);

    /*
     * We prefer either writing the back-slash ourselves, or having the end of
     * the original path be a slash, so we should remove any slash in the front
     * of component.
     */

    const char *first_comp_iter = first_comp;
    uint64_t first_comp_copy_len = first_comp_len;

    if (ch_is_slash(first_comp[0])) {
        /*
         * We add a front-slash to the path-component, so we remove any existing
         * front-slashes here.
         */

        first_comp_copy_len =
            remove_end_slashes(first_comp_iter, first_comp_copy_len);
    }

    const char *second_comp_iter = second_comp;
    uint64_t second_comp_copy_len = second_comp_len;

    if (ch_is_slash(second_comp[0])) {
        second_comp_copy_len =
            remove_end_slashes(second_comp_iter, second_comp_copy_len);
    }

    if (unlikely(path_copy_length == 0)) {
        char *result = NULL;
        if (first_comp_copy_len == 0) {
            if (second_comp_copy_len == 0) {
                return NULL;
            }
            result =
                alloc_and_copy(second_comp_iter, second_comp_copy_len);

            *length_out = second_comp_copy_len;
        } else if (second_comp_copy_len == 0) {
            result =
                alloc_and_copy(first_comp_iter, first_comp_copy_len);

            *length_out = first_comp_copy_len;
        }

        return result;
    }

    if (unlikely(first_comp_copy_len == 0)) {
        if (second_comp_copy_len == 0) {
            return alloc_and_copy(path, path_copy_length);
        }

        char *const result =
            path_append_component(path,
                                  path_copy_length,
                                  second_comp_iter,
                                  second_comp_copy_len,
                                  length_out);

        return result;
    }

    if (unlikely(second_comp_copy_len == 0)) {
        char *const result =
            path_append_component(path,
                                  path_copy_length,
                                  first_comp_iter,
                                  first_comp_copy_len,
                                  length_out);

        return result;
    }

    /*
     * An extension may be provided without having a row of dots in front, which
     * needs to be accounted for.
     */

    const char *extension_copy_iter = ext;
    uint64_t extension_copy_length = 0;

    /*
     * We prefer to add the dot ourselves.
     */

    extension_copy_iter = go_to_end_of_dots(ext);
    if (extension_copy_iter != NULL) {
        const uint64_t drift = (uint64_t)(extension_copy_iter - ext);
        extension_copy_length = ext_length - drift;
    }

    /*
     * Add one for the back-slash on the path, and one for the back-slash on the
     * first-component.
     */

    uint64_t combined_length =
        path_copy_length +
        first_comp_copy_len +
        second_comp_copy_len +
        2;

    /*
     * Add one for the path-extension dot, which we prefer to add ourselves.
     */

    if (extension_copy_length != 0) {
        combined_length += (extension_copy_length + 1);
    }

    /*
     * Add one to the length for the null-terminator.
     */

    char *const combined = malloc(combined_length + 1);
    if (unlikely(combined == NULL)) {
        return NULL;
    }

    char *first_combi_comp_iter = combined + path_copy_length;
    char *second_combi_comp_iter =
        first_combi_comp_iter + first_comp_copy_len + 1;

    /*
     * Write the slash-separator between the path and the component.
     */

    *first_combi_comp_iter = '/';
    first_combi_comp_iter += 1;

    *second_combi_comp_iter = '/';
    second_combi_comp_iter += 1;

    memcpy(combined, path, path_copy_length);
    memcpy(first_combi_comp_iter,
           first_comp_iter,
           first_comp_copy_len);

    memcpy(second_combi_comp_iter,
           second_comp_iter,
           second_comp_copy_len);

    if (extension_copy_length != 0) {
        char *const combi_ext_iter =
            second_combi_comp_iter + second_comp_copy_len;

        /*
         * Add one for the extension-dot.
         */

        memcpy(combi_ext_iter + 1,
               extension_copy_iter,
               extension_copy_length);

        *combi_ext_iter = '.';
    }

    if (length_out != NULL) {
        *length_out = combined_length;
    }

    combined[combined_length] = '\0';
    return combined;
}

static bool
component_is_a_directory(const char *__notnull const component_end) {
    /*
     * We have a directory if component_end doesn't point to a row of slashes
     * leading to a null-terminator.
     */

    const char *const end = get_end_of_slashes(component_end);
    return (end != NULL);
}

static const char *get_next_slash_or_end(const char *__notnull const path) {
    const char *iter = path;
    for (char ch = *(++iter); ch != '\0'; ch = *(++iter)) {
        if (ch_is_slash(ch)) {
            break;
        }
    }

    return iter;
}

bool
path_has_dir_component(const char *__notnull const path,
                       const uint64_t path_length,
                       const char *__notnull const component,
                       const uint64_t component_length,
                       const char **const dir_component_out)
{
    const char *iter = path;
    if (ch_is_slash(*path)) {
        /*
         * If path is simply a row of slashes, we have no match unless component
         * is also a row of slashes.
         */

        iter = get_end_of_slashes(path);
        if (iter == NULL) {
            if (ch_is_slash(*component)) {
                const char *const end = get_end_of_slashes(path);
                if (end == NULL) {
                    return true;
                }
            }

            return false;
        }
    }

    /*
     * Add one for the slash following the component, which needs to exist
     * as we're checking for a directory.
     */

    const uint64_t full_component_length = component_length + 1;
    const char *const path_end = path + path_length;

    do {
        const uint64_t max_iter_length = (uint64_t)(path_end - iter);
        if (max_iter_length < full_component_length) {
            return false;
        }

        if (memcmp(iter, component, component_length) != 0) {
            const char *const iter_end = get_next_slash_or_end(iter);
            if (iter_end == NULL) {
                return false;
            }

            iter = get_end_of_slashes(iter_end);
            if (iter == NULL) {
                return false;
            }

            continue;
        }

        /*
         * We may have matched with a path-component that has our component as a
         * prefix.
         *
         * Ex: "loc" vs "local"
         */

        const char *const iter_end = iter + component_length;
        if (!ch_is_slash(*iter_end)) {
            continue;
        }

        if (!component_is_a_directory(iter_end)) {
            return false;
        }

        if (dir_component_out != NULL) {
            *dir_component_out = iter;
        }

        return true;
    } while (true);
}

bool
path_has_filename(const char *__notnull const path,
                  const uint64_t path_length,
                  const char *__notnull const filename,
                  const uint64_t filename_length,
                  const char **const filename_out)
{
    const char *const path_back = path + (path_length - 1);
    const char *path_iter = path_back;

    if (ch_is_slash(*path_back)) {
        const char *const row_front = get_front_of_slashes(path, path_back);

        /*
         * If the first character is a slash, then the entire string is a row of
         * slashes.
         */

        if (row_front == path) {
            return false;
        }

        path_iter = row_front - 1;
    }

    /*
     * We iterate and compare from the back to save on time.
     */

    const char *filename_iter = filename + (filename_length - 1);

    char path_ch = *path_iter;
    char filename_ch = *filename_iter;

    do {
        if (path_ch != filename_ch) {
            return false;
        }

        --path_iter;
        --filename_iter;

        if (path_iter == path) {
            /*
             * We have reached the "reverse version" of the null-terminator if
             * the filename_iter is located before filename.
             */

            if (filename_iter < filename) {
                if (filename_out != NULL) {
                    *filename_out = path_iter;
                }

                return true;
            }

            return false;
        }

        path_ch = *path_iter;
        if (path_ch == '/') {
            if (filename_iter < filename) {
                if (filename_out != NULL) {
                    *filename_out = path_iter + 1;
                }

                return true;
            }

            return false;
        }

        if (filename_iter < filename) {
            return false;
        }

        filename_ch = *filename_iter;
    } while (true);

    return true;
}

uint64_t
path_remove_extension(const char *__notnull const path, const uint64_t length) {
    const char *const back = path + (length - 1);
    const char *iter = back;

    /*
     * If we have a row of slashes at the back of the path-string, remove them
     * from our parsing range.
     */

    char ch = *iter;
    if (ch_is_slash(ch)) {
        iter = get_front_of_slashes(path, iter);
        if (iter == path) {
            return length;
        }

        iter -= 1;
    }

    const char *const rev_end = path - 1;
    for (ch = *(--iter); iter != rev_end; ch = *(--iter)) {
        /*
         * If we hit a path-slash, we are about to leave the range of the last
         * path-component without a dot.
         */

        if (ch_is_slash(ch)) {
            break;
        }

        if (ch != '.') {
            continue;
        }

        /*
         * We haven't found an extension if the dot is at the front of the path.
         */

        if (iter == path) {
            break;
        }

        /*
         * We haven't found an extension if the path-component itself starts
         * with a dot.
         */

        ch = iter[-1];
        if (ch_is_slash(ch)) {
            break;
        }

        const uint64_t new_length = (uint64_t)(iter - path);
        return new_length;
    }

    return length;
}
