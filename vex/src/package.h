#ifndef VEXIUM_PACKAGE_H
#define VEXIUM_PACKAGE_H

#include "common.h"

/* ══════════════════════════════════════════════════════════════
 *  PACKAGE MANAGER TYPES
 * ══════════════════════════════════════════════════════════════ */

/* Dependency specification */
typedef struct {
    char* name;           /* Package name */
    char* version;        /* Version constraint (e.g., "^1.0.0", ">=2.0.0") */
    char* registry;       /* Optional: custom registry URL */
} Dependency;

/* Dependency list */
typedef struct {
    Dependency* items;
    int count;
    int capacity;
} DependencyList;

/* Package metadata from vex.toml */
typedef struct {
    /* Package section */
    char* name;
    char* version;
    char* description;
    char* author;
    char* license;
    char* repository;
    char* homepage;
    
    /* Dependencies */
    DependencyList dependencies;
    DependencyList dev_dependencies;
    
    /* Configuration */
    char* entry_point;    /* Default: "src/main.vxm" */
    char* output_dir;     /* Default: "dist" */
    char** features;      /* Optional feature flags */
    int feature_count;
} PackageConfig;

/* ══════════════════════════════════════════════════════════════
 *  TOML PARSER
 * ══════════════════════════════════════════════════════════════ */

/* Parse vex.toml file into PackageConfig */
bool package_parse_toml(const char* filepath, PackageConfig* config);

/* Free package config memory */
void package_config_free(PackageConfig* config);

/* ══════════════════════════════════════════════════════════════
 *  PACKAGE MANAGER COMMANDS
 * ══════════════════════════════════════════════════════════════ */

/* Initialize new project with vex.toml */
bool package_init(const char* project_name, const char* path);

/* Add dependency to vex.toml */
bool package_add(const char* package_name, const char* version_constraint);

/* Install all dependencies from vex.toml */
bool package_install(void);

/* Update dependencies to latest compatible versions */
bool package_update(void);

/* Build package */
bool package_build(void);

/* Run package tests */
bool package_test(void);

/* Format source files */
bool package_fmt(const char* path);

/* ══════════════════════════════════════════════════════════════
 *  UTILITIES
 * ══════════════════════════════════════════════════════════════ */

/* Get cache directory path */
char* package_get_cache_dir(void);

/* Get global config directory */
char* package_get_config_dir(void);

/* Resolve dependency version using SemVer */
bool package_resolve_version(const char* constraint, const char** resolved);

#endif /* VEXIUM_PACKAGE_H */
