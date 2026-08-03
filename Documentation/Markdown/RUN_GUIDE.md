# 🚀 How to Run Omnix Engine

## 📋 Prerequisites

### Option 1: CMake (Recommended)
- **Windows:** Install [CMake](https://cmake.org/download/) and [Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)
- **Linux:** `sudo apt install cmake g++ build-essential`
- **macOS:** `brew install cmake` or install Xcode Command Line Tools

### Option 2: Direct Compiler
- **Windows:** Install [MinGW-w64](https://sourceforge.net/projects/mingw-w64/) or Visual Studio
- **Linux:** `sudo apt install g++`
- **macOS:** Xcode Command Line Tools (`xcode-select --install`)

## 🏗️ Building the Application

### Method 1: Using CMake (Recommended)

```bash
# Create build directory
mkdir build
cd build

# Generate build files
cmake .. -G "Visual Studio 16 2019"  # Windows
# OR
cmake .. -G "Unix Makefiles"         # Linux/macOS

# Build the project
cmake --build . --config Release

# The executables will be in:
# Windows: build/Release/Application.exe
# Linux/macOS: build/Application
```

### Method 2: Simple Build Script

**Windows:**
```cmd
build_simple.bat
```

**Linux/macOS:**
```bash
chmod +x build_simple.sh
./build_simple.sh
```

### Method 3: Manual Compilation

```bash
# Compile Core library
g++ -c Core/Logger.cpp -I. -std=c++17 -o Core/Logger.o
g++ -c Core/Timer.cpp -I. -std=c++17 -o Core/Timer.o

# Compile ECS library
g++ -c ECS/EntityManager.cpp -I. -std=c++17 -o ECS/EntityManager.o
g++ -c ECS/ComponentManager.cpp -I. -std=c++17 -o ECS/ComponentManager.o
g++ -c ECS/SystemManager.cpp -I. -std=c++17 -o ECS/SystemManager.o
g++ -c ECS/Coordinator.cpp -I. -std=c++17 -o ECS/Coordinator.o
g++ -c ECS/ECS.cpp -I. -std=c++17 -o ECS/ECS.o

# Compile main application
g++ -c Core/Application.cpp -I. -std=c++17 -o Core/Application.o
g++ -c main.cpp -I. -std=c++17 -o main.o

# Link everything together
g++ main.o Core/Application.o Core/Logger.o Core/Timer.o ECS/EntityManager.o ECS/ComponentManager.o ECS/SystemManager.o ECS/Coordinator.o ECS/ECS.o -o Application
```

## 🎯 Running the Application

### Main Application
```bash
# After building with CMake
./build/Application          # Linux/macOS
build\Release\Application.exe  # Windows

# After simple build
./Application                # Linux/macOS
Application.exe              # Windows
```

### Serialization Test
```bash
# Build the test first (if using CMake)
cmake --build . --target serialization_test

# Run the test
./build/serialization_test     # Linux/macOS
build\Release\serialization_test.exe  # Windows
```

### ECS Sampler Test
```bash
# Build the sampler
cmake --build . --target sampler

# Run the sampler
./build/sampler               # Linux/macOS
build\Release\sampler.exe        # Windows
```

## 📊 Expected Output

### Main Application
When you run the main application, you should see:
```
=== Omnix Engine v1.0 – Core bootstrap start ===
Platform: Init (stub)
Sub-systems initialised (stubs for demo)
World: ECS fully initialised (components & systems registered)
BootState: entered
[Game loop running...]
```

The application will automatically transition through states:
1. **BootState** (1 second splash screen)
2. **MainMenuState** (waits for input)
3. **GameplayState** (spawns entities and runs systems)

### Serialization Test
```
╔════════════════════════════════════════╗
║   SERIALIZATION MODULE TEST PROGRAM    ║
║   Testing NormalSerializer/Deserializer║
╚════════════════════════════════════════╝

✓ Component schema registration
✓ Test ECS creation  
✓ Serialization (NormalSerializer)
✓ Deserialization (NormalDeserializer)
✓ Roundtrip serialization
✓ Deterministic verification

╔════════════════════════════════════════╗
║   ✓ ALL TESTS PASSED!                  ║
║   Serialization module is working!     ║
╚════════════════════════════════════════╝
```

## 🔧 Troubleshooting

### Common Issues:

1. **"Compiler not found"**
   - Install a C++ compiler (g++, clang++, or Visual Studio)
   - Make sure it's in your PATH

2. **"CMake not found"**
   - Install CMake from https://cmake.org/download/

3. **Header file errors**
   - Make sure you're running from the project root directory
   - The `-I.` flag includes the current directory

4. **Linker errors**
   - All object files need to be linked together
   - Make sure you compile all required source files

5. **C++17 features not supported**
   - Update your compiler to a newer version
   - GCC 7+, Clang 5+, or Visual Studio 2017+

### Platform-Specific Notes:

**Windows:**
- Use Developer Command Prompt for Visual Studio
- Or make sure MinGW-w64 bin directory is in PATH

**Linux:**
- Install build-essential package: `sudo apt install build-essential`

**macOS:**
- Install Xcode Command Line Tools: `xcode-select --install`

## 🎮 Application Features

The current application demonstrates:
- ✅ ECS entity creation and management
- ✅ Component system with Position and Velocity components
- ✅ Movement system that updates entity positions
- ✅ State machine with Boot, Menu, and Gameplay states
- ✅ Logging system with different severity levels
- ✅ Frame timing and delta time calculation
- ✅ Input simulation (automatic state transitions)

## 📈 Next Steps

After successfully running the application:
1. **Add more components** in the `Components/` directory
2. **Implement new systems** in the `Systems/` directory  
3. **Add serialization** for save/load functionality
4. **Integrate rendering** using the RenderingEngine module
5. **Add real input** using the Input module
6. **Implement physics** using the Physics systems

## 🚀 Quick Start

For the fastest way to get running:

**Windows:**
```cmd
build_simple.bat
Application.exe
```

**Linux/macOS:**
```bash
chmod +x build_simple.sh
./build_simple.sh
./Application
```

Happy coding! 🎉