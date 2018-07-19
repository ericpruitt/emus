#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(__APPLE__) && __APPLE__
#include <mach-o/dyld.h>
#endif

/**
 * Attempt to get the path to the currently running executable.
 *
 * Returns: Path to the executable or `NULL` if it could not be determined.
 */
static char *executable_path(char *path, uint32_t bufsize)
{
    #if defined(__APPLE__) && __APPLE__
    char apple_executable_path[PATH_MAX];

    if (_NSGetExecutablePath(apple_executable_path, &bufsize) != -1 &&
        realpath(apple_executable_path, path)) {

        return path;
    }
    #else
    (void) &bufsize;
    #endif

    // TODO: Support OpenBSD and FreeBSD when procfs is not mounted.
    if ((getenv("_") && realpath(getenv("_"), path)) ||
        realpath("/proc/self/exe", path) ||
        realpath("/proc/curproc/exe", path) ||
        realpath("/proc/curproc/file", path)) {

        return path;
    }

    return NULL;
}
