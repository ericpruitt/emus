/**
 * This library contains reimplementations of common functions that operate on
 * wide characters using utf8proc as the source of truth. This file is designed
 * to be used in two different ways: as a drop-in replacement for musl's
 * wcwidth(3) and as an "LD_PRELOAD" library.
 *
 * To use the library with musl, replace "src/ctype/wcwidth.c" with this file
 * and add the utf8proc source code directory to the compiler's include path.
 *
 * By defining the preprocessor macro "LD_PRELOAD_SO", the resulting binary can
 * be used with "LD_PRELOAD" to make dynamically linked applications use
 * utf8proc for wcwidth(3) and wcswidth(3). The resulting binary can also be
 * executed to set "LD_PRELOAD" automatically; `utf8proc-wcwidth.so mutt` is
 * functionally equivalent to `LD_PRELOAD=".../utf8proc-width.so" mutt`.
 *
 * - Author:  Eric Pruitt; <https://www.codevat.com>
 * - License: [BSD 2-Clause](http://opensource.org/licenses/BSD-2-Clause)
 */
#ifdef LD_PRELOAD_SO
#include <ctype.h>
#include <dlfcn.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wctype.h>
#endif

#include <wchar.h>

#include "utf8proc.c"

/**
 * Check whether a character is printable. The function was adapted from
 * <https://github.com/JuliaLang/utf8proc/blob/14b5779/test/charwidth.c#L5>.
 *
 * Arguments:
 * - rune: Unicode code point.
 *
 * Returns: 0 if the character is printable according to utf8proc and a
 * non-zero value if it is not.
 */
static inline int iswprint_utf8proc(utf8proc_int32_t rune)
{
    utf8proc_category_t c;

    // These characters are printable despite being in the "Other, control"
    // category (UTF8PROC_CATEGORY_CC). For more details, see:
    // - https://github.com/jquast/wcwidth/issues/8#issuecomment-78412242
    // - http://www.unicode.org/versions/Unicode6.2.0/ch08.pdf
    if (rune == 0x0601 || rune == 0x0602 || rune == 0x0603 || rune == 0x06DD) {
        return 1;
    }

    c = utf8proc_category(rune);
    return UTF8PROC_CATEGORY_LU <= c && c <= UTF8PROC_CATEGORY_ZS;
}

/**
 * Determine columns needed for a wide character.
 *
 * Arguments:
 * - wchar: A wide character.
 *
 * Returns: The number of columns needed to represent the wide character
 * "wchar".
 */
int wcwidth(wchar_t wchar)
{
    int width;
    utf8proc_int32_t rune;

    // When built as a shared library, only use utf8proc if the locale's
    // character type is Unicode of some sort.
    #ifdef LD_PRELOAD_SO
    char *ctype = setlocale(LC_CTYPE, NULL);
    int use_utf8proc = ctype && strcasestr(ctype, "UTF");
    #else
    #define use_utf8proc (1)
    #endif

    if (use_utf8proc) {
        rune = (utf8proc_int32_t) wchar;
        width = utf8proc_charwidth(rune);

        // utf8proc_charwidth returns 0 for non-printable and control
        // characters instead of -1 as is the case with wcwidth(3).
        return (width || iswprint_utf8proc(rune)) ? width : -1;
    }

    #ifdef LD_PRELOAD_SO
    // When not using utf8proc, try to fall back to the glibc implementation of
    // wcwidth.
    typedef int (*wcwidth3_type)(wchar_t);
    static int tried_dlsym = 0;
    static wcwidth3_type next_wcwidth = NULL;

    if (!tried_dlsym) {
        next_wcwidth = (wcwidth3_type) dlsym(RTLD_NEXT, "wcwidth");
        tried_dlsym = 1;

        if (!next_wcwidth) {
            fprintf(stderr, "dlsym(RTLD_NEXT, \"wcwidth\"): %s\n", dlerror());
        }
    }

    if (next_wcwidth) {
        return next_wcwidth(wchar);
    } else if (wchar < L'\0') {
        return -1;
    } else if (wchar == L'\0') {
        return 0;
    } else {
        return iswprint((wint_t) wchar) ? (wchar <= L'\xff' ? 1 : 2) : -1;
    }
    #endif
}

#ifdef LD_PRELOAD_SO
/**
 * Determine columns needed for a wide character string.
 *
 * Arguments:
 * - runes: A wide-character string.
 * - n: Maximum number of characters to examine.
 *
 * Returns: The number of columns needed to represent the wide-character string
 * "runes" truncated to at most "n" characters.
 */
int wcswidth(const wchar_t *runes, size_t n)
{
    int total;
    int width;

    for (total = 0; n-- > 0 && *runes != L'\0'; total += width) {
        if ((width = wcwidth(*runes++)) == -1) {
            return -1;
        }
    }

    return total;
}

int main(int argc, char **argv)
{
    char exe[PATH_MAX];
    char *path;
    char path_resolved[PATH_MAX];

    char *ld_preload = NULL;
    char *paths = NULL;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s PROGRAM [ARGUMENT]...\n", argv[0]);
    } else if (!realpath("/proc/self/exe", exe)) {
        perror("realpath: /proc/self/exe");
    } else {
        paths = getenv("LD_PRELOAD");

        if (!paths || paths[0] == '\0') {
            paths = NULL;
            ld_preload = strdup(exe);

            if (!ld_preload) {
                perror("strdup");
                goto error;
            }
        } else if (!(paths = strdup(paths))) {
            perror("strdup");
            goto error;
        } else {
            // Since strtok(3) may modify the underlying string, the new
            // LD_PRELOAD value is prepared in advance even though it might not
            // be used.
            ld_preload = malloc(strlen(paths) + /* ":" */ 1 + strlen(exe) + 1);

            if (!ld_preload) {
                perror("malloc");
                goto error;
            }

            stpcpy(stpcpy(stpcpy(ld_preload, paths), ":"), exe);

            for (path = strtok(paths, ":"); path; path = strtok(NULL, ":")) {
                if (!strcmp(exe, path) || (realpath(path, path_resolved) &&
                  !strcmp(exe, path_resolved))) {
                    goto exec;
                }
            }
        }

        if (setenv("LD_PRELOAD", ld_preload, 1)) {
            perror("setenv: LD_PRELOAD");
            goto error;
        }

exec:
        execvp(argv[1], argv + 1);
        perror("execvp");
    }

error:
    free(paths);
    free(ld_preload);
    _exit(255);
}
#endif
