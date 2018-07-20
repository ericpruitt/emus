#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(__APPLE__) && __APPLE__
#include <mach-o/dyld.h>
#elif defined(__FreeBSD__) && __FreeBSD__
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

/**
 * Attempt to get the path to the currently running executable.
 *
 * Returns: Path to the executable or `NULL` if it could not be determined.
 */
static char *executable_path(char *path)
{
    #if defined(__APPLE__) && __APPLE__
    char apple_executable_path[PATH_MAX];
    uint32_t bufsize = sizeof(apple_executable_path);

    if (_NSGetExecutablePath(apple_executable_path, &bufsize) != -1 &&
        realpath(apple_executable_path, path)) {

        return path;
    }
    #elif defined(__FreeBSD__) && __FreeBSD__
    size_t bufsize = PATH_MAX;
    int names[] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};

    if (sysctl(names, 4, path, &bufsize, NULL, 0) != -1) {
        return path;
    }
    #endif

    // TODO: Support OpenBSD.
    if (realpath("/proc/self/exe", path) ||
        realpath("/proc/curproc/exe", path) ||
        realpath("/proc/curproc/file", path) ||
        (getenv("_") && realpath(getenv("_"), path))) {

        return path;
    }

    return NULL;
}
