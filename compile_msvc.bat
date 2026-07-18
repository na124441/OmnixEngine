@echo off
taskkill /f /im Application.exe 2>nul
call "d:\MSCV\VC\Auxiliary\Build\vcvarsall.bat" x64
"C:\VulkanSDK\1.4.341.1\Bin\glslc.exe" -fshader-stage=comp shaders/light_culling.comp -o shaders/light_culling.spv
"C:\VulkanSDK\1.4.341.1\Bin\glslc.exe" -fshader-stage=comp shaders/frustum_cull.comp -o shaders/frustum_cull.spv
"C:\VulkanSDK\1.4.341.1\Bin\glslc.exe" -fshader-stage=comp shaders/build_indirect_commands.comp -o shaders/build_indirect_commands.spv
"C:\VulkanSDK\1.4.341.1\Bin\glslc.exe" -fshader-stage=vert shaders/depth_indirect_vert.glsl -o shaders/depth_indirect_vert.spv
"C:\VulkanSDK\1.4.341.1\Bin\glslc.exe" -fshader-stage=vert shaders/gbuffer_indirect_vert.glsl -o shaders/gbuffer_indirect_vert.spv
"C:\VulkanSDK\1.4.341.1\Bin\glslc.exe" -fshader-stage=comp shaders/depth_to_hzb.comp -o shaders/depth_to_hzb.spv
"C:\VulkanSDK\1.4.341.1\Bin\glslc.exe" -fshader-stage=comp shaders/hzb_downsample.comp -o shaders/hzb_downsample.spv
"C:\VulkanSDK\1.4.341.1\Bin\glslc.exe" -fshader-stage=comp shaders/occlusion_cull.comp -o shaders/occlusion_cull.spv
"C:\VulkanSDK\1.4.341.1\Bin\glslc.exe" -fshader-stage=comp shaders/rvg_cull.comp -o shaders/rvg_cull.spv
"C:\VulkanSDK\1.4.341.1\Bin\glslc.exe" -fshader-stage=vert shaders/gbuffer_vert.glsl -o shaders/gbuffer_vert.spv
"C:\VulkanSDK\1.4.341.1\Bin\glslc.exe" -fshader-stage=frag shaders/gbuffer_frag.glsl -o shaders/gbuffer_frag.spv
"C:\VulkanSDK\1.4.341.1\Bin\glslc.exe" -fshader-stage=frag shaders/deferred_lighting.glsl -o shaders/deferred_lighting.spv
"C:\VulkanSDK\1.4.341.1\Bin\glslc.exe" -fshader-stage=frag shaders/ssao_frag.glsl -o shaders/ssao_frag.spv
"C:\VulkanSDK\1.4.341.1\Bin\glslc.exe" -fshader-stage=comp shaders/ssr.comp -o shaders/ssr.spv
"C:\VulkanSDK\1.4.341.1\Bin\glslc.exe" -fshader-stage=comp shaders/exposure.comp -o shaders/exposure.spv
"C:\VulkanSDK\1.4.341.1\Bin\glslc.exe" -fshader-stage=comp shaders/taa.comp -o shaders/taa.spv
"C:\VulkanSDK\1.4.341.1\Bin\glslc.exe" -fshader-stage=frag shaders/dof.frag -o shaders/dof.spv
cmake --build build_ninja


