#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* The portable entry point is an ELF program, so file managers execute it
 * directly instead of opening a shell script in an editor or terminal. */
static void fail(const char *operation)
{
    fprintf(stderr, "aha-flow: %s: %s\n", operation, strerror(errno));
    exit(127);
}

static char *join(const char *directory, const char *suffix)
{
    char *path = NULL;
    if (asprintf(&path, "%s%s", directory, suffix) < 0)
        fail("allocate launcher path");
    return path;
}

static void bundle_env(const char *name, const char *directory, const char *suffix)
{
    char *path = join(directory, suffix);
    if (setenv(name, path, 1) != 0)
        fail(name);
    free(path);
}

int main(int argc, char **argv)
{
    (void)argc;
    char executable[PATH_MAX + 1];
    const ssize_t length = readlink("/proc/self/exe", executable, PATH_MAX);
    if (length < 0)
        fail("locate launcher");
    if (length == PATH_MAX) {
        errno = ENAMETOOLONG;
        fail("locate launcher");
    }
    executable[length] = '\0';
    char *separator = strrchr(executable, '/');
    if (!separator) {
        errno = ENOENT;
        fail("locate package directory");
    }
    *separator = '\0';

    char *libraries = join(executable, "/lib");
    const char *existing = getenv("LD_LIBRARY_PATH");
    char *search = NULL;
    if (existing && *existing) {
        if (asprintf(&search, "%s:%s", libraries, existing) < 0)
            fail("allocate library search path");
    }
    if (setenv("LD_LIBRARY_PATH", search ? search : libraries, 1) != 0)
        fail("set library search path");
    free(search);
    free(libraries);

    bundle_env("QT_PLUGIN_PATH", executable, "/plugins");
    bundle_env("QT_QPA_PLATFORM_PLUGIN_PATH", executable, "/plugins/platforms");
    bundle_env("GST_PLUGIN_SYSTEM_PATH_1_0", executable, "/lib/gstreamer-1.0");
    bundle_env("GST_PLUGIN_PATH_1_0", executable, "/lib/gstreamer-1.0");
    bundle_env("GST_PLUGIN_SCANNER_1_0", executable, "/libexec/gst-plugin-scanner");
    if (setenv("QT_MEDIA_BACKEND", "gstreamer", 1) != 0)
        fail("set multimedia backend");

    const char *cache = getenv("XDG_CACHE_HOME");
    const char *user_directory = getenv("HOME");
    if (cache && *cache)
        bundle_env("GST_REGISTRY_1_0", cache, "/aha-flow/gstreamer-registry.bin");
    else if (user_directory && *user_directory)
        bundle_env("GST_REGISTRY_1_0", user_directory, "/.cache/aha-flow/gstreamer-registry.bin");

    char *program = join(executable, "/bin/aha-flow");
    execv(program, argv);
    fail("start bundled application");
    return 127;
}
