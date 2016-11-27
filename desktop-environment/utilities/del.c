/**
 * D.E.L.: Desktop Entry Launcher
 *
 * DEL searches for Freedesktop Desktop Entries, generates a list of graphical
 * commands and uses dmenu as a front-end so the user can select a command to
 * execute. Refer to the "usage" function for more information.
 *
 * Make: c99 -o $@ $?
 * Copyright: Eric Pruitt (https://www.codevat.com/)
 * License: BSD 2-Clause License (https://opensource.org/licenses/BSD-2-Clause)
 */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 500

#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int add_command_to_list(const char *);
static int can_execute(const char *);
static int command_list_contains(const char *);
static char *command_path(const char *, const char *);
static int dmenu(const char *, const int, char **);
static int load_commands_from_file(const char *);
int main(int argc, char **);
static int parse_desktop_entry(const char *, const struct stat *, int,
                               struct FTW *);
static int refresh_command_list(const char *, char **, const size_t);
static int stringcomparator(const void *, const void *);
static void usage(const char *);

/**
 * Indicate a variable is unused to silence compiler warnings.
 */
#define unused (void)

/**
 * This value controls the number of additional members the command list will
 * have allocated when it needs to be resized.
 */
#define INCREMENTAL_ALLOCATION_SIZE 64

/**
 * Basename of default command list file which is saved under "$HOME".
 */
#define DEFAULT_COMMAND_LIST_BASENAME ".del"

/**
 * Execute an expression in a while loop until the condition becomes false or
 * `errno` is set to any value but `EINTR`.
 *
 * @param condition Expression to be used as the while loop condition.
 * @param onfail Statement(s) to be executed when the condition fails and
 * `errno` is not `EINTR`. The macro defines a `break` after this substitution,
 * so an explicit `continue` is required to re-evaluate the `condition`.
 */
#define WHILE_EINTR(condition, onfail) \
    while (condition) { \
        if (errno != EINTR) { \
            onfail; \
            break; \
        } \
    }

#define fork_failed case -1
#define fork_child case 0
#define fork_parent default

/**
 * Values that represent the action to be taken based on the command line
 * options.
 */
typedef enum {
    REFRESH_COMMAND_LIST,
    LAUNCH_DMENU,
} action_et;

/**
 * Unsorted, in-memory list of commands. This is just a dynamically allocated
 * array that is resized in fixed chunks once it grows beyond its original
 * size. Membership tests are done by iterating the array until a match is
 * found. This simple approach is taken to reduce complexity of this
 * application since it is unlikely that the number of items in the list will a
 * bottleneck for this application.
 */
static char **commands = NULL;

/**
 * Maximum number of pointers that `commands` can hold.
 */
static size_t max_commands = 0;

/**
 * Number of pointers in `commands` that have been assigned.
 */
static size_t command_count = 0;

/**
 * Value set by functions involved with updating `commands` when there was a
 * memory allocation failure that is used to propagate errors generated inside
 * of `nftw(3)` to the caller.
 */
static int malloc_failed = 0;

/**
 * This function is used as the comparison function for `qsort(3)` and sorts a
 * list of strings alphabetically ignoring case. This function is **not**
 * locale-aware.
 *
 * @return A negative number when `a` should come before `b`, a positive number
 * when `b` should be come before `a` and 0 otherwise.
 */
static int stringcomparator(const void *a, const void *b)
{
    return strcasecmp(* (char * const *) a, * (char * const *) b);
}

/**
 * Check to see if a given command is already in the command list. The search
 * is case insensitive.
 *
 * @param needle Name of the command.
 *
 * @return 0 if the command is not the command list and a non-zero value
 * otherwise.
 */
static int command_list_contains(const char *needle)
{
    size_t i;

    for (i = 0; i < command_count; i++) {
        if (!strcasecmp(commands[i], needle)) {
            return 1;
        }
    }

    return 0;
}

/**
 * Add given a given command to the command list. This function does not check
 * for duplicates. If memory allocation fails, the global variable
 * `malloc_failed` is set to 1.
 *
 * @param command Name of the command to add to the command list.
 *
 * @return 0 if the addition succeeded and a non-zero value otherwise.
 */
static int add_command_to_list(const char *command)
{
    char **_commands;

    if (command_count >= max_commands) {
        max_commands += INCREMENTAL_ALLOCATION_SIZE;
        if ((_commands = realloc(commands, sizeof(char *) * max_commands))) {
            commands = _commands;
        } else {
            max_commands -= INCREMENTAL_ALLOCATION_SIZE;
            perror("Could not resize command list");
            malloc_failed = 1;
            return 1;
        }
    }

    if (!(commands[command_count] = strdup(command))) {
        perror("Could not update command list");
        malloc_failed = 1;
        return 1;
    }

    command_count++;
    return 0;
}

/**
 * Return a value indicating if a path is an executable file.
 *
 * This function is inherently racy.
 *
 * @param command Relative or absolute path of the command to be tested.
 *
 * @return Non-zero value if the path is a file the current user can execute
 * and 0 otherwise. The value of `errno` can be used to determine exactly why
 * the file cannot be executed.
 */
static int can_execute(const char *command)
{
    struct stat status;

    if (!access(command, X_OK) && !stat(command, &status)) {
        return S_ISREG(status.st_mode);
    }

    return 0;
}

/**
 * When given the name of an executable located in a PATH folder, return a its
 * full path.
 *
 * This function is inherently racy.
 *
 * @param command Name of the executable to resolve.
 * @param path Value of PATH environment variable. When this is `NULL`, the
 * result of `getenv("PATH")` is used instead.
 *
 * @return Pointer to resolved path stored in a statically allocated buffer.
 */
static char *command_path(const char *command, const char *path)
{
    size_t command_length;
    char *dest;
    const char *env_value;
    static char full_path[PATH_MAX];
    const char *src;
    const char *src_mark;

    // This behavior is defined by POSIX 2.9.1 -- "Command Search and
    // Execution," item 2.
    if (strchr(command, '/')) {
        if (can_execute(command)) {
            return (char *) command;
        }
        return NULL;
    } else if (!(env_value = path ? path : getenv("PATH"))) {
        return NULL;
    }

    dest = full_path;
    src = env_value;
    src_mark = src;
    command_length = strlen(command);

    while (1) {
        if (*src == ':' || *src == '\0') {
            if ((src - src_mark) == 0) {
                // Per POSIX 8.3, zero-length prefixes represent the current
                // working directory.
                *dest++ = '.';
                *dest++ = '/';
            } else if (*(dest - 1) != '/') {
                *dest++ = '/';
            }

            if (((size_t) (dest - full_path) + command_length) >=
              sizeof(full_path)) {
                errno = ENAMETOOLONG;
                return NULL;
            } else {
                strcpy(dest, command);
            }

            if (can_execute(full_path)) {
                return full_path;
            } else if (*src == '\0') {
                break;
            } else {
                dest = full_path;
                src_mark = ++src;
            }
        } else {
            *dest++ = *src++;
        }
    }

    return NULL;
}

/**
 * Function designed to be used with nftw(3) that parses Freedesktop entries,
 * looks for executable command and adds them to launcher list.
 */
static int parse_desktop_entry(const char *fpath, const struct stat *sb,
  int typeflag, struct FTW *ftwbuf) {

    unused ftwbuf;
    unused sb;
    unused typeflag;

    char boolean_value_string[6];
    const char *command_basename;
    FILE *file;

    size_t bufsize = 0;
    char command[4096] = "";
    int inside_desktop_entry = 0;
    char *line = NULL;
    size_t strlen_fpath;

    static const char extension[] = ".desktop";

    // If the file name does not end with ".desktop" or cannot be opened, do no
    // further processing.
    if ((strlen_fpath = strlen(fpath)) < (sizeof(extension) - 1) ||
        strcmp(fpath + strlen_fpath - sizeof(extension) + 1, extension) ||
        !(file = fopen(fpath, "r"))) {
        return 0;
    }

    while (getline(&line, &bufsize, file) != -1) {
        if (!inside_desktop_entry) {
            if (line[0] == '[') {
                inside_desktop_entry = !strcmp("[Desktop Entry]\n", line);
            }
        } else if (sscanf(line, "NoDisplay = %5s", boolean_value_string) > 0 ||
          sscanf(line, "Terminal = %5s", boolean_value_string) > 0) {
            if (!strcmp("true", boolean_value_string)) {
                command[0] = '\0';
                break;
            }
        } else {
            sscanf(line, "Exec = %4095s", command);
        }
    }

    free(line);

    if (command[0] != '\0') {
        command_basename = basename(command);
        if (!command_list_contains(command_basename) &&
          command_path(command_basename, NULL)) {
            printf("+ %s\n", command_basename);
            add_command_to_list(command_basename);
        }
    }

    WHILE_EINTR(fclose(file), NULL);
    return malloc_failed;
}

/**
 * Load commands from specified file into memory.
 *
 * @param path Path to file containing list of commands.
 *
 * @return 0 on success and a non-zero value otherwise. On failure, `errno` is
 * set appropriately.
 */
static int load_commands_from_file(const char *path)
{
    FILE *file;
    ssize_t line_length;

    size_t bufsize = 0;
    char *entry = NULL;
    int failed = 0;
    int saved_errno = 0;

    WHILE_EINTR(!(file = fopen(path, "r")),
        return 1;
    );

    while ((line_length = getline(&entry, &bufsize, file)) != -1) {
        if (line_length >= 1 && entry[line_length - 1] == '\n') {
            entry[line_length - 1] = '\0';
        }
        if (command_path(entry, NULL)) {
            if ((failed = add_command_to_list(entry))) {
                break;
            }
        } else {
            printf("- %s\n", entry);
        }
    }

    if (!failed && !feof(file)) {
        saved_errno = errno;
        failed = 1;
    }

    free(entry);
    WHILE_EINTR(fclose(file), NULL);
    errno = saved_errno;
    return failed;
}

/**
 * Display application usage information.
 *
 * @param self Name or path of compiled executable.
 */
static void usage(const char *self)
{
    printf(
        "Usage: %s [-h] [-f PATH] [-r] [ARGUMENTS...]\n"
        "\n"
        "DEL searches for Freedesktop Desktop Entries, generates a list of "
        "graphical\ncommands and uses dmenu as a front-end so the user can "
        "select a command to\nexecute.\n"
        "\n"
        "Exit statuses:\n"
        " 1         Fatal error encountered.\n"
        " 2         Non-fatal error encountered.\n"
        " > 128     The dmenu subprocess was killed by a signal.\n"
        "\n"
        "Options:\n"
        " -h        Show this text and exit.\n"
        " -f PATH   Use specified file as the command list. When this is\n"
        "           unspecified, it defaults to ~/%s\n"
        " -r        Search for desktop entries to refresh the command list.\n"
        "           Trailing command line parameters are interpreted as\n"
        "           folders to be searched. Folders on different devices\n"
        "           must be explicitly enumerated because the search will\n"
        "           not automatically cross filesystem boundaries; in terms\n"
        "           of find(1), the search is equivalent to the following:\n"
        "           find $ARGUMENTS -xdev -name '*.desktop'\n"
        "           When no paths are given, \"/\" is searched by default.\n"
        "\n"
        "When \"-r\" is not specified, dmenu is launched with the command\n"
        "list feed into standard input. Trailing command line parameters can\n"
        "be used to pass arguments to dwm, e.g.: %s -- -sb '#ff0000'.\n"
        ,
        self,
        DEFAULT_COMMAND_LIST_BASENAME,
        self
    );
}

/**
 * Update list of runnable commands by searching for Freedesktop Desktop
 * Entries in a set of folders. The search will not cross filesystem
 * boundaries, so subdirectories on devices that differ from the parent must be
 * explicitly enumerated.
 *
 * @param path File name of the command list.
 * @param dirs List of strings that are directories to search for Freedesktop
 * Desktop Entries. If this is `NULL`, "/" will be searched by default.
 * @param n Number of entries in `dirs`. When this is 0, "/" will be searched
 * by default.
 *
 * @return 0 on success and a non-zero otherwise with `errno` set
 * appropriately.
 */
static int refresh_command_list(const char *path, char **dirs, const size_t n)
{
    int failed;
    size_t i;
    int maxopen;
    int tempfd;
    FILE *tempfile;

    char *tempfile_path = NULL;

    if (load_commands_from_file(path) && errno != ENOENT) {
        perror(path);
        return 1;
    }

    // 3 is subtracted below under the assumption that stdin, stdout and
    // stderr are the only open files.
    if (sysconf(_SC_OPEN_MAX) > INT_MAX) {
        maxopen = INT_MAX - 3;
    } else {
        // POSIX mandates that _SC_OPEN_MAX must be at
        // least 20, so this value should never be negative.
        maxopen = (int) sysconf(_SC_OPEN_MAX) - 3;
    }

    if (dirs && n) {
        for (i = 0; i < n; i++) {
            if (nftw(dirs[i], parse_desktop_entry, maxopen, FTW_MOUNT)) {
                if (!malloc_failed) {
                    perror(dirs[i]);
                }
                return 1;
            }
        }
    } else if (nftw("/", parse_desktop_entry, maxopen, FTW_MOUNT)) {
        if (!malloc_failed) {
            perror("/");
        }
        return 1;
    }

    qsort(commands, command_count, sizeof(char *), stringcomparator);

    if ((tempfile_path = malloc(strlen(path) + 7))) {
        strcpy(tempfile_path, path);
        strcat(tempfile_path, "XXXXXX");
    } else {
        perror("Could not allocate memory for temporary file name");
        return 1;
    }

    WHILE_EINTR((tempfd = mkstemp(tempfile_path)) < 0,
        goto tempfile_error;
    );
    WHILE_EINTR(!(tempfile = fdopen(tempfd, "w")),
        WHILE_EINTR(close(tempfd), NULL);
        goto tempfile_error;
    );

    for (i = 0; i < command_count; i++) {
        if ((!i || strcmp(commands[i - 1], commands[i])) &&
            fprintf(tempfile, "%s\n", commands[i]) < 0) {

            perror("Could not write to temporary file");
            WHILE_EINTR(fclose(tempfile), NULL);
            if (unlink(tempfile_path)) {
                perror(tempfile_path);
            }
            free(tempfile_path);
            return 1;
        }
    }

    WHILE_EINTR(fflush(tempfile),
        goto tempfile_error;
    );
    WHILE_EINTR(fsync(tempfd),
        goto tempfile_error;
    );
    WHILE_EINTR(fclose(tempfile),
        goto tempfile_error;
    );

    failed = rename(tempfile_path, path);
    free(tempfile_path);

    if (failed) {
        perror("Could not rename temporary file");
        return 1;
    }

    return 0;

tempfile_error:
    perror(tempfile_path);
    free(tempfile_path);
    return 1;
}

/**
 * Run dmenu and attempt to execute any commands it returns.
 *
 * @param menu_list_path File containing list of executable commands to display
 * with dmenu.
 * @param argc Number of command line arguments to pass to dmenu.
 * @param argv Array of command line arguments to pass to dmenu. This array
 * must be terminated with a `NULL` pointer.
 *
 * @return The return code depends several different factors. In order of
 * precedence, they are:
 * - 0 is returned if there were no problems during function execution.
 * - 1 is returned if there was a fatal error:
 * - 2 indicates a non-fatal error arose during function execution.
 * - If there were no other problems and dmenu exited with a non-zero status,
 *   this function returns its exit status.
 * - If dmenu was killed by a signal, the signal number + 128 is returned.
 */
static int dmenu(const char *menu_list_path, const int argc, char **argv)
{
    FILE *dmenu_output;
    pid_t dmenu_pid;
    ssize_t line_length;
    char **newargv;
    int pipefds[2];
    int signum;
    int wait_status;

    size_t bufsize = 0;
    char *command = NULL;
    const int dmenu_kill_signal = SIGHUP;
    int parent_killed_dmenu = 0;
    int status = 0;

    if (pipe(pipefds)) {
        perror("Could not create pipes for subprocess communication");
        return 1;
    }

    switch ((dmenu_pid = fork())) {
      fork_failed:
        perror("Could not fork to launch dmenu");
        return 1;

      fork_child:
        WHILE_EINTR(close(pipefds[0]),
            perror("Could not close unused read-end of pipe");
            _exit(1);
        );
        WHILE_EINTR((status = dup2(pipefds[1], STDOUT_FILENO)) < 0,
            perror("Could not redirect stdout to parent process");
            _exit(1);
        );
        WHILE_EINTR(close(STDIN_FILENO),
            perror("Could not close stdin");
            _exit(1);
        );
        WHILE_EINTR(open(menu_list_path, O_RDONLY) < 0,
            perror(menu_list_path);
            _exit(1);
        );

        // Create an array large enough to hold new value for argv[0]. Since
        // argc does not count the NULL pointer, the new array size is
        // (argc + 2) instead of (argc + 1).
        if (!(newargv = malloc((size_t) (argc + 2) * sizeof(char *)))) {
            perror("Could not allocate memory for dmenu arguments");
            _exit(1);
        }

        newargv[0] = "dmenu";
        memcpy(newargv + 1, argv, (size_t) (argc + 1) * sizeof(char *));
        if (execvp(newargv[0], newargv)) {
            perror(newargv[0]);
            return 1;
        }

      fork_parent:
        WHILE_EINTR(close(pipefds[1]),
            perror("Could not close unused write-end of pipe");
            status = 1;
            goto kill_dmenu_if_parent_failed;
        );
        WHILE_EINTR(!(dmenu_output = fdopen(pipefds[0], "r")),
            perror("Could not execute fdopen on dmenu pipe");
            status = 1;
            goto kill_dmenu_if_parent_failed;
        );

        while (status != 1 &&
          (line_length = getline(&command, &bufsize, dmenu_output)) != -1) {
            if (line_length >= 1 && command[line_length - 1] == '\n') {
                command[--line_length] = '\0';
            }

            switch (fork()) {
              fork_failed:
                perror("Could not fork to execute command");
                status = 1;
                break;

              fork_child:
                WHILE_EINTR(fclose(dmenu_output), NULL);
                if (execlp(command, command, NULL)) {
                    perror(command);
                    _exit(1);
                }

              fork_parent:
                break;
            }
        }

        if (!status && !feof(dmenu_output)) {
            perror("Could not read dmenu output");
            status = 1;
        }

        free(command);
        WHILE_EINTR(fclose(dmenu_output), NULL);

kill_dmenu_if_parent_failed:
        if (status == 1) {
            kill(dmenu_pid, dmenu_kill_signal);
            parent_killed_dmenu = 1;
        }

        while (1) {
            if (waitpid(dmenu_pid, &wait_status, 0) == -1) {
                perror("Error waiting on dmenu");
                if (!status) {
                    status = 2;
                }
                break;
            } else if (WIFEXITED(wait_status)) {
                if (!status) {
                    status = WEXITSTATUS(wait_status);
                }
                break;
            } else if (WIFSIGNALED(wait_status)) {
                signum = WTERMSIG(wait_status);
                if (!parent_killed_dmenu || signum != dmenu_kill_signal) {
                    fprintf(stderr, "dmenu: killed by signal %d\n", signum);
                    if (!status) {
                        status = 128 + signum;
                    }
                }
                break;
            }
        }

        return status;
    }
}

int main(int argc, char **argv)
{
    int exit_status;
    const char *home;
    size_t i;
    int option;
    size_t strlen_home;

    action_et action = LAUNCH_DMENU;
    char *command_list_path = NULL;
    int must_free_command_list_path = 0;

    while ((option = getopt(argc, argv, "+hf:r")) != -1) {
        switch (option) {
          case 'h':
            usage(argv[0]);
            return 0;

          case 'f':
            command_list_path = optarg;
            break;

          case 'r':
            action = REFRESH_COMMAND_LIST;
            break;

          case '+':
            // Using "+" to ensure POSIX-style argument parsing is a GNU
            // extension, so an explicit check for "+" as a flag is added for
            // other getopt(3) implementations.
            fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], option);
          default:
            return 1;
        }
    }

    if (!command_list_path) {
        if (!(home = getenv("HOME"))) {
            fputs("HOME is unset. Use \"-f\" to specify list path.", stderr);
            return 1;
        }

        must_free_command_list_path = 1;
        strlen_home = strlen(home);

        // The +1 is to ensure there's room to add a '/' if needed.
        if (!(command_list_path = malloc(
           sizeof(DEFAULT_COMMAND_LIST_BASENAME) + strlen_home + 1))) {
            perror("Could not allocate memory for launcher list filename");
            return 1;
        }

        strcpy(command_list_path, home);
        if (command_list_path[strlen_home - 1] != '/') {
            command_list_path[strlen_home] = '/';
            command_list_path[strlen_home + 1] = '\0';
            strlen_home++;
        }
        strcat(command_list_path + strlen_home, DEFAULT_COMMAND_LIST_BASENAME);
    }

    switch (action) {
      case REFRESH_COMMAND_LIST:
        exit_status = refresh_command_list(command_list_path, argv + optind,
          (size_t) (argc - optind));
        break;

      case LAUNCH_DMENU:
        exit_status = dmenu(command_list_path, argc - optind, argv + optind);
        break;
    }

    if (must_free_command_list_path) {
        free(command_list_path);
    }

    for (i = 0; i < command_count; i++) {
        free(commands[i]);
    }

    free(commands);

    return exit_status;
}
