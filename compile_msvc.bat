@echo off
call "d:\New folder\.vsconfig\VC\Auxiliary\Build\vcvarsall.bat" x64
cmake --build build_ninja
