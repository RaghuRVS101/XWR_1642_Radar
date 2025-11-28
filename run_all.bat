@echo off
start cmd /k "python server.py"
timeout /t 2 >nul
start cmd /k "python radar1.py"
start cmd /k "python radar2.py"
