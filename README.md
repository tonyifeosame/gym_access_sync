# Gym Access Sync

## Windows build prerequisites

This project is built with the MSYS2 UCRT C++ toolchain on Windows. The runtime depends on the MSYS2 UCRT shared libraries being available next to the generated executables.

Required runtime DLLs:
- libsqlite3-0.dll
- libstdc++-6.dll
- libgcc_s_seh-1.dll
- libwinpthread-1.dll

If you installed MSYS2, make sure C:\msys64\ucrt64\bin is available on your PATH. The provided build script also copies the required DLLs automatically into the build output folder.

## Build and test

From PowerShell, run:

```powershell
.\build.ps1
```

The script will:
1. Compile the project.
2. Copy the required runtime DLLs into the build output folder.
3. Run the unit tests.
