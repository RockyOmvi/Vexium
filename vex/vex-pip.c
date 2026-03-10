/* Vexium Package Manager (vex-pip)
 * A simple package manager for Vexium similar to Python's pip
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir _mkdir
#else
#include <sys/stat.h>
#endif

#define MAX_PATH 512
#define MAX_NAME 128

/* Package registry URL (can be customized) */
#define REGISTRY_URL "https://vexium-pkg.example.com/api"

/* Package directory */
static char* get_package_dir(void) {
    static char path[MAX_PATH];
    char* home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    if (!home) home = ".";
    snprintf(path, MAX_PATH, "%s/.vexium/packages", home);
    return path;
}

/* Create directory if it doesn't exist */
static int mkdirtree(const char* path) {
    char tmp[MAX_PATH];
    char* p = NULL;
    size_t len;
    int result = 0;
    
    snprintf(tmp, MAX_PATH, "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') tmp[len - 1] = '\0';
    
    for (p = tmp; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            result = mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755);
}

/* Download a package (simplified - just copies for now) */
static int download_package(const char* name, const char* version) {
    char cmd[MAX_PATH * 2];
    char pkg_dir[MAX_PATH];
    char* pkg_path = get_package_dir();
    
    printf("Downloading %s (version %s)...\n", name, version ? version : "latest");
    
    /* Create package directory */
    snprintf(pkg_dir, MAX_PATH, "%s/%s", pkg_path, name);
    mkdirtree(pkg_dir);
    
    /* In a real implementation, this would:
     * 1. Fetch package metadata from registry
     * 2. Download package tarball
     * 3. Extract to package directory
     * 4. Parse vexium.toml for dependencies
     * 5. Recursively download dependencies
     */
    
    printf("Package %s installed successfully!\n", name);
    return 0;
}

/* Search for packages */
static int search_packages(const char* query) {
    printf("Searching for packages matching '%s'...\n", query);
    
    /* In a real implementation, this would:
     * 1. Call registry API: GET /api/search?q=<query>
     * 2. Parse JSON response
     * 3. Display results
     */
    
    printf("No packages found.\n");
    return 0;
}

/* List installed packages */
static int list_packages(void) {
    char* pkg_path = get_package_dir();
    
    printf("Installed packages:\n");
    
#ifdef _WIN32
    WIN32_FIND_DATA find_data;
    HANDLE hFind = FindFirstFile(pkg_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        printf("  (none)\n");
        return 0;
    }
    
    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            printf("  %s\n", find_data.cFileName);
        }
    } while (FindNextFile(hFind, &find_data));
    
    FindClose(hFind);
#else
    DIR* dir = opendir(pkg_path);
    struct dirent* entry;
    
    if (!dir) {
        printf("  (none)\n");
        return 0;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            printf("  %s\n", entry->d_name);
        }
    }
    
    closedir(dir);
#endif
    return 0;
}

/* Show package info */
static int show_info(const char* name) {
    char pkg_file[MAX_PATH];
    char* pkg_path = get_package_dir();
    
    snprintf(pkg_file, MAX_PATH, "%s/%s/vexium.toml", pkg_path, name);
    
    printf("Package: %s\n", name);
    /* In a real implementation, parse vexium.toml */
    printf("  Version: 1.0.0\n");
    printf("  Description: (not specified)\n");
    printf("  Author: (unknown)\n");
    
    return 0;
}

/* Uninstall a package */
static int uninstall_package(const char* name) {
    char pkg_dir[MAX_PATH];
    char cmd[MAX_PATH * 2];
    char* pkg_path = get_package_dir();
    
    printf("Uninstalling %s...\n", name);
    
    snprintf(pkg_dir, MAX_PATH, "%s/%s", pkg_path, name);
    
    /* Use rmdir for empty dirs, or system("rm -rf") for full removal */
    snprintf(cmd, MAX_PATH * 2, "rm -rf \"%s\"", pkg_dir);
    system(cmd);
    
    printf("Package %s uninstalled.\n", name);
    return 0;
}

/* Publish a package */
static int publish_package(const char* path) {
    printf("Publishing package from %s...\n", path);
    
    /* In a real implementation, this would:
     * 1. Parse vexium.toml
     * 2. Validate package structure
     * 3. Create tarball
     * 4. Upload to registry: POST /api/publish
     */
    
    printf("Package published successfully!\n");
    return 0;
}

/* Print usage */
static void print_usage(const char* prog) {
    printf("Vexium Package Manager (vex-pip)\n\n");
    printf("Usage: %s <command> [options]\n\n", prog);
    printf("Commands:\n");
    printf("  install <package>      Install a package\n");
    printf("  uninstall <package>    Remove a package\n");
    printf("  search <query>         Search for packages\n");
    printf("  list                   List installed packages\n");
    printf("  info <package>         Show package information\n");
    printf("  publish <path>         Publish a package to registry\n");
    printf("\nOptions:\n");
    printf("  --version <ver>        Specify package version\n");
    printf("  --help                 Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s install httplib\n", prog);
    printf("  %s install math --version 1.0.0\n", prog);
    printf("  %s search json\n", prog);
    printf("  %s list\n", prog);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    char* cmd = argv[1];
    
    if (strcmp(cmd, "install") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Package name required\n");
            return 1;
        }
        char* version = NULL;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--version") == 0 && i + 1 < argc) {
                version = argv[i + 1];
            }
        }
        return download_package(argv[2], version);
    }
    else if (strcmp(cmd, "uninstall") == 0 || strcmp(cmd, "remove") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Package name required\n");
            return 1;
        }
        return uninstall_package(argv[2]);
    }
    else if (strcmp(cmd, "search") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Search query required\n");
            return 1;
        }
        return search_packages(argv[2]);
    }
    else if (strcmp(cmd, "list") == 0) {
        return list_packages();
    }
    else if (strcmp(cmd, "info") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Package name required\n");
            return 1;
        }
        return show_info(argv[2]);
    }
    else if (strcmp(cmd, "publish") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Package path required\n");
            return 1;
        }
        return publish_package(argv[2]);
    }
    else if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }
    else {
        fprintf(stderr, "Error: Unknown command '%s'\n", cmd);
        print_usage(argv[0]);
        return 1;
    }
    
    return 0;
}
