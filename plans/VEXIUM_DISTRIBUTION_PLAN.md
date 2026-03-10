# Vexium Distribution Plan

## Overview

This plan outlines how to make Vexium installable and accessible like Python - with installers, editor support, and a package manager.

---

## 1. Windows Installer

### Options:
- **NSIS** - Nullsoft Scriptable Install System (free, widely used)
- **WiX** - Windows Installer XML (Microsoft-supported)
- **Inno Setup** - Free, simple

### Recommended: NSIS

```nsis
; vexium.nsi
!include "MUI2.nsh"

Name "Vexium"
OutFile "vexium-installer.exe"
InstallDir "$PROGRAMFILES\Vexium"

Section "Install"
    File "vexium.exe"
    File /r "lib\*.*"
    File "stdlib\*.*"
    
    ; Add to PATH
    WriteRegStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PATH" "$PATH;$INSTDIR"
    
    ; File association for .vxm files
    WriteRegStr HKCR ".vxm" "" "Vexium.File"
    WriteRegStr HKCR "Vexium.File" "" "Vexium Source File"
    WriteRegStr HKCR "Vexium.File\shell\open\command" "" '"$INSTDIR\vexium.exe" "%1"'
SectionEnd
```

### Build Steps:
1. Install NSIS from https://nsis.sourceforge.io/
2. Run: `makensis vexium.nsi`
3. Output: `vexium-installer.exe`

---

## 2. VS Code Extension

### Directory Structure:
```
vexium-vscode/
├── package.json          # Extension manifest
├── syntaxes/
│   └── vexium.json      # Syntax highlighting
├── language-configuration.json
├── src/
│   └── extension.ts    # Extension code
└── README.md
```

### package.json:
```json
{
  "name": "vexium-language",
  "displayName": "Vexium Language",
  "description": "Language support for Vexium",
  "version": "1.0.0",
  "publisher": "vexium",
  "engines": { "vscode": "^1.70.0" },
  "categories": ["Programming Languages"],
  "languages": [{
    "id": "vexium",
    "aliases": ["Vexium", "vexium"],
    "extensions": [".vxm", ".vxmc"],
    "configuration": "language-configuration.json"
  }],
  "grammars": [{
    "language": "vexium",
    "scopeName": "source.vexium",
    "path": "./syntaxes/vexium.json"
  }]
}
```

### Syntax Highlighting (vexium.json):
```json
{
  "scopeName": "source.vexium",
  "patterns": [
    { "include": "#keywords" },
    { "include": "#strings" },
    { "include": "#numbers" },
    { "include": "#comments" }
  ],
  "repository": {
    "keywords": {
      "patterns": [
        { "name": "keyword.control.vexium", "match": "\\b(fn|if|else|while|for|let|const|struct|can|has|give back|display|spawn|channel|task)\\b" }
      ]
    }
  }
}
```

---

## 3. Package Manager (vex-pip)

### Similar to Python's pip

#### vex-cli:
```c
// Main entry point for package manager
int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }
    
    char* command = argv[1];
    
    if (strcmp(command, "install") == 0) {
        return package_install(argc - 2, argv + 2);
    }
    else if (strcmp(command, "publish") == 0) {
        return package_publish(argc - 2, argv + 2);
    }
    else if (strcmp(command, "search") == 0) {
        return package_search(argc - 2, argv + 2);
    }
    // ... other commands
}
```

#### Package Structure (vexium package):
```
mypackage/
├── __init__.vxm        # Package entry point
├── module1.vxm         # Modules
├── module2.vxm
└── vexium.toml         # Package metadata

vexium.toml:
[name = "mypackage"]
version = "1.0.0"
description = "My Vexium package"
author = "Your Name"
dependencies = ["otherpackage>=1.0.0"]
```

#### Registry API:
- `POST /api/publish` - Upload package
- `GET /api/package/<name>` - Get package info
- `GET /api/search?q=<query>` - Search packages
- `GET /api/package/<name>/<version>` - Download package

---

## 4. Installation Documentation

### Quick Install (Windows):
```powershell
# Option 1: Winget (recommended)
winget install Vexium.Vexium

# Option 2: Chocolatey
choco install vexium

# Option 3: Direct Download
Invoke-WebRequest -Uri "https://vexium.dev/download/vexium-latest.exe" -OutFile "vexium.exe"
```

### Verify Installation:
```bash
vexium --version
vexium --help
```

---

## 5. Directory Structure for Distribution

```
vexium/
├── bin/
│   └── vexium.exe          # Main executable
├── lib/
│   └── vexium/            # Standard library
├── include/
│   └── vexium.h           # C API header
├── packages/              # Installed packages (vex-pip)
├── vexium.toml           # Global config
└── README.txt
```

---

## 6. Implementation Tasks

| Task | Priority | Effort |
|------|----------|--------|
| Create NSIS installer script | High | 1 day |
| Build Windows executable | High | 1 day |
| Create VS Code extension | Medium | 3 days |
| Write syntax highlighting | Medium | 1 day |
| Implement vex-pip CLI | Medium | 5 days |
| Create package registry | Low | 10 days |
| Write installation docs | Medium | 1 day |

---

## 7. Next Steps

1. **First**: Fix any remaining build issues
2. **Then**: Create NSIS installer script  
3. **Then**: Build and test Windows executable
4. **Then**: Create VS Code extension
5. **Then**: Implement package manager

---

*Plan created: 2026-03-09*
