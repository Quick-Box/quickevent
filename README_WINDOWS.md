# Building QuickEvent on Windows

This guide covers building QuickEvent on Windows (including Windows ARM). The build targets x86_64 using MinGW, which runs natively on x64 and via emulation on Windows ARM.

It is heavily inspired by the `.github\workflows\installer.yml` GitHub workflow.

---

## Tools to Install

### 1. Qt 6.10.x (includes MinGW and CMake)
- Download the Qt Online Installer from https://www.qt.io/download-qt-installer
- In the installer, select:
  - **Component**: `Qt 6.10.x → MinGW 64-bit` (`win64_mingw`)
  - **Required extra modules**: `Qt Serial Port`, `Qt Multimedia`
  - Under **Tools**: ensure `MinGW 13.1.0 64-bit` and `CMake` are checked (they are by default)
- Note your Qt install root (e.g. `C:\Qt` or `C:\dev\Qtx86`)

> Qt bundles the correct MinGW variant (msvcrt, not ucrt) under `Tools\$MINGW_VER\`. No separate MinGW install is needed.

### 2. PostgreSQL 17 — x86-64 build required
- **Windows ARM users**: do NOT use `winget` — it may install the ARM64 native version, which cannot be loaded by the x86_64 binary.
- Use Chocolatey (delivers x64): `choco install postgresql17 -y`
- Or download the **Windows x86-64** installer explicitly from https://www.enterprisedb.com/downloads/postgres-postgresql-downloads
- Required for `libpq` headers at build time and DLLs at runtime

### 3. OpenSSL 3.x — x86-64 build required
- **Windows ARM users**: do NOT use `winget` — it may install the ARM64 native version.
- Use Chocolatey (delivers x64): `choco install openssl.light --version=3.1.4 -y`
- Or download **Win64 OpenSSL** (not Win ARM64) from https://slproweb.com/products/Win32OpenSSL.html
- Required at runtime for Qt's network/SSL support

---

## Build Commands

Run the following from a **PowerShell** terminal.

Set `$QT_ROOT` to your Qt install folder. Everything else is derived from it:

```powershell
$QT_ROOT = "C:\dev\Qtx86"   # adjust to your actual Qt install root
$QT_VER  = "6.10.3"         # adjust to the installed Qt version
$MINGW_VER = "mingw1310_64" # adjust to the installed Qt version
$QUICKBOX_ROOT = $PWD       # adjust to the quickbox repo root if necessary
```

One-time PATH setup (restart PowerShell after running this):
```powershell
$p = [Environment]::GetEnvironmentVariable("PATH", "User")
[Environment]::SetEnvironmentVariable("PATH",
  "$p;$QT_ROOT\Tools\CMake_64\bin;$QT_ROOT\Tools\$MINGW_VER\bin",
  "User")
```

Configure, build, and install:
```powershell
$env:CC  = "$QT_ROOT\Tools\$MINGW_VER\bin\gcc.exe"
$env:CXX = "$QT_ROOT\Tools\$MINGW_VER\bin\g++.exe"

cmake `
  -S "$QUICKBOX_ROOT" `
  -B "$QUICKBOX_ROOT\build" `
  -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Release `
  "-DCMAKE_PREFIX_PATH=$QT_ROOT\$QT_VER\mingw_64" `
  -DCMAKE_INSTALL_PREFIX="$QUICKBOX_ROOT\install" `
  -DUSE_QT6=ON `
  -DMINGW=ON `
  -DQF_BUILD_QML_PLUGINS=ON `
  -DBUILD_TESTING=OFF

cmake --build "$QUICKBOX_ROOT\build" --parallel
cmake --install "$QUICKBOX_ROOT\build"
```

---

## After Install — Bundle Dependencies

After installing, run `windeployqt` to copy Qt plugins and DLLs, then manually copy runtime libraries:

```powershell
$PG_BIN = "C:\Program Files\PostgreSQL\17\bin"   # Update this
$SSL_DIR = "C:\Program Files\OpenSSL"            # Update this


# Deploy Qt DLLs and plugins
& "$QT_ROOT\$QT_VER\mingw_64\bin\windeployqt.exe" `
  -serialport -multimedia `
  --qmldir "$QUICKBOX_ROOT\install\bin\qml" `
  --qmldir "$QUICKBOX_ROOT\install\bin\reports" `
  "$QUICKBOX_ROOT\install\bin\quickevent.exe"

# Copy MinGW runtime DLLs
Copy-Item `
  "$QT_ROOT\Tools\$MINGW_VER\bin\libstdc++-6.dll", `
  "$QT_ROOT\Tools\$MINGW_VER\bin\libgcc_s_seh-1.dll", `
  "$QT_ROOT\Tools\$MINGW_VER\bin\libwinpthread-1.dll" `
  "$QUICKBOX_ROOT\install\bin\"

# Copy PostgreSQL runtime DLLs
Copy-Item `
  "$PG_BIN\libiconv-2.dll", "$PG_BIN\libintl-9.dll", `
  "$PG_BIN\liblz4.dll", "$PG_BIN\zlib1.dll", "$PG_BIN\libpq.dll" `
  "$QUICKBOX_ROOT\install\bin\"

# Copy OpenSSL runtime DLLs (path may vary by OpenSSL install)
Copy-Item "$SSL_DIR\libssl-3-x64.dll", "$SSL_DIR\libcrypto-3-x64.dll" `
  "$QUICKBOX_ROOT\install\bin\"
```

---

## Verification

Run `install\bin\quickevent.exe` — it should launch without any missing DLL errors. Connect to a local SQLite or PostgreSQL database and load an event to verify Qt SQL works end-to-end.

---

## Notes on Windows ARM

Quickbox currently has no native ARM64 build support. Building x86_64 with MinGW and running via Windows ARM's built-in x64 emulation is the recommended approach and mirrors the CI configuration exactly.

**Critical**: An x86_64 emulated process cannot load ARM64 DLLs. On Windows ARM, `winget` may install ARM64-native versions of PostgreSQL and OpenSSL. Use Chocolatey or download explicit x86-64 installers as described above — otherwise the app will fail to start with missing or incompatible DLL errors.