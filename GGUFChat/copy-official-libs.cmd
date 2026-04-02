@echo off
REM Copy llama.cpp libraries to GGUFChat project (CPU only)

echo =========================================
echo Copying llama.cpp Libraries
echo =========================================
echo.

REM Source directory (official pkg-adb)
set SRC_LIB=..\pkg-adb\llama.cpp\lib

REM Destination directories
set APP_LIB=app\src\main\jniLibs\arm64-v8a
set AAR_LIB=llama-android\src\main\jniLibs\arm64-v8a

REM Create destination directories
if not exist "%APP_LIB%" mkdir "%APP_LIB%"
if not exist "%AAR_LIB%" mkdir "%AAR_LIB%"

REM Copy core libraries (required) to both modules
echo Copying libggml-base.so...
copy /Y "%SRC_LIB%\libggml-base.so" "%APP_LIB%\" || goto :error
copy /Y "%SRC_LIB%\libggml-base.so" "%AAR_LIB%\" || goto :error

echo Copying libggml-cpu.so...
copy /Y "%SRC_LIB%\libggml-cpu.so" "%APP_LIB%\" || goto :error
copy /Y "%SRC_LIB%\libggml-cpu.so" "%AAR_LIB%\" || goto :error

echo Copying libggml.so...
copy /Y "%SRC_LIB%\libggml.so" "%APP_LIB%\" || goto :error
copy /Y "%SRC_LIB%\libggml.so" "%AAR_LIB%\" || goto :error

echo Copying libllama.so...
copy /Y "%SRC_LIB%\libllama.so" "%APP_LIB%\" || goto :error
copy /Y "%SRC_LIB%\libllama.so" "%AAR_LIB%\" || goto :error

REM Optional: Copy OpenMP library if exists
if exist "%SRC_LIB%\libomp.so" (
    echo Copying libomp.so (OpenMP)...
    copy /Y "%SRC_LIB%\libomp.so" "%APP_LIB%\"
    copy /Y "%SRC_LIB%\libomp.so" "%AAR_LIB%\"
)

REM Optional: Copy ONNX Runtime library if exists (for intent recognition)
if exist "%SRC_LIB%\libonnxruntime.so" (
    echo Copying libonnxruntime.so (Intent Recognition)...
    copy /Y "%SRC_LIB%\libonnxruntime.so" "%APP_LIB%\" || goto :error
    copy /Y "%SRC_LIB%\libonnxruntime.so" "%AAR_LIB%\" || goto :error
) else (
    echo NOTE: libonnxruntime.so not found - intent recognition will be disabled
    echo   To enable, download ONNX Runtime Android v1.17.0 and place libonnxruntime.so in:
    echo     %SRC_LIB%\
)

echo.
echo =========================================
echo All libraries copied successfully!
echo =========================================
echo.
echo Copied to:
echo   %APP_LIB%
echo   %AAR_LIB%
echo.
echo Next step: gradlew assembleDebug
goto :end

:error
echo.
echo ERROR: Failed to copy libraries!
echo Check if source directory exists: %SRC_LIB%
exit /b 1

:end
