/**
 * Desktop Entry Launcher (D.E.L.)
 *
 * DEL searches for Freedesktop Desktop Entries, generates a list of graphical
 * commands and uses dmenu as a front-end so the user can select a command to
 * execute. Refer to the "usage" function for more information.
 *
 * Make: c99 -O1 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=500 -o $@ $?
 * Copyright: Eric Pruitt (https://www.codevat.com/)
 * License: BSD 2-Clause License (https://opensource.org/licenses/BSD-2-Clause)
 */
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
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
 * The suffix added to the command list path for the command exclusion list.
 */
#define EXCLUSION_LIST_SUFFIX "-exclusions"

/**
 * Default command used to present a menu to the user.
 */
#define DEFAULT_MENU_COMMAND "dmenu"

/**
 * Maximum number of files the process may open simultaneously.
 */
#define MAX_OPEN_FILES ( \
    (int) (sysconf(_SC_OPEN_MAX) > INT_MAX ? INT_MAX : sysconf(_SC_OPEN_MAX)) \
)

/**
 * Maximum permitted size of list entries.
 */
#define MAX_LIST_ENTRY_SIZE 4096

/**
 * Maximum strength length of list entries.
 */
#define MAX_LIST_ENTRY_STRLEN (MAX_LIST_ENTRY_SIZE - 1)

/**
 * _sscanf(3)_ pattern used to match Freedesktop Desktop Entry "Exec" lines.
 */
#define EXEC_LINE_PATTERN "Exec = %4095s %n"

/**
 * Suffix template used for _mkstemp(3)_ calls.
 */
#define TEMPFILE_TEMPLATE "XXXXXX"

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
 * Unsorted, in-memory list of strings. This uses a dynamically allocated array
 * that is resized in fixed chunks once it needs to beyond its original size.
 * Membership tests are done with a linear search of the array until a match is
 * found. This simple approach is taken to reduce complexity of this
 * application since it is unlikely that the number of items in the lists will
 * a bottleneck for this application.
 */
typedef struct list_st {
    size_t size;    // Maximum number of entries list may contain.
    size_t count;   // Number of entries the list contains.
    char **entries; // List of strings in list.
} list_st;

/**
 * Values that represent the action to be taken based on the command line
 * options.
 */
typedef enum {
    REFRESH_COMMAND_LIST,
    LAUNCH_MENU,
} action_et;

/**
 * List of commands to use in the menu.
 */
static list_st commands;

/**
 * List of case-insensitive _fnmatch(3)_ patterns that are used to exclude
 * commands from the menu.
 */
static list_st exclusions;

/**
  Value set by functions involved with updating lists when there was a
 * memory allocation failure that is used to propagate errors generated inside
 * of _nftw(3)_ to the caller.
 */
static int malloc_failed = 0;

/**
 * Command usage documentation.
 */
static const char command_usage[] =
"Usage: %1$s [-h] [-f PATH] [-r] [ARGUMENTS...]\n"
"\n"
"DEL searches for Freedesktop Desktop Entries, generates a list of graphical\n"
"commands and uses dmenu as a front-end so the user can select a command to\n"
"execute. The first time DEL is executed, it should be invoked as \"del -r\" to\n"
"generate the application list.\n"
"\n"
"When \"-r\" is not specified, dmenu is launched with the command list feed into\n"
"standard input. Trailing command line arguments can be used to pass flags to\n"
"dmenu or use a different menu altogether:\n"
"\n"
"    Set the background color of selected text to red:\n"
"    $ %1$s -- -sb \"#ff0000\"\n"
"\n"
"    Use rofi in dmenu mode instead of dmenu:\n"
"    $ %1$s rofi -dmenu\n"
"\n"
"Options:\n"
"  -h    Show this text and exit.\n"
"  -f PATH\n"
"        Use specified file as the command list. When this is unspecified, it\n"
"        defaults to \"$HOME/" DEFAULT_COMMAND_LIST_BASENAME "\".\n"
"  -r    Search for desktop entries to refresh the command list. Trailing\n"
"        command line parameters are interpreted as folders to be searched.\n"
"        Folders on different devices must be explicitly enumerated because the\n"
"        search will not automatically cross filesystem boundaries; in terms of\n"
"        find(1), the search is equivalent to the following command:\n"
"\n"
"            find $ARGUMENTS -xdev -name '*.desktop'\n"
"\n"
"        When no paths are given, \"/\" is searched by default. A\n"
"        newline-separated list of programs can be fed to del via stdin to\n"
"        include programs that do not have desktop entries in the generated\n"
"        launcher list. The programs must exist in $PATH or they will be\n"
"        silently ignored.\n"
"\n"
"        Commands can be excluded by specifying case-insensitive fnmatch(3)\n"
"        patterns in a file that is the path of the command list with\n"
"        \"" EXCLUSION_LIST_SUFFIX "\" appended e.g. \"$HOME/"
         DEFAULT_COMMAND_LIST_BASENAME EXCLUSION_LIST_SUFFIX "\".\n"
"\n"
"Exit Statuses:\n"
"- 1: Fatal error encountered.\n"
"- 2: Non-fatal error encountered.\n"
"- > 128: The menu subprocess was killed by signal \"N\" where \"N\" is 128\n"
"  subtracted from the exit status.\n"
;

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
 * Check to see if a given value is already in a list. The search is case
 * insensitive.
 *
 * Arguments:
 * - list: List to search.
 * - needle: Value to search for.
 *
 * Return: 0 if the command is not the command list and a non-zero value
 * otherwise.
 */
static int list_contains(const list_st *list, const char *needle)
{
    size_t i;

    for (i = 0; i < list->count; i++) {
        if (!strcasecmp(list->entries[i], needle)) {
            return 1;
        }
    }

    return 0;
}

/**
 * This function works like _fnmatch(3)_, but the matches are case insensitive,
 * and this function does not accept any flags.
 *
 * - pattern: An _fnmatch(3)_ pattern.
 * - string: String to test against pattern.
 *
 * Return: 0 if the string matches the pattern, FNM_NOMATCH if there is no
 * match or another nonzero value if there is an error.
 */
static int fncasematch(const char *pattern, const char *string)
{
    size_t i;

    char string_lower[MAX_LIST_ENTRY_SIZE] = "";
    char pattern_lower[MAX_LIST_ENTRY_SIZE] = "";

    for (i = 0; pattern[i] != '\0'; i++) {
        pattern_lower[i] = pattern[i] | 32;
    }

    for (i = 0; string[i] != '\0'; i++) {
        string_lower[i] = string[i] | 32;
    }

    return fnmatch(pattern_lower, string_lower, 0);
}

/**
 * Determine whether a command should be excluded from the menu.
 *
 * Arguments:
 * - command
 *
 * Return: 0 if the command should be excluded and a non-zero value otherwise.
 */
static int excluded(const char *command)
{
    size_t i;

    for (i = 0; i < exclusions.count; i++) {
        if (fncasematch(exclusions.entries[i], command) == 0) {
            return 1;
        }
    }

    return 0;
}

/**
 * Add given a given string to a list. This function does not check for
 * duplicates. If memory allocation fails, the global variable "malloc_failed"
 * is set to 1.
 *
 * Arguments:
 * - list: List to update.
 * - value: String to add to the list.
 *
 * Return: 0 if the addition succeeded and a non-zero value otherwise.
 */
static int add_to_list(list_st *list, const char *value)
{
    char **buffer;

    if (strlen(value) > MAX_LIST_ENTRY_STRLEN) {
        fmterr(
            "del: %s: length exceeds %iB limit", value, MAX_LIST_ENTRY_STRLEN
        );
        errno = EOVERFLOW;
        return 1;
    }

    if (list->count >= list->size) {
        list->size += INCREMENTAL_ALLOCATION_SIZE;

        if (!(buffer = realloc(list->entries, sizeof(char *) * list->size))) {
            list->size -= INCREMENTAL_ALLOCATION_SIZE;
            perror("del: could not resize list");
            malloc_failed = 1;
            return 1;
        }

        list->entries = buffer;
    }

    if (!(list->entries[list->count] = strdup(value))) {
        perror("del: could not update list");
        malloc_failed = 1;
        return 1;
    }

    list->count++;
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
static const char *command_path(const char *command)
{
    char *dest;
    const char *env_value;
    static char path[PATH_MAX];
    int saved_errno;
    size_t sizeof_command;
    const char *src;
    const char *src_mark;

    // This behavior is defined by POSIX 2.9.1 -- "Command Search and
    // Execution," item 2.
    if (strchr(command, '/')) {
        return (can_execute(command) ? command : NULL);
    } else if (!(env_value = getenv("PATH"))) {
        return NULL;
    }

    dest = path;
    src = env_value;
    src_mark = src;
    sizeof_command = strlen(command) + 1;

    while (1) {
        if (*src != ':' && *src != '\0') {
            *dest++ = *src++;

            if (((size_t) (dest - path) + sizeof_command) > sizeof(path)) {
                errno = ENAMETOOLONG;
                goto error;
            }

            continue;
        }

        if ((src - src_mark) == 0) {
            // Per POSIX 8.3, zero-length prefixes represent the current
            // working directory.
            *dest++ = '.';
            *dest++ = '/';
        } else if (*(dest - 1) != '/') {
            *dest++ = '/';

            if (((size_t) (dest - path) + sizeof_command) > sizeof(path)) {
                errno = ENAMETOOLONG;
                goto error;
            }
        }

        strcpy(dest, command);

        if (can_execute(path)) {
            return path;
        } else if (*src == '\0') {
            return NULL;
        }

        dest = path;
        src_mark = ++src;
    }

error:
    saved_errno = errno;
    verror("del: %s: unable to resolve command to path", command);
    errno = saved_errno;
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
static int parse_desktop_entry(const char *fpath, const struct stat *_2,
  int _3, struct FTW *_4) {

    /* Unused: */ (void) _2;
    /* Unused: */ (void) _3;
    /* Unused: */ (void) _4;

    char boolean_value_string[6];
    FILE *file;
    char *k;
    int len;
    char *lowercase_basename;
    int offset;

    size_t bufsize = 0;
    int case_changed = 0;
    char command[MAX_LIST_ENTRY_SIZE] = "";
    const char *command_basename = NULL;
    int inside_desktop_entry = 0;
    char *line = NULL;
    size_t strlen_fpath;
    char type[256] = "";

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
            inside_desktop_entry = !strcasecmp("[Desktop Entry]\n", line);
        } else if (sscanf(line, "NoDisplay = %5s", boolean_value_string) > 0 ||
          sscanf(line, "Terminal = %5s", boolean_value_string) > 0) {
            if (!strcasecmp("true", boolean_value_string)) {
                command[0] = '\0';
                break;
            }
        } else if (sscanf(line, "Type = %255s", type) > 0 &&
          !strcmp(type, "KonsoleApplication")) {
            command[0] = '\0';
            break;
        } else if (sscanf(line, EXEC_LINE_PATTERN, command, &offset) > 0) {
            command_basename = basename(command);

            // If the desktop entry uses env(1), use the first word that
            // doesn't appear to be a variable assignment or an option as the
            // command name.
            if (!strcmp(command_basename, "env")) {
                while (sscanf(line + offset, "%4095s %n", command, &len) > 0) {
                    if (!strchr(command + 1, '=') && command[0] != '-') {
                        command_basename = basename(command);
                        break;
                    }

                    command[0] = '\0';
                    offset += len;
                }
            }
        }
    }

    fclose(file);
    free(line);

    if (command[0] == '\0' || list_contains(&commands, command_basename)) {
        return 0;
    } else if (!(lowercase_basename = strdup(command_basename))) {
        perror("del: could not allocate memory for command name");
        return 1;
    }

    if (!excluded(command_basename)) {
        for (k = lowercase_basename; *k; k++) {
            if (*k >= 'A' && *k <= 'Z') {
                *k += 32;
                case_changed = 1;
            }
        }

        if (command_path(lowercase_basename)) {
            printf("+ %s (%s)\n", lowercase_basename, fpath);
            add_to_list(&commands, lowercase_basename);
        } else if (case_changed && command_path(command_basename)) {
            printf("+ %s (%s)\n", command_basename, fpath);
            add_to_list(&commands, command_basename);
        }
    }

    free(lowercase_basename);
    return malloc_failed;
}

/**
 * Populate a list with the lines in a file.
 *
 * Arguments:
 * - list: List to populate.
 * - path: File from which lines are read.
 *
 * Returns: 0 if the there were no errors and a non-zero value if there were.
 */
static int load_list_from_file(list_st *list, const char *path)
{
    FILE *file;
    ssize_t line_length;

    size_t bufsize = 0;
    char *entry = NULL;
    int failed = 0;

    if ((file = fopen(path, "r"))) {
        while ((line_length = getline(&entry, &bufsize, file)) != -1) {
            if (line_length >= 1 && entry[line_length - 1] == '\n') {
                entry[line_length - 1] = '\0';
            }

            if ((failed = add_to_list(list, entry))) {
                break;
            }

            printf("* %s\n", entry);
        }
    } else {
        failed = (errno != ENOENT);
    }

    return failed;
}

/**
 * Load commands from specified file into memory. The file can be specified
 * using either a string or a preexisting `FILE *` but not both at the same
 * time.
 *
 * Arguments:
 * - path: Path to file containing list of commands. This must be `NULL` when
 *   "file" is not.
 * - file: File containing list of commands. This must be `NULL` when "path" is
 *   not. This function will **not** call _fclose(3)_ on this argument.
 *
 * Return: 0 on success and a non-zero value otherwise. On failure, "errno" is
 * set appropriately.
 */
static int load_commands_from_file(const char *path, FILE *file)
{
    ssize_t line_length;

    size_t bufsize = 0;
    char *entry = NULL;
    int failed = 0;
    int saved_errno = 0;

    if ((file && path) || !(file || path)) {
        errno = EINVAL;
        return 1;
    } else if (path && !(file = fopen(path, "r"))) {
        return 1;
    }

    while ((line_length = getline(&entry, &bufsize, file)) != -1) {
        if (line_length >= 1 && entry[line_length - 1] == '\n') {
            entry[line_length - 1] = '\0';
        }

        if (!excluded(entry) && command_path(entry)) {
            if ((failed = add_to_list(&commands, entry))) {
                saved_errno = errno;
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

    if (path) {
        fclose(file);
    }

    errno = saved_errno;
    return failed;
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
static int refresh_command_list(const char *path, char **dirs, size_t n)
{
    int fdtemp;
    FILE *ftemp;
    size_t i;
    int nftw_maxopen;
    char tempname[PATH_MAX];

    if ((strlen(path) + strlen(TEMPFILE_TEMPLATE) + 1) > sizeof(tempname)) {
        errno = ENAMETOOLONG;
        verror("del: unable to update '%s'", path);
        return 1;
    }

    puts("Loading commands from existing list...");

    if (!isatty(STDIN_FILENO) && errno != EBADF &&
      load_commands_from_file(NULL, stdin)) {
        perror("del: could not load commands from stdin");
        return 1;
    } else if (load_commands_from_file(path, NULL) && errno != ENOENT) {
        verror("del: could not load commands from '%s'", path);
        return 1;
    }

    // To determine the number of file descriptors nftw(3) may simultaneously
    // hold open, 4 is subtracted from the maximum number of files the whole
    // process may open under the assumption that parse_desktop_entry will open
    // one file and stdin, stdout & stderr will be already be open. POSIX
    // mandates that _SC_OPEN_MAX must be at least 20, so this value should
    // never be negative.
    nftw_maxopen = MAX_OPEN_FILES - 4;

    puts("Searching for desktop entries...");

    if (dirs && n) {
        for (i = 0; i < n; i++) {
            if (nftw(dirs[i], parse_desktop_entry, nftw_maxopen, FTW_MOUNT)) {
                if (!malloc_failed) {
                    verror("del: unable to walk '%s'", dirs[i]);
                }

                return 1;
            }
        }
    } else if (nftw("/", parse_desktop_entry, nftw_maxopen, FTW_MOUNT)) {
        if (!malloc_failed) {
            perror("del: unable to walk '/'");
        }

        return 1;
    }

    if (!commands.count) {
        fmterr("del: no commands found");
        return 1;
    }

    strcpy(tempname, path);
    strcat(tempname, TEMPFILE_TEMPLATE);

    if ((fdtemp = mkstemp(tempname)) == -1 || !(ftemp = fdopen(fdtemp, "w"))) {
        goto error;
    }

    qsort(commands.entries, commands.count, sizeof(char *), stringcomparator);

    for (i = 0; i < commands.count; i++) {
        if ((!i || strcmp(commands.entries[i - 1], commands.entries[i])) &&
            fprintf(ftemp, "%s\n", commands.entries[i]) < 0) {

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

error:
    if (fdtemp == -1) {
        verror("del: mkstemp: %s", tempname);
    } else {
        verror("del: %s", tempname);

        if (unlink(tempname)) {
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
      // Parent: handle fork failure.
      case -1:
        verror("del: could not fork to launch %s", argv[0]);
        return 1;

      // Child: execute menu program.
      case 0:
        if (close(pipefds[0])) {
            perror("del: could not close unused read-end of pipe");
        } else if ((failure = dup2(pipefds[1], STDOUT_FILENO)) < 0) {
            perror("del: could not redirect stdout to parent process");
        } else if (close(STDIN_FILENO) && errno != EBADF) {
            perror("del: could not close stdin");
        } else if (open(menu_list_path, O_RDONLY) < 0) {
            if (errno == ENOENT) {
                fmterr("del: %s missing; was \"del -r\" run?", menu_list_path);
            } else {
                verror("del: open: %s", menu_list_path);
            }
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
                  // Parent: handle fork failure.
                  case -1:
                    perror("del: could not fork to execute command");
                    failure = 1;
                    break;

                  // Child: execute command printed by menu program.
                  case 0:
                    if (fclose(menu_output)) {
                        perror("del: could not close inherited file");
                    } else if (execlp(command, command, NULL)) {
                        verror("del: %s", command);
                    }

                    _exit(1);
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
    char *exclusion_list_path = NULL;
    int must_free_command_list_path = 0;

    while ((option = getopt(argc, argv, "+hf:r")) != -1) {
        switch (option) {
          case 'h':
            printf(command_usage, argv[0]);
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
        command_list_path = malloc(
            sizeof(DEFAULT_COMMAND_LIST_BASENAME) + strlen_home + 1
        );

        if (!command_list_path) {
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
        exclusion_list_path = malloc(
            strlen(command_list_path) + strlen(EXCLUSION_LIST_SUFFIX) + 1
        );

        if (!exclusion_list_path) {
            perror("del: could not allocate memory for filename");
            return 1;
        }

        strcat(exclusion_list_path, command_list_path);
        strcat(exclusion_list_path, EXCLUSION_LIST_SUFFIX);

        puts("Loading exclusion patterns...");
        exit_status = load_list_from_file(&exclusions, exclusion_list_path);

        if (exit_status == 0) {
            exit_status = refresh_command_list(
                command_list_path, argv + optind, (size_t) (argc - optind)
            );
        } else {
            verror("del: %s: could not load patterns", exclusion_list_path);
        }

        break;

      case LAUNCH_MENU:
        menu_argv0 = *(argv + optind);

        if (optind <= argc && (!menu_argv0 || menu_argv0[0] == '-')) {
            *(argv + (--optind)) = DEFAULT_MENU_COMMAND;
        }

        exit_status = menu(command_list_path, argv + optind);
        break;
    }

    if (must_free_command_list_path) {
        free(command_list_path);
    }

    for (i = 0; i < commands.count; i++) {
        free(commands.entries[i]);
    }

    free(commands.entries);

    for (i = 0; i < exclusions.count; i++) {
        free(exclusions.entries[i]);
    }

    free(exclusions.entries);

    return exit_status;
}
