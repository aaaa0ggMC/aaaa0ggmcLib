#!/bin/bash

# Get the current operating system name
OS="$(uname -s)"

echo "Detecting operating system..."

case "${OS}" in
    Linux*)
        echo "Arch Linux environment detected."
        # sudo privileges are required on Arch Linux
        if command -v pacman > /dev/null; then
            echo "Installing dependencies via pacman for Arch Linux..."
            sudo pacman -S --needed glm rapidjson tomlplusplus
        else
            echo "Error: pacman package manager not found. Please ensure you are on Arch Linux."
            exit 1
        fi
        ;;
        
    MINGW*|MSYS*|UCRT64*)
        echo "MSYS2 (Windows) environment detected."
        # sudo is not required in MSYS2. Installing dependencies for the UCRT64 architecture directly.
        echo "Installing UCRT64 dependencies via pacman..."
        pacman -S --needed mingw-w64-ucrt-x86_64-glm \
                           mingw-w64-ucrt-x86_64-rapidjson \
                           mingw-w64-ucrt-x86_64-tomlplusplus
        ;;
        
    *)
        echo "Unsupported or unknown operating system: ${OS}"
        exit 1
        ;;
esac

echo "======================================"
echo "Dependencies installed successfully!"
echo "If you are in the MSYS2 UCRT64 terminal, please run the following commands to build the project:"
echo "  xmake f -p mingw -c"
echo "  xmake"
echo "======================================"