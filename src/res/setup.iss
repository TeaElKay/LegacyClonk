; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

#define C4ENGINENAME "LegacyClonk"
#define C4PUBLISHER "LegacyClonk Team"
#define C4CLONKSPOTURL "https://clonkspot.org/lc-en"

[Setup]
AppId={{033940D5-5460-4B4D-9174-B0AFF9F53AEA}
AppName={#C4ENGINENAME}
AppVersion={#C4VERSION} [{#C4XVERBUILD}]
AppVerName={#C4ENGINENAME} {#C4VERSION} [{#C4XVERBUILD}]
AppPublisher={#C4PUBLISHER}
AppPublisherURL={#C4CLONKSPOTURL}
AppSupportURL={#C4CLONKSPOTURL}
AppUpdatesURL={#C4CLONKSPOTURL}
DefaultDirName={localappdata}\{#C4ENGINENAME}
DefaultGroupName={#C4ENGINENAME}
OutputBaseFilename=lc_setup_win
DisableProgramGroupPage=yes
LicenseFile=clonk_trademark_license.txt
; Remove the following line to run in administrative install mode (install for all users.)
PrivilegesRequired=lowest
SetupIconFile={#SourcePath}\lc.ico
; {#C4SOURCEDIR}
SourceDir={#C4SOURCEDIR}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DisableWelcomePage=no
WizardImageFile={#SourcePath}\setup.bmp
DisableDirPage=no
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[Files]
Source: "clonk.exe"; DestDir: "{app}"; Flags: ignoreversion; Check: Is64BitInstallMode
Source: "clonk.pdb"; DestDir: "{app}"; Flags: ignoreversion; Check: Is64BitInstallMode
Source: "c4group.exe"; DestDir: "{app}"; Flags: ignoreversion; Check: Is64BitInstallMode
Source: "c4group.pdb"; DestDir: "{app}"; Flags: ignoreversion; Check: Is64BitInstallMode

Source: "x86\clonk.exe"; DestDir: "{app}"; DestName: "clonk.exe"; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: "x86\clonk.pdb"; DestDir: "{app}"; DestName: "clonk.pdb"; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: "x86\c4group.exe"; DestDir: "{app}"; DestName: "c4group.exe"; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: "x86\c4group.pdb"; DestDir: "{app}"; DestName: "c4group.pdb"; Flags: ignoreversion; Check: not Is64BitInstallMode

Source: "clonk_content_license.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "clonk_trademark_license.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "Fantasy.c4d"; DestDir: "{app}"; Flags: ignoreversion
Source: "Fantasy.c4f"; DestDir: "{app}"; Flags: ignoreversion
Source: "FarWorlds.c4d"; DestDir: "{app}"; Flags: ignoreversion
Source: "FarWorlds.c4f"; DestDir: "{app}"; Flags: ignoreversion
Source: "Graphics.c4g"; DestDir: "{app}"; Flags: ignoreversion
Source: "Hazard.c4d"; DestDir: "{app}"; Flags: ignoreversion
Source: "Hazard.c4f"; DestDir: "{app}"; Flags: ignoreversion
Source: "Knights.c4d"; DestDir: "{app}"; Flags: ignoreversion
Source: "Knights.c4f"; DestDir: "{app}"; Flags: ignoreversion
Source: "Material.c4g"; DestDir: "{app}"; Flags: ignoreversion
Source: "Melees.c4f"; DestDir: "{app}"; Flags: ignoreversion
Source: "Missions.c4f"; DestDir: "{app}"; Flags: ignoreversion
Source: "Music.c4g"; DestDir: "{app}"; Flags: ignoreversion
Source: "Objects.c4d"; DestDir: "{app}"; Flags: ignoreversion
Source: "Races.c4f"; DestDir: "{app}"; Flags: ignoreversion
Source: "Sound.c4g"; DestDir: "{app}"; Flags: ignoreversion
Source: "System.c4g"; DestDir: "{app}"; Flags: ignoreversion
Source: "Tutorial.c4f"; DestDir: "{app}"; Flags: ignoreversion
Source: "Western.c4d"; DestDir: "{app}"; Flags: ignoreversion
Source: "Western.c4f"; DestDir: "{app}"; Flags: ignoreversion
Source: "Worlds.c4f"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#C4ENGINENAME}"; Filename: "{app}\clonk.exe"; WorkingDir: "{app}"

[Run]
Filename: "{app}\clonk.exe"; Description: "{cm:LaunchProgram,{#StringChange(C4ENGINENAME, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
