#define APP_NAME "QuickEvent"
#define APP_NAME_LOWER "quickevent"
#define SSL_DIR "C:\Qt\Tools\OpenSSL\Win_x64\bin"

#define VERSION "3.2.1"

#define QT_DIR "c:\Qt\6.7.2\mingw_64"
#define MINGW_DIR "c:\Qt\Tools\mingw1310_64"
#define POSTGRES_DIR "c:\Users\x\Downloads\pgsql"
#define TYPST_DIR "c:\Users\x\Downloads\typst-x86_64-pc-windows-msvc"
#define BUILD_DIR "c:\p\quickbox\build\Desktop-Release\install"

[Setup]
AppName={#APP_NAME}
AppVerName={#APP_NAME} {#VERSION}
AppPublisher=Fanda Vacek
AppPublisherURL=http://www.{#APP_NAME_LOWER}.cz
AppSupportURL=http://www.{#APP_NAME_LOWER}.cz
AppUpdatesURL=http://www.{#APP_NAME_LOWER}.cz
DefaultDirName=C:\{#APP_NAME}
DefaultGroupName={#APP_NAME}
OutputDir={#BUILD_DIR}\_inno\{#APP_NAME_LOWER}
DisableDirPage=no
OutputBaseFilename={#APP_NAME_LOWER}-{#VERSION}-setup
Compression=lzma
SolidCompression=yes

[Tasks]
Name: desktopicon; Description: {cm:CreateDesktopIcon}; GroupDescription: {cm:AdditionalIcons}; Flags: unchecked
Name: quicklaunchicon; Description: {cm:CreateQuickLaunchIcon}; GroupDescription: {cm:AdditionalIcons}; Flags: unchecked

[Files]
Source: {#BUILD_DIR}\bin\*; DestDir: {app}; Flags: ignoreversion recursesubdirs
Source: {#QT_DIR}\bin\Qt6*.dll; DestDir: {app}; Flags: ignoreversion recursesubdirs

Source: {#QT_DIR}\plugins\platforms\*; DestDir: {app}\platforms; Flags: ignoreversion recursesubdirs
Source: {#QT_DIR}\plugins\sqldrivers\*; DestDir: {app}\sqldrivers; Flags: ignoreversion recursesubdirs
Source: {#QT_DIR}\plugins\imageformats\*; DestDir: {app}\imageformats; Flags: ignoreversion recursesubdirs
Source: {#QT_DIR}\plugins\iconengines\*; DestDir: {app}\iconengines; Flags: ignoreversion recursesubdirs
Source: {#QT_DIR}\plugins\multimedia\*; DestDir: {app}\multimedia; Flags: ignoreversion recursesubdirs
Source: {#QT_DIR}\plugins\tls\*; DestDir: {app}\tls; Flags: ignoreversion recursesubdirs
Source: {#QT_DIR}\plugins\styles\*; DestDir: {app}\styles; Flags: ignoreversion recursesubdirs

Source: {#QT_DIR}\qml\QtQml\*; DestDir: {app}/qml\QtQml; Flags: ignoreversion recursesubdirs

Source: {#MINGW_DIR}\bin\libstdc++-6.dll; DestDir: {app}; Flags: ignoreversion recursesubdirs
Source: {#MINGW_DIR}\bin\libgcc_s_seh-1.dll; DestDir: {app}; Flags: ignoreversion recursesubdirs
Source: {#MINGW_DIR}\bin\libwinpthread-1.dll; DestDir: {app}; Flags: ignoreversion recursesubdirs

Source: {#POSTGRES_DIR}\bin\libpq.dll; DestDir: {app}; Flags: ignoreversion recursesubdirs
Source: {#POSTGRES_DIR}\bin\libiconv-2.dll; DestDir: {app}; Flags: ignoreversion recursesubdirs
Source: {#POSTGRES_DIR}\bin\libintl-9.dll; DestDir: {app}; Flags: ignoreversion recursesubdirs
Source: {#POSTGRES_DIR}\bin\liblz4.dll; DestDir: {app}; Flags: ignoreversion recursesubdirs
Source: {#POSTGRES_DIR}\bin\liblz4.dll; DestDir: {app}; Flags: ignoreversion recursesubdirs
Source: {#POSTGRES_DIR}\bin\libpgtypes.dll; DestDir: {app}; Flags: ignoreversion recursesubdirs
Source: {#POSTGRES_DIR}\bin\libcrypto-3-x64.dll; DestDir: {app}; Flags: ignoreversion recursesubdirs
Source: {#POSTGRES_DIR}\bin\libecpg.dll; DestDir: {app}; Flags: ignoreversion recursesubdirs
Source: {#POSTGRES_DIR}\bin\libssl-3-x64.dll; DestDir: {app}; Flags: ignoreversion recursesubdirs

Source: {#TYPST_DIR}\typst.exe; DestDir: {app}; Flags: ignoreversion recursesubdirs

[Icons]
Name: {group}\{#APP_NAME}; Filename: {app}\{#APP_NAME_LOWER}.exe
Name: {group}\{cm:UninstallProgram,{#APP_NAME}}; Filename: {uninstallexe}
Name: {userdesktop}\{#APP_NAME}; Filename: {app}\{#APP_NAME_LOWER}.exe; Tasks: desktopicon
Name: {userappdata}\Microsoft\Internet Explorer\Quick Launch\{#APP_NAME}; Filename: {app}\{#APP_NAME_LOWER}.exe; Tasks: quicklaunchicon

[Run]
Filename: {app}\{#APP_NAME_LOWER}.exe; Description: {cm:LaunchProgram,{#APP_NAME}}; Flags: nowait postinstall skipifsilent
