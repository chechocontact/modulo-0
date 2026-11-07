@echo off
rem Vintage Maker - configure, build (Debug + Release) and test on Windows x64.
rem Run from the project root:  scripts\build-windows.bat
setlocal
cd /d "%~dp0.."

cmake -S . -B build -G "Visual Studio 17 2022" -A x64 || goto :error

for %%C in (Debug Release) do (
    echo.
    echo ===== %%C =====
    cmake --build build --config %%C --parallel || goto :error
    ctest --test-dir build -C %%C --output-on-failure || goto :error
)

echo.
echo OK. Plugin: build\VintageMaker_artefacts\Release\VST3\Vintage Maker.vst3
exit /b 0

:error
echo.
echo BUILD OR TESTS FAILED (see messages above)
exit /b 1
