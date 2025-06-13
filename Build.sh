#!/bin/bash

# Direct build script without CMake

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Direct Build Script for Unreal LSP${NC}"
echo -e "${BLUE}========================================${NC}"

# Clean up old build
echo -e "${YELLOW}🧹 Cleaning up old builds...${NC}"
rm -rf build
rm -f unreal-lsp-server
rm -f CMakeLists_temp.txt

# Compile directly
echo -e "${YELLOW}🔨 Compiling directly with clang++...${NC}"

clang++ -std=c++20 -O3 -Wall -Wextra -stdlib=libc++ \
    -I. \
    main.cpp UnrealEngineLSP.cpp \
    -o unreal-lsp-server \
    -pthread

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Compilation successful!${NC}"
else
    echo -e "${RED}❌ Compilation failed!${NC}"
    exit 1
fi

# Test the binary
echo -e "${YELLOW}🔍 Testing binary...${NC}"
./unreal-lsp-server --version

# Install
echo -e "${YELLOW}📦 Installing (requires sudo)...${NC}"
sudo cp unreal-lsp-server /usr/local/bin/
sudo chmod +x /usr/local/bin/unreal-lsp-server

# Install config
echo -e "${YELLOW}📝 Installing config file...${NC}"
mkdir -p ~/.config/sourcekit-lsp
cp sourcekit-lsp.json ~/.config/sourcekit-lsp/config.json

# Verify
echo -e "${YELLOW}✅ Verifying installation...${NC}"
which unreal-lsp-server
unreal-lsp-server --version

echo -e "${GREEN}🎉 Installation complete!${NC}"
echo -e "${BLUE}Next steps:${NC}"
echo "1. Restart Xcode completely"
echo "2. Open your Unreal Engine project"
echo "3. Test auto-completion with 'UC' → Tab"
