@echo off
if exist "build_ninja\CMakeCache.txt" del /q /f "build_ninja\CMakeCache.txt"
if exist "build_ninja\CMakeFiles" rd /s /q "build_ninja\CMakeFiles"
call "d:\MSCV\VC\Auxiliary\Build\vcvarsall.bat" x64
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/Users/nayan/vcpkg/scripts/buildsystems/vcpkg.cmake -G Ninja -B build_ninja -S .
