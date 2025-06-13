
# Unreal Engine Xcode LSP Server for macOS & Xcode

🎮 Full IntelliSense support for Unreal Engine C++ development in Xcode

## Features
- ✅ Auto-completion for Unreal Engine macros (UCLASS, UFUNCTION, UPROPERTY, etc.)
- ✅ IntelliSense for Unreal Engine classes and functions
- ✅ Code generation templates for common Unreal patterns
- ✅ Version-aware support for UE 4.20+ and UE 5.x
- ✅ Header/Source synchronization
- ✅ Blueprint integration support
- ✅ Compile error interpretation with solutions
- ✅ Log analysis for performance and memory issues

## Requirements
- macOS 10.15 or later
- Xcode 12.0 or later
- CMake 3.20 or later
- Unreal Engine 4.20+ or 5.x installed

## Quick Start
Clone the repository:

```bash
git clone https://github.com/Parkyongseok1120/UnrealEngine-Xcode-LSP
cd UnrealEngine-Xcode-LSP
````

Make the build script executable:

```bash
chmod +x build.sh
```

Build and install:

```bash
./build.sh
```

Restart Xcode

Open your Unreal Engine project and start coding!

## Usage Examples

* Auto-completion
* Type `UC` and press Tab → Complete UCLASS template
* Type `AActor::` → See all AActor methods
* Type `UFUNC` → Generate UFUNCTION template
* Type `UPROP` → Generate UPROPERTY template

## Commands

The LSP server provides several commands:

* Generate UCLASS: Creates a complete class template
* Generate Blueprint Function: Converts C++ function to Blueprint-callable
* Sync Header ↔ Source: Synchronizes declarations and implementations
* Analyze Logs: Analyzes Unreal logs for issues
* Interpret Errors: Provides solutions for compile errors

### Common Commands

```bash
# List all detected Unreal Engine installations
UnrealEngine-Xcode-LSP --list-engines

# Run with a specific project
UnrealEngine-Xcode-LSP --project-path /path/to/your/UnrealProject

# Interactive project selection
UnrealEngine-Xcode-LSP --interactive

# Show help
UnrealEngine-Xcode-LSP --help
```

## Troubleshooting

**LSP server not working in Xcode**

* Make sure Xcode is completely closed before running `build.sh`
* Check that the config file exists:

```bash
ls ~/.config/sourcekit-lsp/config.json
```

* Verify the installation:

```bash
which UnrealEngine-Xcode-LSP
```

**No auto-completion**

* Make sure you're editing a `.cpp` or `.h` file
* Try restarting Xcode
* Check the LSP server logs:

```bash
UnrealEngine-Xcode-LSP --project-path /your/project 2> lsp.log
```

**Engine not detected**

* Check common installation paths:

  * `/Users/Shared/Epic Games/`
  * `/Applications/Epic Games/`
  * `~/Library/Epic Games/`
* Set the `UE_ROOT` environment variable:

```bash
export UE_ROOT="/path/to/UnrealEngine"
```

## Advanced Configuration

**Custom Engine Path**

```bash
unreal-lsp-server --project-path /your/project --engine-path /custom/engine/path
```

**Environment Variables**

* `UE_ROOT`: Primary Unreal Engine installation
* `UE5_ROOT`: Unreal Engine 5 installation
* `UE4_ROOT`: Unreal Engine 4 installation

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

## License

This project is licensed under the MIT License.

## Acknowledgments

* Built with [nlohmann/json](https://github.com/nlohmann/json)
* Designed for seamless Xcode integration
* Inspired by the Unreal Engine community

