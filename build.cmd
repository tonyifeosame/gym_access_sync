@echo off
setlocal
set "ROOT=%~dp0"
set "BUILD_DIR=%ROOT%build"
set "MSYS_BIN=C:\msys64\ucrt64\bin"

if not exist "%MSYS_BIN%\c++.exe" (
  echo Missing MSYS2 UCRT toolchain at %MSYS_BIN%
  exit /b 1
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

copy /Y "%MSYS_BIN%\libsqlite3-0.dll" "%BUILD_DIR%" >nul
copy /Y "%MSYS_BIN%\libstdc++-6.dll" "%BUILD_DIR%" >nul
copy /Y "%MSYS_BIN%\libgcc_s_seh-1.dll" "%BUILD_DIR%" >nul
copy /Y "%MSYS_BIN%\libwinpthread-1.dll" "%BUILD_DIR%" >nul

"%MSYS_BIN%\c++.exe" -std=c++17 -I"%ROOT%" -o "%BUILD_DIR%\unit_tests.exe" "%ROOT%unit_tests.cpp" "%ROOT%database.cpp" "%ROOT%mock_esp32_device.cpp" "%ROOT%api_client.cpp" "%ROOT%fingerprint_service.cpp" "%ROOT%mock_fingerprint_scanner.cpp" "%ROOT%door_controller.cpp" "%ROOT%mock_relay.cpp" "%ROOT%validation.cpp" -lsqlite3 -lws2_32
if errorlevel 1 exit /b 1

"%MSYS_BIN%\c++.exe" -std=c++17 -I"%ROOT%" -o "%BUILD_DIR%\integration_tests.exe" "%ROOT%integration_test.cpp" "%ROOT%database.cpp" "%ROOT%api_client.cpp" "%ROOT%crow_server.cpp" -lsqlite3 -lws2_32
if errorlevel 1 exit /b 1

"%MSYS_BIN%\c++.exe" -std=c++17 -I"%ROOT%" -o "%BUILD_DIR%\gym_access_terminal.exe" "%ROOT%main.cpp" "%ROOT%database.cpp" "%ROOT%api_client.cpp" "%ROOT%crow_server.cpp" "%ROOT%sync.cpp" "%ROOT%validation.cpp" -lsqlite3 -lws2_32
if errorlevel 1 exit /b 1

pushd "%BUILD_DIR%"
unit_tests.exe
if errorlevel 1 exit /b %ERRORLEVEL%
integration_tests.exe
if errorlevel 1 exit /b %ERRORLEVEL%
popd

echo Build complete.
