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
static char *command_path(const char *);
static int load_commands_from_file(const char *);
int main(int argc, char **);
static int menu(const char *, char **);
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
 * Default command used to present a menu to the user.
 */
#define DEFAULT_MENU "dmenu"

/**
 * Works like _printf(3)_ but writes to stderr and implicitly adds a newline to
 * the output. This macro should not be used directly because passing a format
 * string without additional arguments may produce syntactically invalid code.
 */
#define _eprintf(fmt, ...) fprintf(stderr, fmt "\n%s", __VA_ARGS__)

/**
 * Works like _printf(3)_ but writes to stderr and implicitly adds a newline to
 * the output.
 */
#define fmterr(...) _eprintf(__VA_ARGS__, "")

/**
 * Variable format alternative to _perror(3)_; this macro accepts a _printf(3)_
 * format string and, optionally, a list of values for format substitution.
 */
#define verror(fmt, ...) _eprintf(fmt ": %s", __VA_ARGS__, strerror(errno), "")

/**
 * Values that represent the action to be taken based on the command line
 * options.
 */
typedef enum {
    REFRESH_COMMAND_LIST,
    LAUNCH_MENU,
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
 * Maximum number of pointers that "commands" can hold.
 */
static size_t max_commands = 0;

/**
 * Number of pointers in "commands" that have been assigned.
 */
static size_t command_count = 0;

/**
 * Value set by functions involved with updating "commands" when there was a
 * memory allocation failure that is used to propagate errors generated inside
 * of _nftw(3)_ to the caller.
 */
static int malloc_failed = 0;

/**
 * This function is used as the comparison function for _qsort(3)_ and sorts a
 * list of strings alphabetically ignoring case. This function is **not**
 * locale-aware.
 *
 * Return: A negative number when "a" should come before "b", a positive number
 * when "b" should be come before "a" and 0 otherwise.
 */
static int stringcomparator(const void *a, const void *b)
{
    return strcasecmp(* (char * const *) a, * (char * const *) b);
}

/**
 * Check to see if a given command is already in the command list. The search
 * is case insensitive.
 *
 * Arguments:
 * - needle: Name of the command.
 *
 * Return: 0 if the command is not the command list and a non-zero value
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
 * "malloc_failed" is set to 1.
 *
 * Arguments:
 * - command: Name of the command to add to the command list.
 *
 * Return: 0 if the addition succeeded and a non-zero value otherwise.
 */
static int add_command_to_list(const char *command)
{
    char **_commands;

    if (command_count >= max_commands) {
        max_commands += INCREMENTAL_ALLOCATION_SIZE;
        if (!(_commands = realloc(commands, sizeof(char *) * max_commands))) {
            max_commands -= INCREMENTAL_ALLOCATION_SIZE;
            perror("del: could not resize command list");
            malloc_failed = 1;
            return 1;
        }

        commands = _commands;
    }

    if (!(commands[command_count] = strdup(command))) {
        perror("del: could not update command list");
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
 * Arguments:
 * - command: Relative or absolute path of the command to be tested.
 *
 * Return: Non-zero value if the path is a file the current user can execute
 * and 0 otherwise. The value of "errno" can be used to determine exactly why
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
 * When given the name of an executable located in a PATH folder, return its
 * full path.
 *
 * This function is inherently racy.
 *
 * Arguments:
 * - command: Name of the executable to resolve.
 *
 * Return: Pointer to resolved path stored in a statically allocated buffer.
 */
static char *command_path(const char *command)
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
    } else if (!(env_value = getenv("PATH"))) {
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
 * Function designed to be used with _nftw(3)_ that parses Freedesktop entries,
 * looks for executable command and adds them to launcher list.
 *
 * Arguments:
 * - fpath: Path of the file to process.
 *
 * Return: 1 if a non-recoverable error was encountered while processing the
 * file and 0 otherwise.
 */
static int parse_desktop_entry(const char *fpath, const struct stat *sb,
  int typeflag, struct FTW *ftwbuf) {

    unused ftwbuf;
    unused sb;
    unused typeflag;

    char boolean_value_string[6];
    FILE *file;
    char *k;
    char *lowercase_basename;

    size_t bufsize = 0;
    int case_changed = 0;
    char command[4096] = "";
    const char *command_basename = NULL;
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
        } else if (sscanf(line, "Exec = %4095s", command) > 0) {
            command_basename = basename(command);
        }
    }

    fclose(file);
    free(line);

    if (command[0] == '\0' || command_list_contains(command_basename)) {
        return 0;
    } else if (!(lowercase_basename = strdup(command_basename))) {
        perror("del: could not allocate memory for command name");
        return 1;
    }

    for (k = lowercase_basename; *k; k++) {
        if (*k >= 'A' && *k <= 'Z') {
            *k += 32;
            case_changed = 1;
        }
    }

    if (command_path(lowercase_basename)) {
        printf("+ %s\n", lowercase_basename);
        add_command_to_list(lowercase_basename);
    } else if (case_changed && command_path(command_basename)) {
        printf("+ %s\n", command_basename);
        add_command_to_list(command_basename);
    }

    free(lowercase_basename);
    return malloc_failed;
}

/**
 * Load commands from specified file into memory.
 *
 * Arguments:
 * - path: Path to file containing list of commands.
 *
 * Return: 0 on success and a non-zero value otherwise. On failure, "errno" is
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

    if (!(file = fopen(path, "r"))) {
        return 1;
    }

    while ((line_length = getline(&entry, &bufsize, file)) != -1) {
        if (line_length >= 1 && entry[line_length - 1] == '\n') {
            entry[line_length - 1] = '\0';
        }
        if (command_path(entry)) {
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
    fclose(file);
    errno = saved_errno;
    return failed;
}

/**
 * Display application usage information.
 *
 * Arguments:
 * - self: Name or path of compiled executable.
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
        "  1        Fatal error encountered.\n"
        "  2        Non-fatal error encountered.\n"
        "  > 128    The dmenu subprocess was killed by a signal.\n"
        "\n"
        "Options:\n"
        "  -h       Show this text and exit.\n"
        "  -f PATH  Use specified file as the command list. When this is\n"
        "           unspecified, it defaults to ~/%s\n"
        "  -r       Search for desktop entries to refresh the command list.\n"
        "           Trailing command line parameters are interpreted as\n"
        "           folders to be searched. Folders on different devices\n"
        "           must be explicitly enumerated because the search will\n"
        "           not automatically cross filesystem boundaries; in terms\n"
        "           of find(1), the search is equivalent to the following:\n"
        "           find $ARGUMENTS -xdev -name '*.desktop'\n"
        "           When no paths are given, \"/\" is searched by default.\n"
        "\n"
        "When \"-r\" is not specified, dmenu is launched with the command\n"
        "list feed into standard input. Trailing command line arguments can\n"
        "be used to pass flags to dmenu or use a different menu altogether:\n"
        "\n"
        "  Set the background color of selected text to red:\n"
        "  $ %s -- -sb \"#ff0000\"\n"
        "\n"
        "  Use rofi in dmenu mode instead of dmenu:\n"
        "  $ %s rofi -dmenu\n"
        ,
        self,
        DEFAULT_COMMAND_LIST_BASENAME,
        self,
        self
    );
}

/**
 * Update list of runnable commands by searching for Freedesktop Desktop
 * Entries in a set of folders. The search will not cross filesystem
 * boundaries, so subdirectories on devices that differ from the parent must be
 * explicitly enumerated.
 *
 * Arguments:
 * - path: File name of the command list.
 * - dirs: List of strings that are directories to search for Freedesktop
 *   Desktop Entries. If this is `NULL`, "/" will be searched by default.
 * - n: Number of entries in "dirs". When this is 0, "/" will be searched by
 *   default.
 *
 * Return: 0 on success and a non-zero value otherwise.
 */
static int refresh_command_list(const char *path, char **dirs, const size_t n)
{
    int fdtemp;
    FILE *ftemp;
    size_t i;
    int maxopen;

    char tempname[PATH_MAX + 6];  // 6: strlen of the mkstemp suffix

    if (load_commands_from_file(path) && errno != ENOENT) {
        verror("del: could not load commands from '%s'", path);
        return 1;
    }

    // 4 is subtracted below under the assumption that stdin, stdout and stderr
    // are the only open files and that parse_desktop_entry will open 1 file.
    if (sysconf(_SC_OPEN_MAX) > INT_MAX) {
        maxopen = INT_MAX - 4;
    } else {
        // POSIX mandates that _SC_OPEN_MAX must be at
        // least 20, so this value should never be negative.
        maxopen = (int) sysconf(_SC_OPEN_MAX) - 4;
    }

    if (dirs && n) {
        for (i = 0; i < n; i++) {
            if (nftw(dirs[i], parse_desktop_entry, maxopen, FTW_MOUNT)) {
                if (!malloc_failed) {
                    verror("del: unable to walk '%s'", dirs[i]);
                }
                return 1;
            }
        }
    } else if (nftw("/", parse_desktop_entry, maxopen, FTW_MOUNT)) {
        if (!malloc_failed) {
            perror("del: unable to walk '/'");
        }
        return 1;
    }

    if (!command_count) {
        fmterr("del: no commands found");
        return 1;
    }

    strcpy(tempname, path);
    strcat(tempname, "XXXXXX");
    tempname[sizeof(tempname) - 1] = '\0';

    if ((fdtemp = mkstemp(tempname)) != -1 &&
        (ftemp = fdopen(fdtemp, "w"))) {

        qsort(commands, command_count, sizeof(char *), stringcomparator);

        for (i = 0; i < command_count; i++) {
            if ((!i || strcmp(commands[i - 1], commands[i])) &&
                fprintf(ftemp, "%s\n", commands[i]) < 0) {
                goto error;
            }
        }

        if (fflush(ftemp) || fsync(fdtemp) || fclose(ftemp)) {
            verror("del: unable to flush changes to '%s'", tempname);
        } else if (rename(tempname, path)) {
            verror("del: unable to rename '%s' to '%s'", tempname, path);
        } else {
            return 0;
        }
    }

error:
    if (fdtemp == -1) {
        verror("del: mkstemp: %s", tempname);
    } else {
        verror("del: %s", tempname);
        if (fdtemp != -1 && unlink(tempname)) {
            verror("del: could not delete temporary file '%s'", tempname);
        }
    }

    return 1;
}

/**
 * Launch a menu and execute the commands it prints to standard output. Each
 * line must contain one command with no arguments.
 *
 * Arguments:
 * - menu_list_path: File containing list of executable commands. Its contents
 *   will be redirected to the menu's standard input.
 * - argv: Command name and argument list for menu. This will become "argv" in
 *   the subprocess and must be terminated with a `NULL` pointer.
 *
 * Return: The return code depends several different factors. In order of
 * precedence, they are:
 * - 0 is returned if there were no problems during function execution.
 * - 1 is returned if there was a fatal error.
 * - 2 indicates a non-fatal error arose during function execution.
 * - If there were no other problems and menu exited with a non-zero status,
 *   this function returns its exit status.
 * - If menu was killed by a signal, `signal_number + 128` is returned.
 */
static int menu(const char *menu_list_path, char **argv)
{
    FILE *menu_output;
    pid_t menu_pid;
    ssize_t line_length;
    int pipefds[2];
    int signum;
    int wait_status;

    size_t bufsize = 0;
    char *command = NULL;
    int menu_kill_signal = 0;
    int failure = 1;

    if (pipe(pipefds)) {
        perror("del: could not create pipes for subprocess communication");
        return 1;
    }

    switch ((menu_pid = fork())) {
      case -1:
        verror("del: could not fork to launch %s", argv[0]);
        return 1;

      case 0:  // Child: execute menu
        if (close(pipefds[0])) {
            perror("del: could not close unused read-end of pipe");
        } else if ((failure = dup2(pipefds[1], STDOUT_FILENO)) < 0) {
            perror("del: could not redirect stdout to parent process");
        } else if (close(STDIN_FILENO) && errno != EBADF) {
            perror("del: could not close stdin");
        } else if (open(menu_list_path, O_RDONLY) < 0) {
            verror("del: open: %s", menu_list_path);
        } else {
            execvp(argv[0], argv);
            verror("del: %s", argv[0]);
        }
        _exit(1);
    }

    if (close(pipefds[1])) {
        perror("del: could not close unused write-end of pipe");
    } else if (!(menu_output = fdopen(pipefds[0], "r"))) {
        verror("del: could not execute fdopen on %s pipe", argv[0]);
    } else {
        failure = 0;
        while (!failure &&
          (line_length = getline(&command, &bufsize, menu_output)) >= 0) {
            if (line_length >= 1 && command[line_length - 1] == '\n') {
                command[line_length - 1] = '\0';

                switch (fork()) {
                  case 0:  // Child: execute printed command
                    if (fclose(menu_output)) {
                        perror("del: could not close inherited file");
                    } else if (execlp(command, command, NULL)) {
                        verror("del: %s", command);
                    }
                    _exit(1);

                  case -1:
                    perror("del: could not fork to execute command");
                    failure = 1;
                }
            } else {
                fmterr("del: missing newline after '%s'", command);
            }
        }

        if (!failure && (failure = !feof(menu_output))) {
            verror("del: could not read %s output", argv[0]);
        }

        if (fclose(menu_output)) {
            verror("del: unable to close %s output file", argv[0]);
        }

        free(command);
    }

    if (failure == 1) {
        kill(menu_pid, (menu_kill_signal = SIGHUP));
    }

    while (1) {
        if (waitpid(menu_pid, &wait_status, 0) == -1) {
            verror("del: error waiting on %s", argv[0]);
            if (!failure) {
                failure = 2;
            }
            break;
        } else if (WIFEXITED(wait_status)) {
            if (!failure && (failure = WEXITSTATUS(wait_status))) {
                fmterr("del: %s died with exit status %d", argv[0], failure);
            }
            break;
        } else if (WIFSIGNALED(wait_status)) {
            if ((signum = WTERMSIG(wait_status)) != menu_kill_signal) {
                fmterr("del: %s received signal %d", argv[0], signum);
                failure = failure ? failure : 128 + signum;
            }
            break;
        }
    }

    return failure;
}

int main(int argc, char **argv)
{
    int exit_status;
    const char *home;
    size_t i;
    char *menu_argv0;
    int option;
    size_t strlen_home;

    action_et action = LAUNCH_MENU;
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
            fmterr("del: invalid option -- '%c'", option);
          default:
            return 1;
        }
    }

    if (!command_list_path) {
        if (!(home = getenv("HOME"))) {
            fmterr("del: HOME is unset; use \"-f\" to specify list path");
            return 1;
        }

        must_free_command_list_path = 1;
        strlen_home = strlen(home);

        // The +1 is to ensure there's room to add a '/' if needed.
        if (!(command_list_path = malloc(
           sizeof(DEFAULT_COMMAND_LIST_BASENAME) + strlen_home + 1))) {
            perror("del: could not allocate memory for filename");
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

      case LAUNCH_MENU:
        menu_argv0 = *(argv + optind);
        if (optind <= argc && (!menu_argv0 || menu_argv0[0] == '-')) {
            *(argv + (--optind)) = DEFAULT_MENU;
        }
        exit_status = menu(command_list_path, argv + optind);
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
