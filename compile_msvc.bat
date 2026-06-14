@echo off
call "d:\New folder\.vsconfig\VC\Auxiliary\Build\vcvarsall.bat" x64
"C:\VulkanSDK\1.4.341.1\Bin\glslc.exe" -fshader-stage=comp shaders/light_culling.comp -o shaders/light_culling.spv
cmake --build build_ninja
