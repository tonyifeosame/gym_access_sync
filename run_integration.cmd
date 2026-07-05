@echo off
setlocal
cd /d C:\c\gym_access_sync
if exist build rmdir /s /q build
mkdir build
C:\msys64\ucrt64\bin\g++.exe -std=c++17 -I. -o build\integration_tests.exe integration_test.cpp database.cpp api_client.cpp crow_server.cpp -lsqlite3 -lws2_32
copy C:\msys64\ucrt64\bin\libsqlite3-0.dll build\ /Y >nul
copy C:\msys64\ucrt64\bin\libstdc++-6.dll build\ /Y >nul
copy C:\msys64\ucrt64\bin\libgcc_s_seh-1.dll build\ /Y >nul
copy C:\msys64\ucrt64\bin\libwinpthread-1.dll build\ /Y >nul
cd build
integration_tests.exe
exit /b %ERRORLEVEL%
