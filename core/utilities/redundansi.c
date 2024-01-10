/**
 * RedundANSI
 *
 * When terminals render escape sequences, attributes are applied to characters
 * across line boundaries. When Less renders a file with the "-R" /
 * "--RAW-CONTROL-CHARS" option, it does not apply the same logic terminals use
 * which results in attributes not being applied to multiple lines. RedundANSI
 * generates explicit, redundant ANSI SGR escape sequences for every line in
 * the data its processing to ensure Less renders the output the way a terminal
 * would.
 *
 * Make: $(CC) -O1 -D_POSIX_C_SOURCE=200809L $(CFLAGS) -o $@ $?
 * Copyright: Eric Pruitt (https://www.codevat.com/)
 * License: BSD 2-Clause License (https://opensource.org/licenses/BSD-2-Clause)
 */
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * Get the number of members in a fixed-length array.
 *
 * Arguments:
 * - array
 *
 * Return: The number of members in the array.
 */
#define ARRAY_LENGTH(array) (sizeof(array) / sizeof((array)[0]))

/**
 * Exit status to indicate the command the child process encountered an
 * unspecified error.
 */
#define EXIT_CHILD_FAILURE 126

/**
 * Exit status to indicate the command the child process tried to execute was
 * not found.
 */
#define EXIT_EXEC_ENOENT 127

/**
 * Exit status to indicate one or more I/O errors were encountered while
 * reading input or writing output.
 */
#define EXIT_IO_ERROR 2

/**
 * Structure representing the parameters in an SGR escape sequence for a single
 * attribute.
 */
typedef struct sgr_parameters_st {
    /**
     * The number of parameters in the SGR attribute specification.
     */
    size_t count;
    /**
     * The numeric parameters of the SGR attribute specification. The number of
     * defined values is specified in the "count" member, and the maximum
     * number of parameters is 5 to accommodate the longest, valid single
     * attribute SGR sequences ("\033[38;..." and "\033[48;..." with RGB
     * colors).
     */
    int parameters[5];
} sgr_parameters_st;

/**
 * Generate an escape sequence string from an sgr_parameters_st structure.
 *
 * Arguments:
 * - sgr: An sgr_parameters_st representing an attribute specification.
 *
 * Return: An ANSI escape sequence. This value points to a static buffer that
 * will be overwritten by subsequent calls to this function.
 */
static char* sgr2str(sgr_parameters_st *sgr)
{
    static char buffer[32];
    ssize_t length;

    if (sgr->count) {
        length = sprintf(buffer, "\033[%d", sgr->parameters[0]);

        for (size_t n = 1; n < sgr->count; n++) {
            length += sprintf(buffer + length, ";%d", sgr->parameters[n]);
        }

        strcpy(buffer + length, "m");
    } else {
        buffer[0] = '\0';
    }

    return buffer;
}

/**
 * Reset the contents of an sgr_parameters_st structure.
 *
 * Arguments:
 * - parameters: An sgr_parameters_st structure.
 */
static void clear_parameters(sgr_parameters_st *sgr)
{
    sgr->count = 0;
}

/**
 * Parse an SGR escape sequence.
 *
 * Arguments:
 * - escape: An SGR escape sequence. All parameters must be explicit for this
 *   function to parse the escape sequence.
 *
 * Return: A `NULL`-terminated array of sgr_parameters_st pointers with each
 * representing a terminal attribute specification. The returned array and its
 * members use statically allocated memory that will be overwritten by
 * subsequent calls to this function.
 */
static sgr_parameters_st **parse_sgr_escape(char* escape)
{
    static sgr_parameters_st attributes[128];
    static sgr_parameters_st *results[ARRAY_LENGTH(attributes) + 1];
    int value;

    size_t attribute_count = 0;
    size_t consume = 0;
    sgr_parameters_st *last = NULL;

    do {
        // Advance to the next number.
        for (; *escape && (*escape < '0' || *escape > '9'); escape++);

        if (consume == 0) {
            attributes[attribute_count].count = 0;
            last = &attributes[attribute_count];
            results[attribute_count++] = last;
        } else if (last->count >= ARRAY_LENGTH(attributes)) {
            break;
        } else {
            consume--;
        }

        last->parameters[last->count++] = (
            value = (int) strtol(escape, &escape, 10)
        );

        if (last->count == 1 && (value == 38 || value == 48)) {
            consume++;
        } else if ((last->parameters[0] == 38 || last->parameters[0] == 48) &&
          last->count == 2) {
            if (last->parameters[1] == 2) {
                consume += 3;
            } else if (last->parameters[1] == 5) {
                consume += 1;
            }
        }
    } while (*escape && *escape != 'm');

    results[attribute_count] = NULL;
    return results;
}

/**
 * Handler invoked when SIGCHLD is caught in a manner indicating the child
 * process exited prematurely.
 */
static void handle_sigchld(int unused)
{
    int child_status;

    (void) unused;

    while (1) {
        if (waitpid(-1, &child_status, 0) == -1) {
            if (errno != EINTR) {
                _exit(EXIT_CHILD_FAILURE);
            }
        } else if (WIFEXITED(child_status)) {
            _exit(WEXITSTATUS(child_status));
        } else if (WIFSIGNALED(child_status)) {
            _exit(128 + WTERMSIG(child_status));
        }
    }
}

int main(int argc, char **argv)
{
    char byte;
    size_t bytes_read;
    int child_status;
    char escape[32];
    int first;
    char readbuf[65536];
    sgr_parameters_st *parameters;
    int pipefds[2];
    char previous_byte;
    sgr_parameters_st **seqs;
    int signum;
    char writebuf[1024];

    pid_t child = 0;
    int child_kill_signal = 0;
    FILE *destfile = stdout;
    size_t escapelen = 0;
    int exit_code = EXIT_SUCCESS;
    bool inside_sgr_escape_sequence = false;
    void (*original_sigchld_hanlder)(int) = NULL;
    bool print_escapes = false;

    // Last explicit SGR parameters encountered for each class of attributes.
    sgr_parameters_st bgcolor = {0};
    sgr_parameters_st blink = {0};
    sgr_parameters_st bold = {0};
    sgr_parameters_st faint = {0};
    sgr_parameters_st fgcolor = {0};
    sgr_parameters_st hidden = {0};
    sgr_parameters_st italic = {0};
    sgr_parameters_st reverse = {0};
    sgr_parameters_st strike = {0};
    sgr_parameters_st underline = {0};

    if (argc >= 2 && !(strcmp(argv[1], "--help") && strcmp(argv[1], "-h") &&
      strcmp(argv[1], "-V"))) {
        printf("Usage: %s [COMMAND [ARGUMENT]...]", basename(argv[0]));
        return EXIT_SUCCESS;
    }

    if (isatty(STDIN_FILENO)) {
        fputs("Awaiting input from TTY...\n", stderr);
    }

    if (argc > 1) {
        if (pipe(pipefds)) {
            perror("pipe2");
            return EXIT_FAILURE;
        }

        original_sigchld_hanlder = signal(SIGCHLD, handle_sigchld);

        switch ((child = fork())) {
          // Error
          case -1:
            perror("fork");
            return EXIT_FAILURE;

          // Child
          case 0:
            close(pipefds[1]);

            if (dup2(pipefds[0], STDIN_FILENO)) {
                perror("dup2");
                _exit(1);
            }

            argv++;
            execvp(argv[0], argv);
            perror(argv[0]);
            _exit(errno == ENOENT ? EXIT_EXEC_ENOENT : EXIT_CHILD_FAILURE);

          // Parent
          default:
            if (close(pipefds[0])) {
                perror("close");
                signal(SIGCHLD, NULL);
                kill(child, SIGHUP);
                return EXIT_FAILURE;
            }

            if (!(destfile = fdopen(pipefds[1], "w"))) {
                perror("fdopen");
                signal(SIGCHLD, NULL);
                kill(child, SIGHUP);
                return EXIT_FAILURE;
            }
        }
    }

    while ((bytes_read = fread(readbuf, 1, sizeof(readbuf), stdin))) {
        for (size_t n = 0; n < bytes_read; n++) {
            // Printing escapes after the last newline in the file can result
            // in an extra, blank line being rendered by a pager, so we only
            // print escapes after reading the next byte following a newline
            // since we know at that stage that the newline was not end of the
            // file.
            if (print_escapes) {
                strcpy(writebuf, sgr2str(&bgcolor));
                strcat(writebuf, sgr2str(&blink));
                strcat(writebuf, sgr2str(&bold));
                strcat(writebuf, sgr2str(&faint));
                strcat(writebuf, sgr2str(&fgcolor));
                strcat(writebuf, sgr2str(&hidden));
                strcat(writebuf, sgr2str(&italic));
                strcat(writebuf, sgr2str(&reverse));
                strcat(writebuf, sgr2str(&strike));
                strcat(writebuf, sgr2str(&underline));

                if (fputs(writebuf, destfile) == EOF) {
                    exit_code = EXIT_IO_ERROR;
                }

                print_escapes = false;
            }

            byte = readbuf[n];

            if (!byte) {
                // The terminal emulators I tested seem to just ignore NUL
                // bytes inside of escape sequences. In Less, they seem to
                // cause rendering issues, so we simply drop NUL bytes so the
                // rendered text is the same regardless of whether or not it is
                // fed to Less.
                continue;
            } else if (fputc(byte, destfile) == EOF) {
                perror("fputc");
                exit_code = EXIT_IO_ERROR;
            }

            if (byte == '\n') {
                inside_sgr_escape_sequence = false;
                escapelen = 0;
                print_escapes = true;
            } else if (byte == '\033') {
                inside_sgr_escape_sequence = true;
                escape[escapelen++] = byte;
            } else if (inside_sgr_escape_sequence) {
                if (escapelen >= sizeof(escape) - 1) {
                    goto invalid_sgr_sequence;
                }

                // Per the ANSI spec, empty parameters are equivalent to 0.
                previous_byte = escape[escapelen - 1];

                if ((previous_byte == '[' || previous_byte == ';') &&
                  (byte == ';' || byte == 'm')) {
                    escape[escapelen++] = '0';

                    if (escapelen >= sizeof(escape) - 1) {
                        goto invalid_sgr_sequence;
                    }
                }

                escape[escapelen++] = byte;

                if (byte == 'm' && escapelen > 2) {
                    for (seqs = parse_sgr_escape(escape); *seqs; seqs++) {
                        parameters = *seqs;
                        first = parameters->parameters[0];

                        if (first == 0) {
                            clear_parameters(&bgcolor);
                            clear_parameters(&blink);
                            clear_parameters(&bold);
                            clear_parameters(&faint);
                            clear_parameters(&fgcolor);
                            clear_parameters(&hidden);
                            clear_parameters(&italic);
                            clear_parameters(&reverse);
                            clear_parameters(&strike);
                            clear_parameters(&underline);
                        } else if (first == 1) {
                            bold = *parameters;
                        } else if (first == 2) {
                            faint = *parameters;
                        } else if (first == 3) {
                            italic = *parameters;
                        } else if (first == 4) {
                            underline = *parameters;
                        } else if (first == 5 || first == 6) {
                            blink = *parameters;
                        } else if (first == 7) {
                            reverse = *parameters;
                        } else if (first == 8) {
                            hidden = *parameters;
                        } else if (first == 9) {
                            strike = *parameters;
                        } else if (first == 22) {
                            clear_parameters(&faint);
                            clear_parameters(&italic);
                        } else if (first == 23) {
                            clear_parameters(&italic);
                        } else if (first == 24) {
                            clear_parameters(&underline);
                        } else if (first == 25) {
                            clear_parameters(&blink);
                        } else if (first == 27) {
                            clear_parameters(&reverse);
                        } else if (first == 28) {
                            clear_parameters(&hidden);
                        } else if (first == 29) {
                            clear_parameters(&strike);
                        } else if (first >= 30 && first <= 38) {
                            fgcolor = *parameters;
                        } else if (first == 39) {
                            clear_parameters(&fgcolor);
                        } else if (first >= 40 && first <= 48) {
                            bgcolor = *parameters;
                        } else if (first == 49) {
                            clear_parameters(&bgcolor);
                        }
                    }
                } else if (escapelen == 2 && byte == '[') {
                    continue;
                } else if (escapelen > 2) {
                    if (byte == ';' || (byte >= '0' && byte <= '9')) {
                        continue;
                    }
                }

invalid_sgr_sequence:
                inside_sgr_escape_sequence = false;
                escapelen = 0;
            }
        }
    }

    if (child) {
        signal(SIGCHLD, original_sigchld_hanlder);
    }

    if (ferror(stdin)) {
        exit_code = EXIT_IO_ERROR;
    }

    if (fclose(destfile) == EOF && exit_code == 0) {
        exit_code = EXIT_IO_ERROR;
    }

    if (child) {
        if (exit_code) {
            kill(child, (child_kill_signal = SIGHUP));
        }

        while (1) {
            if (waitpid(child, &child_status, 0) == -1) {
                perror("waitpid");
                return exit_code ? exit_code : EXIT_FAILURE;
            } else if (WIFEXITED(child_status)) {
                return exit_code ? exit_code : WEXITSTATUS(child_status);
            } else if (WIFSIGNALED(child_status)) {
                if ((signum = WTERMSIG(child_status)) != child_kill_signal) {
                    fprintf(stderr, "received signal %d", signum);
                    return exit_code ? exit_code : 128 + signum;
                }

                break;
            }
        }
    }

    return exit_code;
}
