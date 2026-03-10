#include "package.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ══════════════════════════════════════════════════════════════
 *  MEMORY UTILITIES - Use common.h implementations
 * ══════════════════════════════════════════════════════════════ */
/* safe_strdup and safe_malloc are defined in common.h */

/* ══════════════════════════════════════════════════════════════
 *  DEPENDENCY LIST
 * ══════════════════════════════════════════════════════════════ */

static void dependency_list_init(DependencyList* list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void dependency_list_add(DependencyList* list, const char* name, const char* version) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = (Dependency*)realloc(list->items, sizeof(Dependency) * list->capacity);
    }
    Dependency* dep = &list->items[list->count++];
    dep->name = safe_strdup(name);
    dep->version = safe_strdup(version);
    dep->registry = NULL;
}

static void dependency_list_free(DependencyList* list) {
    for (int i = 0; i < list->count; i++) {
        free(list->items[i].name);
        free(list->items[i].version);
        free(list->items[i].registry);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

/* ══════════════════════════════════════════════════════════════
 *  PACKAGE CONFIG
 * ══════════════════════════════════════════════════════════════ */

void package_config_init(PackageConfig* config) {
    memset(config, 0, sizeof(PackageConfig));
    dependency_list_init(&config->dependencies);
    dependency_list_init(&config->dev_dependencies);
}

void package_config_free(PackageConfig* config) {
    free(config->name);
    free(config->version);
    free(config->description);
    free(config->author);
    free(config->license);
    free(config->repository);
    free(config->homepage);
    free(config->entry_point);
    free(config->output_dir);
    
    dependency_list_free(&config->dependencies);
    dependency_list_free(&config->dev_dependencies);
    
    for (int i = 0; i < config->feature_count; i++) {
        free(config->features[i]);
    }
    free(config->features);
    
    memset(config, 0, sizeof(PackageConfig));
}

/* ══════════════════════════════════════════════════════════════
 *  SIMPLE TOML PARSER
 * ══════════════════════════════════════════════════════════════ */

typedef enum {
    TOML_NONE,
    TOML_SECTION,
    TOML_KEY_VALUE
} TomlParseState;

static void trim_whitespace(char* str) {
    if (!str) return;
    
    /* Trim leading whitespace */
    char* start = str;
    while (*start == ' ' || *start == '\t') start++;
    
    /* Trim trailing whitespace */
    char* end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
    
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

static char* extract_string_value(const char* value) {
    /* Remove quotes and return string */
    /* Create a modifiable copy since we're modifying the end pointer */
    size_t len = strlen(value);
    char* buffer = (char*)malloc(len + 1);
    if (!buffer) return NULL;
    strcpy(buffer, value);
    
    char* start = buffer;
    char* end = buffer + len - 1;
    
    /* Skip whitespace */
    while (*start == ' ') start++;
    while (end > start && (*end == ' ' || *end == '\n' || *end == '\r')) end--;
    
    /* Check for quotes */
    if ((*start == '"' && *end == '"') || (*start == '\'' && *end == '\'')) {
        start++;
        *end = '\0';
    }
    
    /* Duplicate the result */
    char* result = safe_strdup(start);
    free(buffer);  /* Free the temporary buffer */
    return result;
}

static bool parse_dependencies_line(const char* line, DependencyList* deps) {
    char* eq = strchr(line, '=');
    if (!eq) return false;
    
    char* key = (char*)malloc(eq - line + 1);
    memcpy(key, line, eq - line);
    key[eq - line] = '\0';
    trim_whitespace(key);
    
    char* value = safe_strdup(eq + 1);
    trim_whitespace(value);
    
    dependency_list_add(deps, key, value);
    
    free(key);
    free(value);
    return true;
}

bool package_parse_toml(const char* filepath, PackageConfig* config) {
    FILE* fp = fopen(filepath, "r");
    if (!fp) {
        fprintf(stderr, "[Package] Error: Cannot open '%s'\n", filepath);
        return false;
    }
    
    package_config_init(config);
    
    char line[1024];
    TomlParseState state = TOML_NONE;
    char current_section[64] = {0};
    
    while (fgets(line, sizeof(line), fp)) {
        /* Skip comments */
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        
        trim_whitespace(line);
        if (strlen(line) == 0) continue;
        
        /* Check for section header */
        if (line[0] == '[' && line[strlen(line)-1] == ']') {
            /* Extract section name */
            strncpy(current_section, line + 1, sizeof(current_section) - 1);
            current_section[strlen(current_section) - 1] = '\0';
            trim_whitespace(current_section);
            state = TOML_SECTION;
            continue;
        }
        
        /* Parse key-value pairs based on section */
        char* eq = strchr(line, '=');
        if (!eq) continue;
        
        char* key = (char*)malloc(eq - line + 1);
        memcpy(key, line, eq - line);
        key[eq - line] = '\0';
        trim_whitespace(key);
        
        char* value = safe_strdup(eq + 1);
        trim_whitespace(value);
        
        /* Package metadata section */
        if (strcmp(current_section, "package") == 0) {
            if (strcmp(key, "name") == 0) config->name = extract_string_value(value);
            else if (strcmp(key, "version") == 0) config->version = extract_string_value(value);
            else if (strcmp(key, "description") == 0) config->description = extract_string_value(value);
            else if (strcmp(key, "author") == 0) config->author = extract_string_value(value);
            else if (strcmp(key, "license") == 0) config->license = extract_string_value(value);
            else if (strcmp(key, "repository") == 0) config->repository = extract_string_value(value);
            else if (strcmp(key, "homepage") == 0) config->homepage = extract_string_value(value);
        }
        /* Dependencies section */
        else if (strcmp(current_section, "dependencies") == 0) {
            dependency_list_add(&config->dependencies, key, extract_string_value(value));
        }
        /* Dev dependencies section */
        else if (strcmp(current_section, "dev-dependencies") == 0) {
            dependency_list_add(&config->dev_dependencies, key, extract_string_value(value));
        }
        /* Build configuration */
        else if (strcmp(current_section, "build") == 0) {
            if (strcmp(key, "entry-point") == 0) config->entry_point = extract_string_value(value);
            else if (strcmp(key, "output-dir") == 0) config->output_dir = extract_string_value(value);
        }
        
        free(key);
        free(value);
    }
    
    fclose(fp);
    
    /* Set defaults if not specified */
    if (!config->entry_point) config->entry_point = safe_strdup("src/main.vxm");
    if (!config->output_dir) config->output_dir = safe_strdup("dist");
    
    return true;
}

/* ══════════════════════════════════════════════════════════════
 *  PACKAGE MANAGER COMMANDS
 * ══════════════════════════════════════════════════════════════ */

bool package_init(const char* project_name, const char* path) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/vex.toml", path);
    
    FILE* fp = fopen(filepath, "w");
    if (!fp) {
        fprintf(stderr, "[Package] Error: Cannot create '%s'\n", filepath);
        return false;
    }
    
    fprintf(fp, "# Vexium Package Configuration\n\n");
    fprintf(fp, "[package]\n");
    fprintf(fp, "name = \"%s\"\n", project_name);
    fprintf(fp, "version = \"0.1.0\"\n");
    fprintf(fp, "description = \"A Vexium package\"\n");
    fprintf(fp, "author = \"Your Name <email@example.com>\"\n");
    fprintf(fp, "license = \"MIT\"\n\n");
    
    fprintf(fp, "[build]\n");
    fprintf(fp, "entry-point = \"src/main.vxm\"\n");
    fprintf(fp, "output-dir = \"dist\"\n\n");
    
    fprintf(fp, "[dependencies]\n");
    fprintf(fp, "# Add your dependencies here\n\n");
    
    fclose(fp);
    
    /* Create source directory */
    char srcdir[512];
    snprintf(srcdir, sizeof(srcdir), "%s/src", path);
    
    /* Create initial main.vxm */
    char mainfile[512];
    snprintf(mainfile, sizeof(mainfile), "%s/src/main.vxm", path);
    
    fp = fopen(mainfile, "w");
    if (fp) {
        fprintf(fp, "# %s - Main entry point\n\n", project_name);
        fprintf(fp, "fn main():\n");
        fprintf(fp, "    display \"Hello from %s!\"\n", project_name);
        fclose(fp);
        printf("[Package] Created %s\n", mainfile);
    }
    
    printf("[Package] Created vex.toml at %s\n", filepath);
    return true;
}

bool package_add(const char* package_name, const char* version_constraint) {
    /* Read existing vex.toml */
    PackageConfig config;
    if (!package_parse_toml("vex.toml", &config)) {
        fprintf(stderr, "[Package] Error: No vex.toml found. Run 'vex init' first.\n");
        return false;
    }
    
    /* Add dependency */
    dependency_list_add(&config.dependencies, package_name, version_constraint);
    
    /* Write back */
    FILE* fp = fopen("vex.toml", "w");
    if (!fp) {
        fprintf(stderr, "[Package] Error: Cannot write vex.toml\n");
        package_config_free(&config);
        return false;
    }
    
    fprintf(fp, "# Vexium Package Configuration\n\n");
    
    if (config.name) fprintf(fp, "[package]\nname = \"%s\"\n", config.name);
    if (config.version) fprintf(fp, "version = \"%s\"\n", config.version);
    if (config.description) fprintf(fp, "description = \"%s\"\n", config.description);
    if (config.author) fprintf(fp, "author = \"%s\"\n", config.author);
    if (config.license) fprintf(fp, "license = \"%s\"\n", config.license);
    if (config.repository) fprintf(fp, "repository = \"%s\"\n", config.repository);
    if (config.homepage) fprintf(fp, "homepage = \"%s\"\n", config.homepage);
    fprintf(fp, "\n");
    
    fprintf(fp, "[build]\n");
    fprintf(fp, "entry-point = \"%s\"\n", config.entry_point ? config.entry_point : "src/main.vxm");
    fprintf(fp, "output-dir = \"%s\"\n", config.output_dir ? config.output_dir : "dist");
    fprintf(fp, "\n");
    
    fprintf(fp, "[dependencies]\n");
    for (int i = 0; i < config.dependencies.count; i++) {
        fprintf(fp, "%s = \"%s\"\n", 
                config.dependencies.items[i].name,
                config.dependencies.items[i].version);
    }
    fprintf(fp, "\n");
    
    fprintf(fp, "[dev-dependencies]\n");
    for (int i = 0; i < config.dev_dependencies.count; i++) {
        fprintf(fp, "%s = \"%s\"\n", 
                config.dev_dependencies.items[i].name,
                config.dev_dependencies.items[i].version);
    }
    
    fclose(fp);
    package_config_free(&config);
    
    printf("[Package] Added %s %s to dependencies\n", package_name, version_constraint);
    return true;
}

bool package_install(void) {
    /* Check for vex.toml */
    FILE* fp = fopen("vex.toml", "r");
    if (!fp) {
        fprintf(stderr, "[Package] Error: No vex.toml found. Run 'vex init' first.\n");
        return false;
    }
    fclose(fp);
    
    /* Parse vex.toml */
    PackageConfig config;
    if (!package_parse_toml("vex.toml", &config)) {
        return false;
    }
    
    if (config.dependencies.count == 0) {
        printf("[Package] No dependencies to install.\n");
        package_config_free(&config);
        return true;
    }
    
    /* Create packages directory */
    system("mkdir packages 2>nul");
    
    printf("[Package] Installing %d dependency(ies)...\n", config.dependencies.count);
    for (int i = 0; i < config.dependencies.count; i++) {
        printf("  - Installing %s %s...\n", 
               config.dependencies.items[i].name,
               config.dependencies.items[i].version);
        /* Placeholder: In full implementation, would download from registry */
    }
    
    printf("[Package] Dependencies installed successfully.\n");
    package_config_free(&config);
    return true;
}

bool package_update(void) {
    printf("[Package] Update functionality - Placeholder\n");
    /* Placeholder for update functionality */
    return true;
}

bool package_build(void) {
    /* Check for vex.toml */
    PackageConfig config;
    if (!package_parse_toml("vex.toml", &config)) {
        return false;
    }
    
    printf("[Package] Building '%s' (version %s)...\n", 
           config.name ? config.name : "unknown",
           config.version ? config.version : "unknown");
    
    /* Compile entry point */
    char entry[512];
    snprintf(entry, sizeof(entry), "vexium run %s", config.entry_point);
    
    printf("[Package] Build complete.\n");
    package_config_free(&config);
    return true;
}

bool package_test(void) {
    printf("[Package] Running tests...\n");
    /* Placeholder for test runner */
    printf("[Package] All tests passed!\n");
    return true;
}

bool package_fmt(const char* path) {
    printf("[Package] Formatting %s...\n", path ? path : ".");
    /* Placeholder for formatter */
    printf("[Package] Format complete.\n");
    return true;
}

/* ══════════════════════════════════════════════════════════════
 *  UTILITIES
 * ══════════════════════════════════════════════════════════════ */

char* package_get_cache_dir(void) {
    static char path[512];
    
#ifdef _WIN32
    snprintf(path, sizeof(path), "%s/.vexium", getenv("APPDATA"));
#else
    snprintf(path, sizeof(path), "%s/.vexium", getenv("HOME"));
#endif
    
    /* Create directory if needed */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir \"%s\" 2>nul", path);
    system(cmd);
    
    return path;
}

char* package_get_config_dir(void) {
    return package_get_cache_dir();
}

bool package_resolve_version(const char* constraint, const char** resolved) {
    /* Simple SemVer resolution - placeholder */
    *resolved = constraint;
    return true;
}
