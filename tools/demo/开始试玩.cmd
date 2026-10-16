@echo off
rem ============================================================
rem  MiniEngine demo launcher (S11.5)
rem
rem  ASCII ONLY on purpose: cmd.exe parses this file with the OEM
rem  code page (936 on this machine), so Chinese text inside a .cmd
rem  is a trap (mojibake, and worse: a stray quote can break parsing).
rem  The Chinese explanation lives in the .txt next to this file.
rem ============================================================

cd /d "%~dp0"
chcp 65001 >nul 2>&1

rem Why the classic console is used on purpose (two reasons):
rem   1) key-up events: hold-to-move works there; ConPTY terminals
rem      (Windows Terminal / VS Code) never deliver key-up;
rem   2) window size: the program tries to grow the window to 100x30
rem      on startup, but ConPTY refuses window-size calls.
if exist "%SystemRoot%\System32\conhost.exe" (
    start /wait "" conhost.exe "MiniEngine.exe"
) else (
    MiniEngine.exe
)

rem Keep this window open, so a crash does not look like "nothing happened".
echo.
echo Demo finished. Press any key to close this window...
pause >nul
