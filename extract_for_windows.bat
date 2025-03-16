@echo off
setlocal enabledelayedexpansion
REM Script to extract TSTO Server ELF binary and dependencies from Docker container on Windows

echo Creating extraction directory...
mkdir tsto_extracted 2>nul
cd tsto_extracted

echo Extracting TSTO Server binary from Docker container...
docker cp tsto---simpsons-tapped-out---private-server-tsto-server-1:/app/tsto-server ./tsto-server

echo Finding shared library dependencies...
docker exec tsto---simpsons-tapped-out---private-server-tsto-server-1 bash -c "ldd /app/tsto-server" > dependencies.txt

echo Extracting library information from Docker...
docker exec tsto---simpsons-tapped-out---private-server-tsto-server-1 bash -c "dpkg -l | grep protobuf" > protobuf_version.txt
docker exec tsto---simpsons-tapped-out---private-server-tsto-server-1 bash -c "dpkg -l | grep libev" > libev_version.txt
docker exec tsto---simpsons-tapped-out---private-server-tsto-server-1 bash -c "dpkg -l | grep glog" > glog_version.txt

echo Creating lib directory for dependencies...
mkdir lib 2>nul

echo Extracting required libraries from Docker...
docker exec tsto---simpsons-tapped-out---private-server-tsto-server-1 bash -c "ldd /app/tsto-server | grep '=>' | awk '{print \$3}' | grep -v 'not found'" > lib_paths.txt
for /f "tokens=*" %%a in (lib_paths.txt) do (
    echo Extracting %%a...
    for /f "delims=/" %%b in ("%%a") do set filename=%%~nxb
    docker cp tsto---simpsons-tapped-out---private-server-tsto-server-1:%%a lib\!filename!
)

echo Creating run script for Linux...
(
echo #!/bin/bash
echo # Run TSTO Server with the correct library path
echo 
echo # Set the library path to use our extracted libraries
echo export LD_LIBRARY_PATH="$PWD/lib:$LD_LIBRARY_PATH"
echo 
echo # Run the server
echo ./tsto-server "$@"
) > run_tsto.sh

echo Copying required data files...
docker cp tsto---simpsons-tapped-out---private-server-tsto-server-1:/app/dlc ./dlc
docker cp tsto---simpsons-tapped-out---private-server-tsto-server-1:/app/server-config.json ./server-config.json
docker cp tsto---simpsons-tapped-out---private-server-tsto-server-1:/app/towns ./towns 2>nul
docker cp tsto---simpsons-tapped-out---private-server-tsto-server-1:/app/currencies ./currencies 2>nul
docker cp tsto---simpsons-tapped-out---private-server-tsto-server-1:/app/web ./web

echo Extraction complete!
echo.
echo The ELF binary and all dependencies have been extracted to the tsto_extracted folder.
echo To use on Ubuntu:
echo 1. Copy the entire tsto_extracted folder to your Ubuntu machine
echo 2. Make the scripts executable: chmod +x tsto-server run_tsto.sh
echo 3. Run the server with: ./run_tsto.sh
echo.
echo This should resolve the protobuf symbol error by using the exact same libraries as in Docker.

cd ..
endlocal
