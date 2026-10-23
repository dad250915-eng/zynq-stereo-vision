@echo off
cd /d "%~dp0"
set "DUMMY_NEW_PY=C:\Users\25680\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
if exist "%DUMMY_NEW_PY%" (
  "%DUMMY_NEW_PY%" "%~dp0main.py"
) else (
  py -3 "%~dp0main.py"
)
if errorlevel 1 pause
