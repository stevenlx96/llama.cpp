@echo off
REM Copy llama.cpp libraries to GGUFChat project (with optional NPU/GPU acceleration)

echo =========================================
echo Copying llama.cpp Libraries
echo (with NPU/GPU acceleration support)
echo =========================================
echo.

REM Source directory (official pkg-adb)
set SRC_LIB=..\pkg-adb\llama.cpp\lib

REM Destination directory (GGUFChat jniLibs)
set DEST_LIB=app\src\main\jniLibs\arm64-v8a

REM Create destination directory
if not exist "%DEST_LIB%" mkdir "%DEST_LIB%"

REM Copy core libraries (required)
echo Copying libggml-base.so...
copy /Y "%SRC_LIB%\libggml-base.so" "%DEST_LIB%\" || goto :error

echo Copying libggml-cpu.so...
copy /Y "%SRC_LIB%\libggml-cpu.so" "%DEST_LIB%\" || goto :error

echo Copying libggml.so...
copy /Y "%SRC_LIB%\libggml.so" "%DEST_LIB%\" || goto :error

echo Copying libllama.so...
copy /Y "%SRC_LIB%\libllama.so" "%DEST_LIB%\" || goto :error

REM Optional: Copy OpenMP library if exists
if exist "%SRC_LIB%\libomp.so" (
    echo Copying libomp.so (OpenMP)...
    copy /Y "%SRC_LIB%\libomp.so" "%DEST_LIB%\"
)

REM Optional: Copy OpenCL backend (GPU acceleration)
if exist "%SRC_LIB%\libggml-opencl.so" (
    echo Copying libggml-opencl.so (GPU acceleration)...
    copy /Y "%SRC_LIB%\libggml-opencl.so" "%DEST_LIB%\"
)

REM Optional: Copy Hexagon backend (NPU acceleration)
if exist "%SRC_LIB%\libggml-hexagon.so" (
    echo Copying libggml-hexagon.so (NPU acceleration)...
    copy /Y "%SRC_LIB%\libggml-hexagon.so" "%DEST_LIB%\"
)

REM Optional: Copy Hexagon HTP libraries
for %%f in ("%SRC_LIB%\libggml-htp*.so") do (
    echo Copying %%~nxf (Hexagon HTP)...
    copy /Y "%%f" "%DEST_LIB%\"
)

echo.
echo =========================================
echo All libraries copied successfully!
echo =========================================
echo.
echo Copied to: %DEST_LIB%
echo.
echo Note: Backends (CPU, OpenCL, Hexagon) are loaded
echo       dynamically at runtime via ggml_backend_load_all_from_path()
echo.
echo Next step: gradlew assembleDebug
goto :end

:error
echo.
echo ERROR: Failed to copy libraries!
echo Check if source directory exists: %SRC_LIB%
exit /b 1

:end
