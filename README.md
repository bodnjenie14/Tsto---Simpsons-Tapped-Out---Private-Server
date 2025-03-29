# TSTO Server - Docker Setup Guide

## Prerequisites

Before you begin, ensure that you have the following tools installed:
- [Docker Desktop](https://www.docker.com/products/docker-desktop) (Windows/macOS users)
- [Docker](https://docs.docker.com/get-docker/) (Linux users)
- [Git](https://git-scm.com/)


## Compilation Methods

### 1. Cross-Compile from Windows using Docker (Recommended)
This guide focuses on using Docker for cross-compilation from Windows, which is the recommended approach. Simply follow the Docker setup instructions above.

### 2. Direct Linux Compilation
If you prefer to compile directly on Linux without Docker, please refer to:
[Linux Build Instructions](Linux_Build_Instructions.md)

This alternative method provides instructions for:
- Cross-compiling from Windows using Visual Studio
- Direct compilation on Linux systems
- Handling dependencies and common issues

Note: While both methods work, **Docker** is recommended as it provides a consistent build environment and handles all dependencies automatically.


## Quick Start for Docker/Windows.

1. **Clone the repository**:
    ```bash
    git clone <repository-url>
    cd <repository-directory>
    ```

2. **Build and start the server**:
    - Build the Docker container without using the cache:
    ```bash
    docker compose build --no-cache
    ```

    - Start the server in the foreground to view the logs:
    ```bash
    docker compose up
    ```

    - Or, start the server in the background:
    ```bash
    docker compose up -d
    ```

## DLC Setup

There are two ways to handle the DLC files (approximately 30GB):

1. **Auto-Linking (Default)**:
   - Place DLC files in the source folder
   - Docker will automatically link and use these files
   - No rebuild needed when updating DLC
   - Saves container size and build time

2. **Docker Copy Method**:
   - Open `Dockerfile.multistage`
   - Uncomment these lines:
   ```dockerfile
   # COPY dlc/ /app/dlc/
   # COPY dlc/dlc/ /app/dlc/dlc/
   ```
   - This will copy DLC into the container
   - Increases container size by ~30GB
   - Requires rebuild when updating DLC

Note: Build time is around 15-20 minutes.

## Directory Structure

```plaintext
/
├── Dockerfile.multistage    # Docker build configuration
├── docker-compose.yml       # Docker compose configuration
├── cmake.txt                # CMake configuration
├── source/                  # Source code
├── deps/                    # Dependencies
├── cmake/                   # CMake files
├── tools/                   # Tools
├── webpanel/                # Web interface files
├── config.json              # Server configuration
└── dlc/                     # Game DLC files (30GB)
```

## Docker Configuration

The server includes specific configuration options for Docker environments:

### Docker Configuration Options

In your `server-config.json` file, you can configure the following Docker-specific settings:

```json
{
  "ServerConfig": {
    "DockerEnabled": true,
    "DockerPort": 8080
  }
}
```

- **DockerEnabled**: Set to `true` to enable Docker mode (default: `false`)
- **DockerPort**: The port mapping for Docker (default: `8080`)

When Docker mode is enabled:
1. The server will use the Docker port for constructing URLs
2. The RTM host URL will include the Docker port
3. All redirected service URLs will include the Docker port
4. Docker configuration will be exposed to clients through the serverData array

### Recommended Settings

For Docker environments, we recommend:
```json
{
  "ServerConfig": {
    "DockerEnabled": true,
    "DockerPort": 8080
  }
}
```

For non-Docker environments, keep the default settings:
```json
{
  "ServerConfig": {
    "DockerEnabled": false,
    "DockerPort": 8080
  }
}
```

## Advanced Configuration

You can also modify the Docker configuration directly in the server startup code **Before compiling**:

1. The Docker configuration is initialized in `source/server/networking/server_startup.cpp`:
```cpp
// Read Docker configuration
bool docker_enabled = utils::configuration::ReadBoolean(CONFIG_SECTION, "DockerEnabled", false);
int docker_port = static_cast<int>(utils::configuration::ReadUnsignedInteger(CONFIG_SECTION, "DockerPort", 8080));
```



## Detailed Setup

### Building the Docker Container

To build the Docker container from scratch without using the cache:

```bash
docker compose build --no-cache
```

This ensures that the latest dependencies and configurations are used.

### Dlc Setup

Dlc files to be placed in the `dlc` folder before building the container.

### Running the Server

After building the container, you can start the server:

- **Run in foreground (view logs)**:
    ```bash
    docker compose up
    ```

- **Run in the background (detached mode)**:
    ```bash
    docker compose up -d
    ```

### Stopping the Server

To stop the server that’s running in the background, use:

```bash
docker compose down
```

## Compiling for Different Ubuntu Versions

The server can be compiled for different Ubuntu versions by modifying the Dockerfile. Here's how to do it:

1. Open `Dockerfile.multistage` in your text editor
2. Change both the build and runtime stages to use your desired Ubuntu version. For example, to use Ubuntu 22.04:

```dockerfile
# Build stage
FROM ubuntu:22.04 as builder

# ... rest of build stage ...

# Runtime stage
FROM ubuntu:22.04

# ... rest of runtime stage ...
```

### Example Ubuntu Versions:
- Ubuntu 22.04 (Jammy Jellyfish): `ubuntu:22.04`
- Ubuntu 23.04 (Lunar Lobster): `ubuntu:23.04`
- Ubuntu 23.10 (Mantic Minotaur): `ubuntu:23.10`
- Ubuntu 24.04 (Noble Numbat): `ubuntu:24.04`

Note: Package names and versions might differ between Ubuntu releases. You may need to adjust the package installation commands in the Dockerfile accordingly.

## Windows Development Workflow

If you're developing on Windows and want to extract the compiled Linux binary:

1. After successfully building and running the Docker container, run:
```batch
extract_for_windows.bat
```

This script will:
- Extract the compiled ELF binary from the Docker container
- Place it in your local directory for deployment or testing
- Preserve all necessary dependencies and configurations

This is particularly useful when:
- Cross-compiling from Windows to Linux
- Testing the server on different Linux environments
- Deploying to a production Linux server

---

## Linux-Specific Setup

For Linux users, the steps are similar to the ones above, with a few variations in the setup.

### Prerequisites for Linux

- Install Docker on your Linux system using the following commands:
    ```bash
    sudo apt update
    sudo apt install apt-transport-https ca-certificates curl software-properties-common
    curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg
    echo "deb [arch=amd64 signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
    sudo apt update
    sudo apt install docker-ce docker-ce-cli containerd.io
    sudo usermod -aG docker $USER
    ```

### Docker Compose on Linux

To install Docker Compose on Linux:

```bash
sudo curl -L "https://github.com/docker/compose/releases/download/1.29.2/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
sudo chmod +x /usr/local/bin/docker-compose
```

### Build and Run Docker Container on Linux

Once Docker is installed, follow the same steps as mentioned earlier to build and run the Docker container:

```bash
git clone <repository-url>
cd <repository-directory>
docker compose build --no-cache
docker compose up -d
```

---



For more details, refer to the [TSTO Server GitHub Repository](<https://github.com/bodnjenie14/Tsto---Simpsons-Tapped-Out---Private-Server/tree/main>) or check out the [documentation](https://jenienbods-organization.gitbook.io/bodnjenie-tsto-private-server).
