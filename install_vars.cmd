@echo off
FOR /F "usebackq tokens=3*" %%A IN (`REG QUERY "HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Valve\Steam" /v InstallPath`) DO (
    set "STEAMVR_DIR=%%A %%B\steamapps\common\SteamVR"
)

set "PWD=%cd%"
set "DEVICE_NAME=lodestar"

set "DRIVER_DEST=%STEAMVR_DIR%\drivers\%DEVICE_NAME%"
set "DRIVER_SRC=%PWD%\%DEVICE_NAME%"

set "VRPATHREG_PATH=%STEAMVR_DIR%\bin\win64\vrpathreg.exe"