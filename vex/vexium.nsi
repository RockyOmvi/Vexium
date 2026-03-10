; Vexium Installer Script for NSIS
; Download NSIS from https://nsis.sourceforge.io/

!include "MUI2.nsh"

; General settings
Name "Vexium"
OutFile "vexium-installer.exe"
InstallDir "$PROGRAMFILES\Vexium"
InstallDirRegKey HKLM "Software\Vexium" "InstallDir"
RequestExecutionLevel admin

; Version info
VIProductVersion "1.0.0.0"
VIAddVersionKey "ProductName" "Vexium"
VIAddVersionKey "CompanyName" "Vexium"
VIAddVersionKey "LegalCopyright" "Copyright 2026"
VIAddVersionKey "FileDescription" "Vexium Programming Language"
VIAddVersionKey "FileVersion" "1.0.0"

; Interface settings
!define MUI_ABORTWARNING

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Languages
!insertmacro MUI_LANGUAGE "English"

; Installer section
Section "Install"
    SetOutPath "$INSTDIR"
    
    ; Copy main executable
    File "vexium.exe"
    
    ; Copy standard library
    SetOutPath "$INSTDIR\lib"
    File /r "lib\*.vxm"
    
    ; Create uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    
    ; Add to PATH
    ReadRegStr $0 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path"
    StrCpy $1 "$0;$INSTDIR"
    WriteRegStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path" "$1"
    
    ; Create Start Menu shortcuts
    CreateDirectory "$SMPROGRAMS\Vexium"
    CreateShortCut "$SMPROGRAMS\Vexium\Vexium.lnk" "$INSTDIR\vexium.exe"
    CreateShortCut "$SMPROGRAMS\Vexium\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
    
    ; File association for .vxm files
    WriteRegStr HKCR ".vxm" "" "Vexium.File"
    WriteRegStr HKCR ".vxm" "Content Type" "text/x-vexium"
    WriteRegStr HKCR "Vexium.File" "" "Vexium Source File"
    WriteRegStr HKCR "Vexium.File\DefaultIcon" "" "$INSTDIR\vexium.exe,0"
    WriteRegStr HKCR "Vexium.File\shell" "" "open"
    WriteRegStr HKCR "Vexium.File\shell\open" "" "Open with Vexium"
    WriteRegStr HKCR "Vexium.File\shell\open\command" "" '"$INSTDIR\vexium.exe" "%1"'
    
    ; File association for .vxmc files
    WriteRegStr HKCR ".vxmc" "" "Vexium.Bytecode"
    WriteRegStr HKCR "Vexium.Bytecode" "" "Vexium Bytecode"
    WriteRegStr HKCR "Vexium.Bytecode\shell\open\command" "" '"$INSTDIR\vexium.exe" "%1"'
    
    ; Register uninstaller in Add/Remove Programs
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Vexium" "DisplayName" "Vexium Programming Language"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Vexium" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Vexium" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Vexium" "DisplayIcon" "$INSTDIR\vexium.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Vexium" "Publisher" "Vexium"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Vexium" "DisplayVersion" "1.0.0"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Vexium" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Vexium" "NoRepair" 1
    
    ; Save install dir
    WriteRegStr HKLM "Software\Vexium" "InstallDir" "$INSTDIR"
SectionEnd

; Uninstaller section
Section "Uninstall"
    ; Remove from PATH
    ReadRegStr $0 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path"
    ; Note: This is simplified - in production use a more robust method
    DeleteRegKey HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"
    
    ; Remove file associations
    DeleteRegKey HKCR ".vxm"
    DeleteRegKey HKCR "Vexium.File"
    DeleteRegKey HKCR ".vxmc"
    DeleteRegKey HKCR "Vexium.Bytecode"
    
    ; Remove shortcuts
    Delete "$SMPROGRAMS\Vexium\Vexium.lnk"
    Delete "$SMPROGRAMS\Vexium\Uninstall.lnk"
    RMDir "$SMPROGRAMS\Vexium"
    
    ; Remove files
    Delete "$INSTDIR\vexium.exe"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir /r "$INSTDIR\lib"
    RMDir "$INSTDIR"
    
    ; Remove registry keys
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Vexium"
    DeleteRegKey HKLM "Software\Vexium"
SectionEnd
