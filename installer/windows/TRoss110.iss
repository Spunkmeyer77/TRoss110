; Inno Setup script for the Windows installer.
; Build:  ISCC /DAppVersion=0.1.0 installer\windows\TRoss110.iss
; (paths are relative to this file; expects a Release build in ..\..\build)

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

#define Artefacts "..\..\build\TRoss110_artefacts\Release"

[Setup]
AppId={{E18BEA30-1A50-4DC9-883E-4F8BD6CCE2C0}
AppName=TRoss 110
AppVersion={#AppVersion}
AppVerName=TRoss 110 {#AppVersion}
AppPublisher=Ticco Ross
DefaultDirName={autopf}\Ticco Ross\TRoss 110
DefaultGroupName=Ticco Ross
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=..\..\dist
OutputBaseFilename=TRoss110-{#AppVersion}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=TRoss 110

[Types]
Name: "full";   Description: "VST3 plug-in and standalone app"
Name: "custom"; Description: "Choose components"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (Ableton Live, Bitwig, Reaper, Cubase...)"; Types: full custom
Name: "app";  Description: "Standalone app";                                         Types: full custom

[Files]
Source: "{#Artefacts}\VST3\TRoss 110.vst3\*"; DestDir: "{commoncf64}\VST3\TRoss 110.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Artefacts}\Standalone\TRoss 110.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\TRoss 110"; Filename: "{app}\TRoss 110.exe"; Components: app

[Messages]
FinishedLabel=TRoss 110 is installed.%n%nRescan plug-ins in your DAW (Ableton Live: Settings > Plug-Ins > Rescan).
