@echo off
cd /d "%~dp0"
if exist "..\.venv\Scripts\python.exe" (
  "..\.venv\Scripts\python.exe" "%~dp0main.py"
) else (
  py -3 "%~dp0main.py"
)
if errorlevel 1 pause
