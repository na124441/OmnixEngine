# 🔧 Omnix Engine Installation Guide

This guide provides instructions for building and running the Omnix Engine on Windows.

## 1. Prerequisites

- **Visual Studio 2022**: Install Visual Studio 2022 (the Community Edition is free).
  - Download from: https://visualstudio.microsoft.com/downloads/
  - During installation, you **must** select the **"Desktop development with C++"** workload. This includes the C++ compiler, CMake, and all other necessary tools.

## 2. Building the Project

For the best results, use the **Developer Command Prompt for VS 2022**, which you can find in your Start Menu. This command prompt is pre-configured with the correct paths for the build tools.

### Using Windows Command Prompt (cmd.exe)

Execute the following commands from the project's root directory (`E:\Omnix_Studio_v0.1\Engine`):

```cmd
:: 1. Navigate to the project root
cd /d E:\Omnix_Studio_v0.1\Engine

:: 2. Remove the old build directory to ensure a clean build
if exist build rmdir /s /q build

:: 3. Create a new build directory and navigate into it
mkdir build
cd build

:: 4. Generate the build files. CMake will automatically detect your Visual Studio installation.
cmake ..

:: 5. Build the project in Release mode.
cmake --build . --config Release
```

### Using PowerShell

Execute the following commands from the project's root directory (`E:\Omnix_Studio_v0.1\Engine`):

```powershell
# 1. Navigate to the project root (if you're not already there)
Set-Location E:\Omnix_Studio_v0.1\Engine

# 2. Remove the old build directory to ensure a clean build
if (Test-Path build) {
    Remove-Item -Recurse -Force build
}

# 3. Create a new build directory and navigate into it
mkdir build
cd build

# 4. Generate the build files. CMake will automatically detect your Visual Studio installation.
cmake ..

# 5. Build the project in Release mode.
cmake --build . --config Release
```

This will compile all libraries and executables. The output files will be located in the `E:\Omnix_Studio_v0.1\Engine\build\Release` directory.

## 3. Running the Applications

After a successful build, you can run the following executables.

### Using Windows Command Prompt (cmd.exe)

```cmd
:: Navigate to the directory containing the executables
cd E:\Omnix_Studio_v0.1\Engine\build\Release

:: Run the main engine application
Application.exe

:: Run the serialization test suite
serialization_test.exe

:: Run the ECS sampler
sampler.exe
```

### Using PowerShell

```powershell
# Navigate to the directory containing the executables
cd E:\Omnix_Studio_v0.1\Engine\build\Release

# Run the main engine application
.\Application.exe

# Run the serialization test suite
.\serialization_test.exe

# Run the ECS sampler
.\sampler.exe
```

### Expected Output

-   **Application.exe**: You should see logs indicating the engine is starting up, initializing subsystems, and entering the main game loop.
-   **serialization_test.exe**: This will run a series of tests and should report that all tests passed.
-   **sampler.exe**: This is a simple test or example for the Entity Component System.

## 4. Troubleshooting

-   **"cmake is not recognized"** or **"cl is not recognized"**:
    -   This error means the build tools are not in your system's PATH.
    -   **Solution**: Always use the **"Developer Command Prompt for VS 2022"**. It automatically sets up all necessary paths.
-   **CMake Errors During Generation (`cmake ..`)**:
    -   If you see errors about missing compilers or generators, it likely means the "Desktop development with C++" workload is not installed correctly in Visual Studio.
    -   **Solution**: Open the "Visual Studio Installer", click "Modify" on your Visual Studio 2022 installation, and ensure the C++ workload is checked and installed.
-   **Build Errors (`cmake --build .`)**:
    -   If you encounter errors during the build process after a successful generation, it indicates a problem in the source code.
    -   **Solution**: Review the compiler errors in the console. The errors usually point to the exact file and line number causing the issue.

---

Happy coding with Omnix Engine! 🚀
