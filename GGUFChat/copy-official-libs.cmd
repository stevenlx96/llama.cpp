@echo off
REM Copy official llama.cpp libraries to GGUFChat project
REM Excludes OpenCL to avoid system library dependencies

echo =========================================
echo Copying Official llama.cpp Libraries
echo =========================================
echo.

REM Source directory (official pkg-adb)
set SRC_LIB=..\pkg-adb\llama.cpp\lib

REM Destination directory (GGUFChat jniLibs)
set DEST_LIB=app\src\main\jniLibs\arm64-v8a

REM Create destination directory
if not exist "%DEST_LIB%" mkdir "%DEST_LIB%"

REM Copy libraries (EXCLUDE libggml-opencl.so!)
echo Copying libggml-base.so...
copy /Y "%SRC_LIB%\libggml-base.so" "%DEST_LIB%\" || goto :error

echo Copying libggml-cpu.so...
copy /Y "%SRC_LIB%\libggml-cpu.so" "%DEST_LIB%\" || goto :error

echo Copying libggml-hexagon.so...
copy /Y "%SRC_LIB%\libggml-hexagon.so" "%DEST_LIB%\" || goto :error

echo Copying libggml-htp-v73.so...
copy /Y "%SRC_LIB%\libggml-htp-v73.so" "%DEST_LIB%\" || goto :error

echo Copying libggml-htp-v75.so...
copy /Y "%SRC_LIB%\libggml-htp-v75.so" "%DEST_LIB%\" || goto :error

echo Copying libggml-htp-v79.so...
copy /Y "%SRC_LIB%\libggml-htp-v79.so" "%DEST_LIB%\" || goto :error

echo Copying libggml-htp-v81.so...
copy /Y "%SRC_LIB%\libggml-htp-v81.so" "%DEST_LIB%\" || goto :error

echo Copying libggml.so...
copy /Y "%SRC_LIB%\libggml.so" "%DEST_LIB%\" || goto :error

echo Copying libllama.so...
copy /Y "%SRC_LIB%\libllama.so" "%DEST_LIB%\" || goto :error

echo.
echo =========================================
echo All libraries copied successfully!
echo =========================================
echo.
echo Copied to: %DEST_LIB%
echo.
echo Next step: gradlew assembleDebug
goto :end

:error
echo.
echo ERROR: Failed to copy libraries!
echo Check if source directory exists: %SRC_LIB%
exit /b 1

:end
