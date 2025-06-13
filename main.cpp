#include "UnrealEngineLSP.hpp"
#include <iostream>
#include <exception>
#include <functional>

using namespace UnrealEngine;

void printUsage(const char* programName) {
    std::cerr << "Usage: " << programName << " [options]\n";
    std::cerr << "\nOptions:\n";
    std::cerr << "  --project-path <path>    Specify the Unreal project path\n";
    std::cerr << "  --engine-path <path>     Specify the Unreal Engine path (optional)\n";
    std::cerr << "  --interactive, -i        Interactive project selection\n";
    std::cerr << "  --search-path <path>     Path to search for projects (default: current dir)\n";
    std::cerr << "  --list-engines           List all detected Unreal Engine installations\n";
    std::cerr << "  --help, -h               Show this help message\n";
    std::cerr << "  --version, -v            Show version information\n";
    std::cerr << "\nDescription:\n";
    std::cerr << "  Unreal Engine Language Server Protocol (LSP) Server for macOS\n";
    std::cerr << "  Designed for Xcode integration with UE 4.20+ and UE 5.x\n";
    std::cerr << "\nFeatures:\n";
    std::cerr << "  - IntelliSense for Unreal Engine C++ in Xcode\n";
    std::cerr << "  - Auto-completion for Unreal classes and macros\n";
    std::cerr << "  - Code generation (UCLASS, USTRUCT, UFUNCTION)\n";
    std::cerr << "  - Header/Source file synchronization\n";
    std::cerr << "  - Blueprint integration support\n";
    std::cerr << "\nXcode Integration Examples:\n";
    std::cerr << "  " << programName << " --project-path ~/Documents/MyUnrealProject\n";
    std::cerr << "  " << programName << " --interactive\n";
    std::cerr << "  " << programName << " --search-path ~/Documents/UnrealProjects\n";
}

void printVersion() {
    std::cerr << "Unreal Engine LSP Server for macOS v1.0.0\n";
    std::cerr << "Xcode IntelliSense support for Unreal Engine 4.20+ and 5.x\n";
    std::cerr << "Built with C++17 and nlohmann/json\n";
}

// 프로젝트 파일 검색 및 선택 기능
std::string findAndSelectProject(const std::string& searchPath = "") {
    std::vector<std::string> projectFiles;
    std::string basePath = searchPath.empty() ? std::filesystem::current_path().string() : searchPath;
    
    std::cerr << "🔍 Searching for Unreal project files in: " << basePath << std::endl;
    
    // 현재 디렉토리와 하위 디렉토리에서 .uproject 파일 찾기 (최대 3레벨 깊이)
    std::function<void(const std::filesystem::path&, int)> searchProjects;
    searchProjects = [&projectFiles, &searchProjects](const std::filesystem::path& path, int depth) {
        if (depth > 3) return; // 최대 3레벨까지만 검색
        
        try {
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                if (entry.is_regular_file() && entry.path().extension() == ".uproject") {
                    projectFiles.push_back(entry.path().parent_path().string());
                } else if (entry.is_directory() && depth < 3) {
                    // 특정 디렉토리는 제외 (빌드 아티팩트, 숨김 폴더 등)
                    std::string dirName = entry.path().filename().string();
                    if (dirName[0] != '.' &&
                        dirName != "Binaries" &&
                        dirName != "Intermediate" &&
                        dirName != "DerivedDataCache" &&
                        dirName != "node_modules") {
                        searchProjects(entry.path(), depth + 1);
                    }
                }
            }
        } catch (const std::filesystem::filesystem_error&) {
            // 접근 권한 없는 디렉토리 무시
        }
    };
    
    searchProjects(basePath, 0);
    
    if (projectFiles.empty()) {
        std::cerr << "❌ No Unreal project files (.uproject) found" << std::endl;
        return "";
    }
    
    if (projectFiles.size() == 1) {
        std::cerr << "✅ Found Unreal project: " << projectFiles[0] << std::endl;
        return projectFiles[0];
    }
    
    // 여러 프로젝트가 발견된 경우 사용자에게 선택하게 함
    std::cerr << "📋 Found " << projectFiles.size() << " Unreal projects:" << std::endl;
    for (size_t i = 0; i < projectFiles.size(); ++i) {
        // 프로젝트 이름 추출 (.uproject 파일명 기준)
        std::string projectName = "Unknown";
        try {
            for (const auto& entry : std::filesystem::directory_iterator(projectFiles[i])) {
                if (entry.path().extension() == ".uproject") {
                    projectName = entry.path().stem().string();
                    break;
                }
            }
        } catch (...) {}
        
        std::cerr << "  [" << (i + 1) << "] " << projectName << " (" << projectFiles[i] << ")" << std::endl;
    }
    
    std::cerr << "  [0] Cancel" << std::endl;
    std::cerr << "\n📌 Select a project (1-" << projectFiles.size() << "): ";
    
    int choice;
    std::cin >> choice;
    
    if (choice < 1 || choice > static_cast<int>(projectFiles.size())) {
        std::cerr << "❌ Invalid selection or cancelled" << std::endl;
        return "";
    }
    
    std::string selectedProject = projectFiles[choice - 1];
    std::cerr << "✅ Selected project: " << selectedProject << std::endl;
    return selectedProject;
}

// 문자열이 특정 prefix로 시작하는지 확인하는 헬퍼 함수
bool startsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

// 문자열이 특정 suffix로 끝나는지 확인하는 헬퍼 함수
bool endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

int main(int argc, char* argv[]) {
    std::string projectPath;
    std::string enginePath;
    std::string searchPath;
    bool interactive = false;
    bool listEngines = false;
    
    // 명령줄 인자 파싱
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
        else if (arg == "--version" || arg == "-v") {
            printVersion();
            return 0;
        }
        else if (arg == "--project-path" && i + 1 < argc) {
            projectPath = argv[++i];
        }
        else if (arg == "--engine-path" && i + 1 < argc) {
            enginePath = argv[++i];
        }
        else if (arg == "--search-path" && i + 1 < argc) {
            searchPath = argv[++i];
        }
        else if (arg == "--interactive" || arg == "-i") {
            interactive = true;
        }
        else if (arg == "--list-engines") {
            listEngines = true;
        }
        else if (startsWith(arg, "--")) {
            std::cerr << "❌ Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // 시작 메시지
    std::cerr << "🎯 Unreal Engine LSP Server for macOS & Xcode" << std::endl;
    std::cerr << "   IntelliSense Support for UE 4.20+ and UE 5.x" << std::endl;
    std::cerr << std::string(60, '=') << std::endl;
    
    try {
        // 엔진 목록만 표시하고 종료
        if (listEngines) {
            std::cerr << "🔍 Scanning for Unreal Engine installations..." << std::endl;
            
            UnrealEngineDetector detector;
            auto engineVersions = detector.findAllEngineVersions();
            
            if (engineVersions.empty()) {
                std::cerr << "❌ No Unreal Engine installations found on macOS" << std::endl;
                std::cerr << "\n💡 Common macOS installation locations:" << std::endl;
                std::cerr << "   📁 /Users/Shared/Epic Games/" << std::endl;
                std::cerr << "   📁 /Applications/Epic Games/" << std::endl;
                std::cerr << "   📁 ~/Library/Epic Games/" << std::endl;
                std::cerr << "\n🔧 Install Unreal Engine via Epic Games Launcher" << std::endl;
                return 1;
            }
            
            std::cerr << "✅ Found " << engineVersions.size() << " Unreal Engine installation(s):" << std::endl;
            for (const auto& version : engineVersions) {
                std::cerr << "\n🎮 Unreal Engine " << version.toString() << std::endl;
                std::cerr << "   📁 Path: " << version.installPath << std::endl;
                
                // 엔진 유효성 검사
                std::string engineBinary = version.installPath + "/Engine/Binaries";
                if (std::filesystem::exists(engineBinary)) {
                    std::cerr << "   ✅ Status: Ready" << std::endl;
                } else {
                    std::cerr << "   ⚠️  Status: Installation may be incomplete" << std::endl;
                }
            }
            return 0;
        }
        
        // 프로젝트 선택 로직
        if (interactive || projectPath.empty()) {
            if (interactive) {
                std::cerr << "🎯 Interactive project selection mode" << std::endl;
            }
            
            std::string selectedProject = findAndSelectProject(searchPath);
            if (selectedProject.empty()) {
                if (projectPath.empty()) {
                    std::cerr << "❌ No project selected. Use --project-path to specify manually" << std::endl;
                    std::cerr << "   Or use --interactive for project selection" << std::endl;
                    return 1;
                }
            } else {
                projectPath = selectedProject;
            }
        }
        
        // 기존 자동 탐지 로직 (Xcode DerivedData 등)
        if (projectPath.empty()) {
            projectPath = std::filesystem::current_path().string();
        }
        
        // Xcode DerivedData 경로인지 확인하고 올바른 프로젝트 경로 찾기
        if (projectPath.find("DerivedData") != std::string::npos ||
            projectPath.find("Xcode") != std::string::npos) {
            
            std::cerr << "⚠️  Warning: Detected Xcode build directory" << std::endl;
            std::cerr << "   Searching for actual Unreal project..." << std::endl;
            
            std::string foundProject = findAndSelectProject();
            if (!foundProject.empty()) {
                projectPath = foundProject;
            } else {
                std::cerr << "❌ Could not find Unreal project. Please specify --project-path" << std::endl;
                std::cerr << "   Example: " << argv[0] << " --project-path /path/to/your/unreal/project" << std::endl;
                return 1;
            }
        }
        
        std::cerr << "📁 Project path: " << projectPath << std::endl;
        
        // 프로젝트 경로 유효성 검사
        if (!std::filesystem::exists(projectPath)) {
            std::cerr << "❌ Error: Project path does not exist: " << projectPath << std::endl;
            return 1;
        }
        
        // .uproject 파일 확인
        bool hasUProjectFile = false;
        std::string projectName = "Unknown";
        try {
            for (const auto& entry : std::filesystem::directory_iterator(projectPath)) {
                if (entry.path().extension() == ".uproject") {
                    hasUProjectFile = true;
                    projectName = entry.path().stem().string();
                    std::cerr << "🎮 Found Unreal project: " << projectName << " (" << entry.path().filename().string() << ")" << std::endl;
                    break;
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "⚠️  Warning: Cannot scan project directory: " << e.what() << std::endl;
        }
        
        if (!hasUProjectFile) {
            std::cerr << "❌ Error: No .uproject file found in: " << projectPath << std::endl;
            std::cerr << "   Please ensure you're pointing to a valid Unreal Engine project directory" << std::endl;
            std::cerr << "   Use --interactive to search and select a project" << std::endl;
            return 1;
        }
        
        // 엔진 버전 감지
        std::cerr << "🔍 Detecting Unreal Engine installation..." << std::endl;
        
        UnrealEngineDetector detector;
        std::vector<EngineVersion> engineVersions;
        EngineVersion projectEngineVersion{5, 3, 0, "5.3.0", ""};
        
        try {
            engineVersions = detector.findAllEngineVersions();
            
            if (!engineVersions.empty()) {
                std::cerr << "✅ Found " << engineVersions.size() << " Unreal Engine installation(s):" << std::endl;
                
                for (size_t i = 0; i < std::min(engineVersions.size(), size_t(3)); ++i) {
                    const auto& version = engineVersions[i];
                    std::cerr << "   🎮 UE " << version.toString();
                    if (!version.installPath.empty()) {
                        std::cerr << " at " << version.installPath;
                    }
                    std::cerr << std::endl;
                }
                
                if (engineVersions.size() > 3) {
                    std::cerr << "   ... and " << (engineVersions.size() - 3) << " more (use --list-engines to see all)" << std::endl;
                }
                
                // 엔진 경로가 명시적으로 지정되지 않았으면 첫 번째 엔진 사용
                if (enginePath.empty() && !engineVersions.empty()) {
                    enginePath = engineVersions[0].installPath;
                }
            } else {
                std::cerr << "⚠️  Warning: No Unreal Engine installations detected" << std::endl;
                std::cerr << "   LSP server will use default settings (UE 5.3)" << std::endl;
                std::cerr << "   Use --list-engines to see search locations" << std::endl;
            }
            
            // 프로젝트별 엔진 버전 감지
            projectEngineVersion = detector.detectProjectEngineVersion(projectPath);
            std::cerr << "🎯 Project '" << projectName << "' uses UE " << projectEngineVersion.toString() << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "⚠️  Warning: Engine detection failed: " << e.what() << std::endl;
            std::cerr << "   Using default engine version (UE 5.3)" << std::endl;
        }
        
        // LSP 서버 초기화
        std::cerr << "🚀 Initializing LSP server..." << std::endl;
        LSPServer server;
        server.initialize(projectPath, enginePath);
        
        std::cerr << "✅ LSP Server ready for project: " << projectName << std::endl;
        std::cerr << "📡 Listening for LSP messages on stdin..." << std::endl;
        std::cerr << std::string(60, '=') << std::endl;
        
        // LSP 통신 시작
        server.run();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Fatal Error: " << e.what() << std::endl;
        std::cerr << "   Please check your project path and try again" << std::endl;
        std::cerr << "   Use --help for usage information" << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown fatal error occurred" << std::endl;
        return 1;
    }
    
    std::cerr << "👋 LSP Server shutting down..." << std::endl;
    return 0;
}
