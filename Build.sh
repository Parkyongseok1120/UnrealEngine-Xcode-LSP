#!/bin/bash

# ============================================================================
# Unreal Engine LSP Server for macOS - Build & Install Script
# ============================================================================

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print colored message
print_message() {
    local color=$1
    local message=$2
    echo -e "${color}${message}${NC}"
}

# Print header
print_header() {
    echo ""
    print_message "$BLUE" "=============================================="
    print_message "$BLUE" "Unreal Engine LSP Server for macOS"
    print_message "$BLUE" "Build & Installation Script"
    print_message "$BLUE" "=============================================="
    echo ""
}

# Check if running on macOS
check_macos() {
    if [[ "$OSTYPE" != "darwin"* ]]; then
        print_message "$RED" "❌ Error: This script is for macOS only!"
        exit 1
    fi
}

# Check dependencies
check_dependencies() {
    print_message "$YELLOW" "🔍 Checking dependencies..."
    
    # Check for CMake
    if ! command -v cmake &> /dev/null; then
        print_message "$RED" "❌ CMake not found!"
        print_message "$YELLOW" "Install with: brew install cmake"
        exit 1
    fi
    
    # Check for C++ compiler
    if ! command -v clang++ &> /dev/null; then
        print_message "$RED" "❌ Clang++ not found!"
        print_message "$YELLOW" "Install Xcode Command Line Tools: xcode-select --install"
        exit 1
    fi
    
    print_message "$GREEN" "✅ All dependencies found!"
}

# Create build directory
setup_build_dir() {
    print_message "$YELLOW" "📁 Setting up build directory..."
    
    if [ -d "build" ]; then
        print_message "$YELLOW" "Cleaning existing build directory..."
        rm -rf build
    fi
    
    mkdir -p build
    cd build
}

# Download json.hpp if not present
download_json_hpp() {
    if [ ! -f "../include/json.hpp" ]; then
        print_message "$YELLOW" "📥 Downloading nlohmann/json.hpp..."
        mkdir -p ../include
        curl -L -o ../include/json.hpp https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
        print_message "$GREEN" "✅ json.hpp downloaded!"
    else
        print_message "$GREEN" "✅ json.hpp already present"
    fi
}

# Create sourcekit-lsp config
create_config() {
    print_message "$YELLOW" "📝 Creating sourcekit-lsp configuration..."
    
    mkdir -p ../config
    cat > ../config/sourcekit-lsp-config.json << 'EOF'
{
  "languages": {
    "cpp": {
      "command": "/usr/local/bin/unreal-lsp-server",
      "args": [],
      "env": {},
      "workspaceSymbolProvider": true,
      "triggerCharacters": [".", ":", "U", "A", "F", "E"]
    },
    "c": {
      "command": "/usr/local/bin/unreal-lsp-server",
      "args": [],
      "env": {},
      "workspaceSymbolProvider": true
    }
  },
  "commands": {
    "unreal.generateUClass": {
      "displayName": "Generate UCLASS",
      "description": "Generate Unreal Engine UCLASS template"
    },
    "unreal.generateBlueprintFunction": {
      "displayName": "Generate Blueprint Function",
      "description": "Convert C++ function to Blueprint callable"
    },
    "unreal.syncHeaderSource": {
      "displayName": "Sync Header ↔ Source",
      "description": "Synchronize header and source files"
    },
    "unreal.analyzeLogs": {
      "displayName": "Analyze Unreal Logs",
      "description": "Analyze project logs for issues"
    },
    "unreal.interpretErrors": {
      "displayName": "Interpret Compile Errors",
      "description": "Analyze and provide solutions for compile errors"
    }
  }
}
EOF
    print_message "$GREEN" "✅ Configuration created!"
}

# Configure project
configure_project() {
    print_message "$YELLOW" "⚙️  Configuring project..."
    
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 \
        -DCMAKE_INSTALL_PREFIX=/usr/local
    
    if [ $? -eq 0 ]; then
        print_message "$GREEN" "✅ Configuration successful!"
    else
        print_message "$RED" "❌ Configuration failed!"
        exit 1
    fi
}

# Build project
build_project() {
    print_message "$YELLOW" "🔨 Building project..."
    
    # Get number of CPU cores
    CORES=$(sysctl -n hw.ncpu)
    
    make -j$CORES
    
    if [ $? -eq 0 ]; then
        print_message "$GREEN" "✅ Build successful!"
    else
        print_message "$RED" "❌ Build failed!"
        exit 1
    fi
}

# Install project
install_project() {
    print_message "$YELLOW" "📦 Installing project (requires sudo)..."
    
    sudo make install
    
    if [ $? -eq 0 ]; then
        print_message "$GREEN" "✅ Installation successful!"
    else
        print_message "$RED" "❌ Installation failed!"
        exit 1
    fi
}

# Verify installation
verify_installation() {
    print_message "$YELLOW" "🔍 Verifying installation..."
    
    if [ -f "/usr/local/bin/unreal-lsp-server" ]; then
        print_message "$GREEN" "✅ LSP server installed at: /usr/local/bin/unreal-lsp-server"
    else
        print_message "$RED" "❌ LSP server not found!"
        exit 1
    fi
    
    if [ -f "$HOME/.config/sourcekit-lsp/config.json" ]; then
        print_message "$GREEN" "✅ Config file installed at: $HOME/.config/sourcekit-lsp/config.json"
    else
        print_message "$YELLOW" "⚠️  Config file not found, creating backup location..."
        mkdir -p "$HOME/.config/sourcekit-lsp"
        cp ../config/sourcekit-lsp-config.json "$HOME/.config/sourcekit-lsp/config.json"
    fi
}

# Print usage instructions
print_usage() {
    echo ""
    print_message "$GREEN" "🎉 Installation Complete!"
    echo ""
    print_message "$BLUE" "Next Steps:"
    echo "  1. Restart Xcode"
    echo "  2. Open your Unreal Engine project"
    echo "  3. Try the following:"
    echo "     - Type 'UC' and press Tab for UCLASS completion"
    echo "     - Type 'AActor::' for member function suggestions"
    echo "     - Type 'UFUNC' for UFUNCTION templates"
    echo ""
    print_message "$BLUE" "To test the server manually:"
    echo "  unreal-lsp-server --list-engines"
    echo "  unreal-lsp-server --project-path /path/to/your/project"
    echo ""
    print_message "$BLUE" "For help:"
    echo "  unreal-lsp-server --help"
    echo ""
}

# Main build process
main() {
    print_header
    check_macos
    check_dependencies
    download_json_hpp
    create_config
    setup_build_dir
    configure_project
    build_project
    install_project
    verify_installation
    print_usage
}

# Run main function
main
