#ifndef SourceDir
  #define SourceDir ""
#endif
#ifndef OutputDir
  #define OutputDir ""
#endif
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

[Setup]
AppId={{B9D87858-5B5A-4CB7-8B78-8A1D6F0D1E20}
AppName=DouyuMonitor
AppVersion={#AppVersion}
AppPublisher=DouyuMonitor
DefaultDirName={localappdata}\Programs\DouyuMonitor
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=DouyuMonitor-Setup-V{#AppVersion}
SetupIconFile={#SourceDir}\douyu_monitor.ico
UninstallDisplayIcon={app}\douyu_monitor.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=no

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加快捷方式:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\DouyuMonitor\DouyuMonitor"; Filename: "{app}\douyu_monitor_native.exe"; WorkingDir: "{app}"; IconFilename: "{app}\douyu_monitor.ico"
Name: "{autoprograms}\DouyuMonitor\卸载 DouyuMonitor"; Filename: "{uninstallexe}"; IconFilename: "{app}\douyu_monitor.ico"
Name: "{autodesktop}\DouyuMonitor"; Filename: "{app}\douyu_monitor_native.exe"; WorkingDir: "{app}"; IconFilename: "{app}\douyu_monitor.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\douyu_monitor_native.exe"; Description: "启动 DouyuMonitor"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent unchecked

[Code]
function IsDriveRoot(const Value: string): Boolean;
begin
  Result := ((Length(Value) = 2) and (Value[2] = ':')) or
    ((Length(Value) = 3) and (Value[2] = ':') and (Value[3] = '\'));
end;

function NormalizeInstallDirectory(const Value: string): string;
var
  Candidate: string;
begin
  Candidate := Trim(Value);
  if (Length(Candidate) = 2) and (Candidate[2] = ':') then
    Candidate := Candidate + '\';
  if IsDriveRoot(Candidate) then
    Candidate := AddBackslash(Candidate) + 'DouyuMonitor';
  Result := Candidate;
end;

function ValidateInstallDirectory: Boolean;
var
  Candidate: string;
begin
  Candidate := NormalizeInstallDirectory(WizardDirValue);
  if Candidate <> WizardDirValue then
    WizardForm.DirEdit.Text := Candidate;
  Result := not IsDriveRoot(Candidate);
  if not Result then
    MsgBox('不能直接安装到磁盘根目录，请选择一个文件夹。', mbError, MB_OK);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpSelectDir then
    Result := ValidateInstallDirectory;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  NeedsRestart := False;
  if ValidateInstallDirectory then
    Result := ''
  else
    Result := '安装目录无效，不能使用磁盘根目录。';
end;
