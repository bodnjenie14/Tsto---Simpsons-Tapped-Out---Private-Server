# Building TSTO Server for Linux

## Method 1: Cross-Compile from Windows (Recommended)

### Prerequisites
1. Visual Studio with C++ development workload installed
2. Linux development tools in Visual Studio
3. WSL (Windows Subsystem for Linux) or remote Linux machine

### Steps

1. **Prepare Windows Environment**
   ```bash
   # Run these scripts from the project root
   .\generate_vs_linux.bat
   .\setup_linux_build.bat
   ```

2. **Configure Linux Environment**
   ```bash
   # On your Linux machine/WSL, run:
   ./check_headers.sh     # Verifies required headers
   ./setup_linux_deps.sh  # Installs dependencies
   ./build_evpp.sh       # Builds the evpp library
   ```

3. **Configure Visual Studio**
   1. Open Visual Studio
   2. Go to Tools > Options > Cross Platform
   3. Add your Linux connection (either WSL or remote machine)
   4. Select the Linux platform in your solution configuration

4. **Build the Project**
   - Select "Linux" as the target platform
   - Build the solution in Visual Studio

## Method 2: Direct Build on Linux

### Prerequisites
```bash
# Install required packages
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    libprotobuf-dev \
    protobuf-compiler \
    libssl-dev \
    zlib1g-dev
```

### Build Steps
1. **Clone the Repository**
   ```bash
   git clone <repository-url>
   cd Tsto---Simpsons-Tapped-Out---Private-Server
   ```

2. **Generate Build Files**
   ```bash
   mkdir build
   cd build
   cmake ..
   ```

3. **Build the Project**
   ```bash
   make -j$(nproc)
   ```

## Common Issues and Solutions

1. **Missing Headers**
   - If you encounter "Cannot open include file: 'netdb.h'", ensure you have the proper Linux development headers installed
   - The compatibility layer (win_linux_compat.h) should handle most Windows/Linux differences

2. **Library Issues**
   - Missing libevpp.a: Run `build_evpp.sh` script
   - Other missing libraries: Check `setup_linux_deps.sh` output

3. **Compilation Errors**
   - Platform-specific code issues are handled by platform_config.h
   - Check linux_compat.hpp for proper header inclusions

## Notes
- All helper scripts contain proper error checking and dependency verification
- The build system automatically handles platform-specific differences through compatibility layers
