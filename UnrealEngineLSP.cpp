#include "UnrealEngineLSP.hpp"

namespace UnrealEngine {

// Helper function for C++17 compatibility
static bool endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// =============================================================================
// UnrealEngineDetector 구현
// =============================================================================

UnrealEngineDetector::UnrealEngineDetector() {
    // macOS 전용 - Epic Games Launcher 기본 설치 위치
    commonInstallPaths_ = {
        "/Users/Shared/Epic Games",           // 공유 설치 (권장)
        "/Applications/Epic Games",           // 애플리케이션 폴더
        "/Applications/UnrealEngine",         // 직접 설치
        "/Applications/UE_5.0",              // 버전별 설치
        "/Applications/UE_5.1",
        "/Applications/UE_5.2",
        "/Applications/UE_5.3",
        "/Applications/UE_5.4",
        "/Applications/UE_5.5"
    };
    
    // 사용자별 설치 경로 추가
    if (const char* home = getenv("HOME")) {
        std::string homeStr(home);
        
        // macOS 사용자별 경로
        commonInstallPaths_.push_back(homeStr + "/Library/Epic Games");
        commonInstallPaths_.push_back(homeStr + "/Epic Games");
        commonInstallPaths_.push_back(homeStr + "/UnrealEngine");
        commonInstallPaths_.push_back(homeStr + "/Applications/Epic Games");
        commonInstallPaths_.push_back(homeStr + "/Documents/Epic Games");
        commonInstallPaths_.push_back(homeStr + "/Documents/UnrealEngine");
        
        // 버전별 경로도 추가
        for (int major = 5; major <= 5; ++major) {
            for (int minor = 0; minor <= 5; ++minor) {
                commonInstallPaths_.push_back(homeStr + "/UnrealEngine/UE_" +
                    std::to_string(major) + "." + std::to_string(minor));
            }
        }
    }
}

std::vector<EngineVersion> UnrealEngineDetector::findAllEngineVersions() {
    std::vector<EngineVersion> versions;
    
    for (const auto& basePath : commonInstallPaths_) {
        if (!fs::exists(basePath)) continue;
        
        try {
            // 직접 경로가 엔진인지 확인
            auto version = detectEngineVersion(basePath);
            if (version.major > 0) {
                versions.push_back(version);
                continue;
            }
            
            // 하위 디렉토리 검색
            for (const auto& entry : fs::directory_iterator(basePath)) {
                if (entry.is_directory()) {
                    try {
                        auto version = detectEngineVersion(entry.path().string());
                        if (version.major > 0) {
                            versions.push_back(version);
                        }
                    } catch (const std::exception&) {
                        // 개별 디렉토리 처리 실패 시 계속 진행
                        continue;
                    }
                }
            }
        } catch (const fs::filesystem_error&) {
            // 디렉토리 접근 실패 시 다음 경로로 진행
            continue;
        }
    }
    
    // 환경변수에서도 찾기
    if (const char* ueRoot = getenv("UE_ROOT")) {
        try {
            auto version = detectEngineVersion(ueRoot);
            if (version.major > 0) {
                versions.push_back(version);
            }
        } catch (const std::exception&) {
            // 환경변수 경로 처리 실패 시 무시
        }
    }
    
    // UE4_ROOT, UE5_ROOT 등도 확인
    for (const char* envVar : {"UE4_ROOT", "UE5_ROOT", "UNREAL_ENGINE_ROOT"}) {
        if (const char* path = getenv(envVar)) {
            try {
                auto version = detectEngineVersion(path);
                if (version.major > 0) {
                    versions.push_back(version);
                }
            } catch (const std::exception&) {
                continue;
            }
        }
    }
    
    // 중복 제거 및 정렬 (최신 버전 우선 - 내림차순)
    std::sort(versions.begin(), versions.end(),
        [](const EngineVersion& a, const EngineVersion& b) {
            return b < a;  // 내림차순 정렬 (큰 것부터)
        });
    
    versions.erase(std::unique(versions.begin(), versions.end(),
        [](const EngineVersion& a, const EngineVersion& b) {
            return a == b;
        }), versions.end());
    
    return versions;
}

EngineVersion UnrealEngineDetector::detectProjectEngineVersion(const std::string& projectPath) {
    try {
        for (const auto& entry : fs::directory_iterator(projectPath)) {
            if (entry.path().extension() == ".uproject") {
                std::ifstream file(entry.path());
                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
                
                try {
                    json projectData = json::parse(content);
                    if (projectData.find("EngineAssociation") != projectData.end()) {
                        std::string engineAssoc = projectData["EngineAssociation"];
                        return parseEngineAssociation(engineAssoc);
                    }
                } catch (...) {
                    // JSON 파싱 실패 시 계속 진행
                }
            }
        }
    } catch (const fs::filesystem_error&) {
        // 디렉토리 접근 실패
    }
    
    // 기본값으로 최신 버전 반환
    auto versions = findAllEngineVersions();
    return versions.empty() ? EngineVersion{5, 3, 0, "5.3.0", ""} : versions[0];
}

EngineVersion UnrealEngineDetector::detectEngineVersion(const std::string& enginePath) {
    EngineVersion version{0, 0, 0, "", enginePath};
    
    // Engine 폴더가 있는지 먼저 확인
    if (!fs::exists(enginePath + "/Engine")) {
        return version;
    }
    
    // Build.version 파일에서 버전 읽기
    std::string buildVersionPath = enginePath + "/Engine/Build/Build.version";
    if (fs::exists(buildVersionPath)) {
        std::ifstream file(buildVersionPath);
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        
        try {
            json buildData = json::parse(content);
            version.major = buildData.value("MajorVersion", 0);
            version.minor = buildData.value("MinorVersion", 0);
            version.patch = buildData.value("PatchVersion", 0);
            version.fullVersion = version.toString();
            version.installPath = enginePath;
            return version;
        } catch (...) {
            // 파싱 실패 시 경로에서 버전 추출 시도
        }
    }
    
    // 경로에서 버전 추출
    std::regex versionPattern(R"((?:UE[_-]?|UnrealEngine[_-]?)(\d+)\.(\d+)(?:\.(\d+))?)",
                             std::regex_constants::icase);
    std::smatch match;
    
    std::string pathStr = enginePath;
    if (std::regex_search(pathStr, match, versionPattern)) {
        version.major = std::stoi(match[1].str());
        version.minor = std::stoi(match[2].str());
        version.patch = match[3].matched ? std::stoi(match[3].str()) : 0;
        version.fullVersion = version.toString();
        version.installPath = enginePath;
    }
    
    return version;
}

EngineVersion UnrealEngineDetector::parseEngineAssociation(const std::string& engineAssoc) {
    std::regex versionPattern(R"((\d+)\.(\d+)(?:\.(\d+))?)");
    std::smatch match;
    
    if (std::regex_search(engineAssoc, match, versionPattern)) {
        EngineVersion version;
        version.major = std::stoi(match[1].str());
        version.minor = std::stoi(match[2].str());
        version.patch = match[3].matched ? std::stoi(match[3].str()) : 0;
        version.fullVersion = version.toString();
        
        // 해당 버전의 설치 경로 찾기
        auto allVersions = findAllEngineVersions();
        for (const auto& v : allVersions) {
            if (v.major == version.major && v.minor == version.minor) {
                version.installPath = v.installPath;
                break;
            }
        }
        
        return version;
    }
    
    return {5, 3, 0, "5.3.0", ""};
}

// =============================================================================
// VersionSpecificAPI 구현
// =============================================================================

VersionSpecificAPI::VersionSpecificAPI() {
    initializeAPIDatabase();
}

void VersionSpecificAPI::initializeAPIDatabase() {
    // UE 4.27 API
    apiDatabase_["4.27"] = {
        {"classes", {
            {"AActor", {
                {"methods", {"BeginPlay", "EndPlay", "Tick", "GetActorLocation", "SetActorLocation",
                           "GetWorld", "Destroy", "GetComponents", "GetRootComponent"}}
            }},
            {"APawn", {
                {"methods", {"PossessedBy", "UnPossessed", "GetController", "SetupPlayerInputComponent",
                           "GetMovementComponent", "AddMovementInput", "AddControllerYawInput"}}
            }},
            {"ACharacter", {
                {"methods", {"Jump", "StopJumping", "CanJump", "GetCharacterMovement", "LaunchCharacter"}}
            }},
            {"UObject", {
                {"methods", {"GetName", "GetClass", "IsA", "GetOuter", "GetWorld", "ConditionalBeginDestroy"}}
            }},
            {"UActorComponent", {
                {"methods", {"BeginPlay", "EndPlay", "TickComponent", "Activate", "Deactivate", "IsActive"}}
            }}
        }},
        {"macros", {
            {"UCLASS", {
                {"template", "UCLASS(BlueprintType, Blueprintable)\nclass GAME_API AClassName : public AActor\n{\n\tGENERATED_UCLASS_BODY()\n\npublic:\n\tvirtual void BeginPlay() override;\n\tvirtual void Tick(float DeltaTime) override;\n};"}
            }},
            {"USTRUCT", {
                {"template", "USTRUCT(BlueprintType)\nstruct FStructName\n{\n\tGENERATED_USTRUCT_BODY()\n\n\tUPROPERTY(EditAnywhere, BlueprintReadWrite)\n\tint32 Value;\n};"}
            }},
            {"UFUNCTION", {
                {"template", "UFUNCTION(BlueprintCallable, Category = \"Gameplay\")\nvoid FunctionName();"}
            }},
            {"UPROPERTY", {
                {"template", "UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = \"Properties\")\nfloat PropertyName;"}
            }}
        }},
        {"includePaths", {
            "Engine/Source/Runtime/Core/Public",
            "Engine/Source/Runtime/CoreUObject/Public",
            "Engine/Source/Runtime/Engine/Public"
        }}
    };
    
    // UE 5.0+ API
    apiDatabase_["5.0"] = {
        {"classes", {
            {"AActor", {
                {"methods", {"BeginPlay", "EndPlay", "Tick", "GetActorLocation", "SetActorLocation",
                           "GetWorld", "GetActorTransform", "SetActorTransform", "Destroy",
                           "GetComponents", "GetRootComponent", "FindComponentByClass"}}
            }},
            {"APawn", {
                {"methods", {"PossessedBy", "UnPossessed", "GetController", "SetupPlayerInputComponent",
                           "AddMovementInput", "GetMovementComponent", "AddControllerYawInput",
                           "AddControllerPitchInput"}}
            }},
            {"ACharacter", {
                {"methods", {"Jump", "StopJumping", "CanJump", "GetCharacterMovement", "LaunchCharacter",
                           "Crouch", "UnCrouch", "CanCrouch"}}
            }},
            {"UObject", {
                {"methods", {"GetName", "GetClass", "IsA", "GetOuter", "GetWorld", "GetTypedOuter",
                           "ConditionalBeginDestroy", "MarkAsGarbage"}}
            }},
            {"UActorComponent", {
                {"methods", {"BeginPlay", "EndPlay", "TickComponent", "Activate", "Deactivate",
                           "IsActive", "RegisterComponent", "UnregisterComponent"}}
            }}
        }},
        {"macros", {
            {"UCLASS", {
                {"template", "UCLASS(BlueprintType, Blueprintable)\nclass GAME_API AClassName : public AActor\n{\n\tGENERATED_BODY()\n\npublic:\n\tAClassName();\n\nprotected:\n\tvirtual void BeginPlay() override;\n\npublic:\n\tvirtual void Tick(float DeltaTime) override;\n};"}
            }},
            {"USTRUCT", {
                {"template", "USTRUCT(BlueprintType)\nstruct FStructName\n{\n\tGENERATED_BODY()\n\n\tUPROPERTY(EditAnywhere, BlueprintReadWrite)\n\tint32 Value = 0;\n};"}
            }},
            {"UFUNCTION", {
                {"template", "UFUNCTION(BlueprintCallable, Category = \"Gameplay\")\nvoid FunctionName();"}
            }},
            {"UPROPERTY", {
                {"template", "UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = \"Properties\")\nfloat PropertyName = 0.0f;"}
            }},
            {"UENUM", {
                {"template", "UENUM(BlueprintType)\nenum class EEnumName : uint8\n{\n\tNone UMETA(DisplayName = \"None\"),\n\tFirst UMETA(DisplayName = \"First\"),\n\tSecond UMETA(DisplayName = \"Second\")\n};"}
            }}
        }},
        {"includePaths", {
            "Engine/Source/Runtime/Core/Public",
            "Engine/Source/Runtime/CoreUObject/Public",
            "Engine/Source/Runtime/Engine/Public",
            "Engine/Source/Runtime/Engine/Classes"
        }}
    };
    
    // UE 5.1+ 추가 기능
    apiDatabase_["5.1"] = apiDatabase_["5.0"];
    apiDatabase_["5.1"]["classes"]["AActor"]["methods"].push_back("GetActorNameOrLabel");
    apiDatabase_["5.1"]["classes"]["AActor"]["methods"].push_back("SetActorLabel");
    
    // UE 5.2+ 추가 기능
    apiDatabase_["5.2"] = apiDatabase_["5.1"];
    apiDatabase_["5.2"]["includePaths"].push_back("Engine/Source/Runtime/UMG/Public");
    
    // UE 5.3+ 추가 기능
    apiDatabase_["5.3"] = apiDatabase_["5.2"];
    apiDatabase_["5.3"]["classes"]["AActor"]["methods"].push_back("GetActorGuid");
    
    // UE 5.4+ 추가 기능
    apiDatabase_["5.4"] = apiDatabase_["5.3"];
    
    // UE 5.5+ 추가 기능
    apiDatabase_["5.5"] = apiDatabase_["5.4"];
}

std::vector<std::string> VersionSpecificAPI::getClassMethods(const std::string& className, const EngineVersion& version) {
    std::string versionKey = getVersionKey(version);
    
    if (apiDatabase_.find(versionKey) != apiDatabase_.end() &&
        apiDatabase_[versionKey].find("classes") != apiDatabase_[versionKey].end() &&
        apiDatabase_[versionKey]["classes"].find(className) != apiDatabase_[versionKey]["classes"].end()) {
        
        auto methods = apiDatabase_[versionKey]["classes"][className]["methods"];
        return methods.get<std::vector<std::string>>();
    }
    
    return getDefaultClassMethods(className);
}

std::string VersionSpecificAPI::getMacroTemplate(const std::string& macroName, const EngineVersion& version) {
    std::string versionKey = getVersionKey(version);
    
    if (apiDatabase_.find(versionKey) != apiDatabase_.end() &&
        apiDatabase_[versionKey].find("macros") != apiDatabase_[versionKey].end() &&
        apiDatabase_[versionKey]["macros"].find(macroName) != apiDatabase_[versionKey]["macros"].end()) {
        
        return apiDatabase_[versionKey]["macros"][macroName]["template"];
    }
    
    return getDefaultMacroTemplate(macroName, version);
}

std::vector<std::string> VersionSpecificAPI::getIncludePaths(const EngineVersion& version) {
    std::string versionKey = getVersionKey(version);
    
    if (apiDatabase_.find(versionKey) != apiDatabase_.end() &&
        apiDatabase_[versionKey].find("includePaths") != apiDatabase_[versionKey].end()) {
        
        auto paths = apiDatabase_[versionKey]["includePaths"];
        return paths.get<std::vector<std::string>>();
    }
    
    return getDefaultIncludePaths(version);
}

std::string VersionSpecificAPI::getVersionKey(const EngineVersion& version) {
    if (version.isUE4()) {
        return "4.27";
    } else if (version.major == 5) {
        if (version.minor >= 5) return "5.5";
        if (version.minor >= 4) return "5.4";
        if (version.minor >= 3) return "5.3";
        if (version.minor >= 2) return "5.2";
        if (version.minor >= 1) return "5.1";
        return "5.0";
    }
    
    return "5.3"; // 기본값
}

std::vector<std::string> VersionSpecificAPI::getDefaultClassMethods(const std::string& className) {
    if (className == "AActor") {
        return {"BeginPlay", "EndPlay", "Tick", "GetActorLocation", "SetActorLocation"};
    } else if (className == "UObject") {
        return {"GetName", "GetClass", "IsA"};
    } else if (className == "APawn") {
        return {"PossessedBy", "UnPossessed", "GetController"};
    } else if (className == "ACharacter") {
        return {"Jump", "StopJumping", "GetCharacterMovement"};
    }
    
    return {};
}

std::string VersionSpecificAPI::getDefaultMacroTemplate(const std::string& macroName, const EngineVersion& version) {
    if (macroName == "UCLASS") {
        if (version.isUE4()) {
            return "UCLASS(BlueprintType, Blueprintable)\nclass GAME_API AClassName : public AActor\n{\n\tGENERATED_UCLASS_BODY()\n\n};";
        } else {
            return "UCLASS(BlueprintType, Blueprintable)\nclass GAME_API AClassName : public AActor\n{\n\tGENERATED_BODY()\n\npublic:\n\tAClassName();\n\n};";
        }
    } else if (macroName == "USTRUCT") {
        if (version.isUE4()) {
            return "USTRUCT(BlueprintType)\nstruct FStructName\n{\n\tGENERATED_USTRUCT_BODY()\n};";
        } else {
            return "USTRUCT(BlueprintType)\nstruct FStructName\n{\n\tGENERATED_BODY()\n};";
        }
    } else if (macroName == "UFUNCTION") {
        return "UFUNCTION(BlueprintCallable, Category = \"Gameplay\")\nvoid FunctionName();";
    } else if (macroName == "UPROPERTY") {
        return "UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = \"Properties\")\nfloat PropertyName;";
    }
    
    return "";
}

std::vector<std::string> VersionSpecificAPI::getDefaultIncludePaths(const EngineVersion& version) {
    std::vector<std::string> paths = {
        "Engine/Source/Runtime/Core/Public",
        "Engine/Source/Runtime/CoreUObject/Public",
        "Engine/Source/Runtime/Engine/Public"
    };
    
    if (version.isUE5()) {
        paths.push_back("Engine/Source/Runtime/Engine/Classes");
        if (version.minor >= 2) {
            paths.push_back("Engine/Source/Runtime/UMG/Public");
        }
    }
    
    return paths;
}

// =============================================================================
// DynamicHeaderScanner 구현
// =============================================================================

DynamicHeaderScanner::DynamicHeaderScanner(const EngineVersion& version) : engineVersion_(version) {
    enginePath_ = version.installPath;
}

void DynamicHeaderScanner::scanEngineHeaders() {
    if (enginePath_.empty()) return;
    
    auto includePaths = getEnginePaths();
    
    for (const auto& includePath : includePaths) {
        std::string fullPath = enginePath_ + "/" + includePath;
        if (fs::exists(fullPath)) {
            scanDirectory(fullPath);
        }
    }
}

std::vector<std::string> DynamicHeaderScanner::getClassMethods(const std::string& className) {
    if (scannedClasses_.find(className) != scannedClasses_.end()) {
        return scannedClasses_[className];
    }
    
    return {};
}

std::vector<std::string> DynamicHeaderScanner::getEnginePaths() {
    VersionSpecificAPI api;
    return api.getIncludePaths(engineVersion_);
}

void DynamicHeaderScanner::scanDirectory(const std::string& dirPath) {
    try {
        for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".h") {
                scanHeaderFile(entry.path().string());
            }
        }
    } catch (const fs::filesystem_error&) {
        // 권한 오류 등 무시
    }
}

void DynamicHeaderScanner::scanHeaderFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return;
    
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    
    // 클래스 선언 찾기
    std::regex classPattern(R"(class\s+\w+_API\s+(\w+)\s*:\s*public)");
    std::smatch match;
    
    auto searchStart = content.cbegin();
    while (std::regex_search(searchStart, content.cend(), match, classPattern)) {
        std::string className = match[1].str();
        
        auto methods = extractClassMethods(content, className);
        if (!methods.empty()) {
            scannedClasses_[className] = methods;
        }
        
        searchStart = match.suffix().first;
    }
}

std::vector<std::string> DynamicHeaderScanner::extractClassMethods(const std::string& content, const std::string& className) {
    std::vector<std::string> methods;
    
    std::regex methodPattern(R"(\s+(\w+)\s*\([^)]*\)\s*(?:const)?\s*(?:override)?\s*;)");
    std::smatch match;
    
    auto searchStart = content.cbegin();
    while (std::regex_search(searchStart, content.cend(), match, methodPattern)) {
        std::string methodName = match[1].str();
        
        if (methodName != className &&
            methodName[0] != '~' &&
            methodName != "operator" &&
            std::isupper(methodName[0])) {
            
            methods.push_back(methodName);
        }
        
        searchStart = match.suffix().first;
    }
    
    return methods;
}

// =============================================================================
// FunctionInfo 구현
// =============================================================================

std::string FunctionInfo::generateBlueprintWrapper() const {
    std::ostringstream ss;
    
    ss << "UFUNCTION(BlueprintCallable, Category = \"Gameplay\")\n";
    ss << returnType << " Blueprint_" << name << "(";
    
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << parameters[i];
    }
    
    ss << ")\n{\n";
    ss << "\t// Blueprint wrapper for " << name << "\n";
    ss << "\treturn " << name << "(";
    
    // 파라미터 이름만 추출하여 함수 호출
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (i > 0) ss << ", ";
        // 간단한 파라미터 이름 추출 (실제로는 더 정교한 파싱 필요)
        size_t lastSpace = parameters[i].find_last_of(' ');
        if (lastSpace != std::string::npos) {
            ss << parameters[i].substr(lastSpace + 1);
        }
    }
    
    ss << ");\n}\n";
    
    return ss.str();
}

// =============================================================================
// LogIssue 구현
// =============================================================================

std::string LogIssue::formatForDisplay() const {
    std::ostringstream ss;
    
    ss << "// File: " << file << ":" << line << "\n";
    ss << "// Type: ";
    switch (type) {
        case LogType::Performance: ss << "Performance"; break;
        case LogType::Memory: ss << "Memory"; break;
        case LogType::Error: ss << "Error"; break;
        case LogType::Blueprint: ss << "Blueprint"; break;
        case LogType::Warning: ss << "Warning"; break;
    }
    ss << ", Severity: ";
    switch (severity) {
        case LogSeverity::Critical: ss << "Critical"; break;
        case LogSeverity::High: ss << "High"; break;
        case LogSeverity::Medium: ss << "Medium"; break;
        case LogSeverity::Low: ss << "Low"; break;
    }
    ss << "\n";
    ss << "// Message: " << message << "\n";
    ss << "// Suggestion: " << suggestion << "\n";
    
    return ss.str();
}

// =============================================================================
// UnrealLogAnalyzer 구현
// =============================================================================

UnrealLogAnalyzer::UnrealLogAnalyzer() {
    initializePatterns();
}

void UnrealLogAnalyzer::initializePatterns() {
    patterns_[LogType::Performance] = {
        std::regex(R"(LogStats:\s+(.+)\s+took\s+(\d+\.?\d*)ms)"),
        std::regex(R"(LogRenderer:\s+Frame\s+time:\s+(\d+\.?\d*)ms)"),
        std::regex(R"(LogGameThread:\s+(.+)\s+(\d+\.?\d*)ms)"),
        std::regex(R"(LogSlate:\s+Slow\s+widget\s+update.*(\d+\.?\d*)ms)")
    };
    
    patterns_[LogType::Memory] = {
        std::regex(R"(LogMemory:\s+(\d+)\s+bytes\s+leaked)"),
        std::regex(R"(LogGC:\s+Garbage\s+collection\s+took\s+(\d+\.?\d*)ms)"),
        std::regex(R"(LogMemory:\s+Out\s+of\s+memory)"),
        std::regex(R"(LogMemory:\s+Allocation\s+failed.*size:\s+(\d+))")
    };
    
    patterns_[LogType::Error] = {
        std::regex(R"(LogTemp:\s+Error:\s+(.+))"),
        std::regex(R"(LogCore:\s+Error:\s+(.+))"),
        std::regex(R"(LogBlueprint:\s+Error:\s+(.+))"),
        std::regex(R"(LogCompile:\s+Error:\s+(.+))"),
        std::regex(R"(Error:\s+(.+))")
    };
    
    patterns_[LogType::Blueprint] = {
        std::regex(R"(LogBlueprint:\s+(.+)\s+failed\s+to\s+compile)"),
        std::regex(R"(LogBlueprintUserMessages:\s+(.+))"),
        std::regex(R"(LogBlueprint:\s+Warning:\s+(.+))"),
        std::regex(R"(Blueprint\s+compile\s+error:\s+(.+))")
    };
    
    patterns_[LogType::Warning] = {
        std::regex(R"(LogTemp:\s+Warning:\s+(.+))"),
        std::regex(R"(LogCore:\s+Warning:\s+(.+))"),
        std::regex(R"(Warning:\s+(.+))")
    };
}

std::vector<LogIssue> UnrealLogAnalyzer::analyzeProject(const std::string& projectPath) {
    std::vector<LogIssue> allIssues;
    
    auto logFiles = findLogFiles(projectPath);
    
    for (const auto& logFile : logFiles) {
        auto issues = analyzeLogFile(logFile);
        allIssues.insert(allIssues.end(), issues.begin(), issues.end());
    }
    
    return allIssues;
}

std::string UnrealLogAnalyzer::generateAnalysisReport(const std::vector<LogIssue>& issues) {
    std::ostringstream report;
    
    report << "/*\n";
    report << " * UNREAL ENGINE LOG ANALYSIS REPORT\n";
    report << " * Generated: " << std::chrono::system_clock::now().time_since_epoch().count() << "\n";
    report << " * Total Issues Found: " << issues.size() << "\n";
    report << " * ==========================================\n";
    report << " */\n\n";
    
    std::unordered_map<LogSeverity, std::vector<LogIssue>> groupedIssues;
    for (const auto& issue : issues) {
        groupedIssues[issue.severity].push_back(issue);
    }
    
    for (auto severity : {LogSeverity::Critical, LogSeverity::High, LogSeverity::Medium, LogSeverity::Low}) {
        if (groupedIssues[severity].empty()) continue;
        
        report << "// " << static_cast<int>(severity) << " SEVERITY ISSUES (" << groupedIssues[severity].size() << ")\n";
        report << "// " << std::string(50, '=') << "\n";
        
        for (const auto& issue : groupedIssues[severity]) {
            report << issue.formatForDisplay() << "\n";
        }
        
        report << "\n";
    }
    
    return report.str();
}

std::vector<std::string> UnrealLogAnalyzer::findLogFiles(const std::string& projectPath) {
    std::vector<std::string> logFiles;
    
    std::vector<std::string> logPaths = {
        projectPath + "/Saved/Logs",
        projectPath + "/Intermediate/Build/Win64/UnrealHeaderTool/Development/Engine/Logs"
    };
    
    for (const auto& logPath : logPaths) {
        if (fs::exists(logPath)) {
            try {
                for (const auto& entry : fs::directory_iterator(logPath)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".log") {
                        logFiles.push_back(entry.path().string());
                    }
                }
            } catch (const fs::filesystem_error&) {
                // 권한 오류 등 무시
            }
        }
    }
    
    return logFiles;
}

std::vector<LogIssue> UnrealLogAnalyzer::analyzeLogFile(const std::string& logFile) {
    std::vector<LogIssue> issues;
    
    std::ifstream file(logFile);
    if (!file.is_open()) return issues;
    
    std::string line;
    int lineNum = 0;
    
    while (std::getline(file, line)) {
        lineNum++;
        
        for (const auto& [logType, patterns] : patterns_) {
            for (const auto& pattern : patterns) {
                std::smatch match;
                if (std::regex_search(line, match, pattern)) {
                    LogIssue issue;
                    issue.type = logType;
                    issue.message = match[0].str();
                    issue.file = logFile;
                    issue.line = lineNum;
                    issue.severity = LogSeverity::Medium; // 기본값
                    issue.suggestion = "Check the related code section";
                    
                    issues.push_back(issue);
                    break;
                }
            }
        }
    }
    
    return issues;
}

// =============================================================================
// CompileError 구현
// =============================================================================

std::string CompileError::formatSolution() const {
    std::ostringstream ss;
    
    ss << "// Error in " << file << ":" << line << "\n";
    ss << "// Category: " << static_cast<int>(category) << "\n";
    ss << "// Confidence: " << (confidence * 100) << "%\n";
    ss << "// Message: " << message << "\n";
    ss << "// Solution: " << solution << "\n";
    
    return ss.str();
}

// =============================================================================
// CompileErrorInterpreter 구현
// =============================================================================

CompileErrorInterpreter::CompileErrorInterpreter() {
    initializePatterns();
}

void CompileErrorInterpreter::initializePatterns() {
    patterns_ = {
        {
            std::regex(R"(error: use of undeclared identifier '(\w+)')"),
            ErrorCategory::MissingInclude,
            "Add #include for '{1}' or check spelling. Common includes for '{1}': CoreMinimal.h, Engine.h",
            0.9
        },
        {
            std::regex(R"(error: no member named '(\w+)' in)"),
            ErrorCategory::MemberNotFound,
            "Member '{1}' does not exist. Check spelling, access level, or add forward declaration",
            0.8
        },
        {
            std::regex(R"(error: UCLASS\(\) must be the first thing)"),
            ErrorCategory::UnrealMacro,
            "Move UCLASS() macro to immediately before class declaration",
            0.95
        },
        {
            std::regex(R"(error: GENERATED_BODY\(\) not found)"),
            ErrorCategory::UnrealMacro,
            "Add GENERATED_BODY() as first line inside UCLASS body",
            0.95
        },
        {
            std::regex(R"(error: Cannot find definition for module '(\w+)')"),
            ErrorCategory::ModuleNotFound,
            "Add '{1}' to PublicDependencyModuleNames in your .Build.cs file",
            0.9
        }
    };
}

std::vector<CompileError> CompileErrorInterpreter::analyzeErrors(const std::string& projectPath) {
    auto errorMessages = extractCompileErrors(projectPath);
    std::vector<CompileError> errors;
    
    for (const auto& message : errorMessages) {
        auto error = interpretError(message);
        errors.push_back(error);
    }
    
    return errors;
}

std::string CompileErrorInterpreter::generateErrorReport(const std::vector<CompileError>& errors) {
    std::ostringstream report;
    
    report << "/*\n";
    report << " * COMPILE ERROR ANALYSIS & SOLUTIONS\n";
    report << " * Found " << errors.size() << " compile errors\n";
    report << " * ==========================================\n";
    report << " */\n\n";
    
    for (size_t i = 0; i < errors.size() && i < 20; ++i) {
        const auto& error = errors[i];
        
        report << "// ERROR #" << (i + 1) << " [" << static_cast<int>(error.category) << "]\n";
        report << "// " << std::string(50, '-') << "\n";
        report << error.formatSolution() << "\n\n";
    }
    
    return report.str();
}

std::vector<std::string> CompileErrorInterpreter::extractCompileErrors(const std::string& projectPath) {
    std::vector<std::string> errors;
    
    // 빌드 로그에서 에러 메시지 추출
    std::string logPath = projectPath + "/Saved/Logs/UnrealBuildTool.log";
    
    if (fs::exists(logPath)) {
        std::ifstream file(logPath);
        std::string line;
        
        while (std::getline(file, line)) {
            if (line.find("error:") != std::string::npos) {
                errors.push_back(line);
            }
        }
    }
    
    return errors;
}

CompileError CompileErrorInterpreter::interpretError(const std::string& errorMessage) {
    CompileError error;
    error.message = errorMessage;
    error.category = ErrorCategory::Unknown;
    error.confidence = 0.0;
    error.solution = "Manual investigation required";
    
    for (const auto& pattern : patterns_) {
        std::smatch match;
        if (std::regex_search(errorMessage, match, pattern.pattern)) {
            error.category = pattern.category;
            error.confidence = pattern.confidence;
            error.solution = pattern.solution;
            break;
        }
    }
    
    return error;
}

// =============================================================================
// UnrealCodeGenerator 구현
// =============================================================================

std::string UnrealCodeGenerator::generateUClass(const ClassTemplate& template_) {
    std::ostringstream ss;
    
    ss << "#pragma once\n\n";
    ss << "#include \"CoreMinimal.h\"\n";
    ss << "#include \"" << template_.baseClass << ".h\"\n";
    ss << "#include \"" << template_.className << ".generated.h\"\n\n";
    
    ss << "UCLASS(";
    if (template_.isBlueprintType) ss << "BlueprintType";
    if (template_.isBlueprintable) {
        if (template_.isBlueprintType) ss << ", ";
        ss << "Blueprintable";
    }
    ss << ")\n";
    
    ss << "class " << template_.moduleName << "_API " << template_.className;
    ss << " : public " << template_.baseClass << "\n{\n";
    ss << "\tGENERATED_BODY()\n\n";
    
    ss << "public:\n";
    ss << "\t" << template_.className << "();\n\n";
    
    if (template_.baseClass == "AActor" || template_.baseClass == "APawn") {
        ss << "protected:\n";
        ss << "\tvirtual void BeginPlay() override;\n\n";
        ss << "public:\n";
        ss << "\tvirtual void Tick(float DeltaTime) override;\n\n";
    }
    
    for (const auto& component : template_.components) {
        ss << "\tUPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = \"Components\")\n";
        ss << "\tclass " << component << "* " << component.substr(1) << "Component;\n\n";
    }
    
    for (const auto& func : template_.customFunctions) {
        ss << "\tUFUNCTION(BlueprintCallable, Category = \"Gameplay\")\n";
        ss << "\tvoid " << func << "();\n\n";
    }
    
    ss << "};";
    
    return ss.str();
}

std::string UnrealCodeGenerator::generateUStruct(const std::string& structName, const std::vector<std::string>& members) {
    std::ostringstream ss;
    
    ss << "USTRUCT(BlueprintType)\n";
    ss << "struct F" << structName << "\n{\n";
    ss << "\tGENERATED_USTRUCT_BODY()\n\n";
    
    for (const auto& member : members) {
        ss << "\tUPROPERTY(EditAnywhere, BlueprintReadWrite)\n";
        ss << "\t" << member << ";\n\n";
    }
    
    ss << "};";
    
    return ss.str();
}

std::string UnrealCodeGenerator::generateUFunction(const std::string& functionName, const std::vector<std::string>& parameters) {
    std::ostringstream ss;
    
    ss << "UFUNCTION(BlueprintCallable, Category = \"Gameplay\")\n";
    ss << "void " << functionName << "(";
    
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << parameters[i];
    }
    
    ss << ");\n";
    
    return ss.str();
}

std::string UnrealCodeGenerator::generateUProperty(const std::string& propertyName, const std::string& type) {
    std::ostringstream ss;
    
    ss << "UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = \"Default\")\n";
    ss << type << " " << propertyName << ";\n";
    
    return ss.str();
}

// =============================================================================
// VersionCompatibleAutoComplete 구현
// =============================================================================

VersionCompatibleAutoComplete::VersionCompatibleAutoComplete(const EngineVersion& version)
    : engineVersion_(version), headerScanner_(version) {
    
    std::thread([this]() {
        headerScanner_.scanEngineHeaders();
    }).detach();
}

std::vector<json> VersionCompatibleAutoComplete::getCompletions(const std::string& prefix, const std::string& context) {
    std::vector<json> completions;
    
    auto macroCompletions = getMacroCompletions(prefix);
    completions.insert(completions.end(), macroCompletions.begin(), macroCompletions.end());
    
    if (context.find("::") != std::string::npos) {
        auto memberCompletions = getMemberCompletions(context, prefix);
        completions.insert(completions.end(), memberCompletions.begin(), memberCompletions.end());
    }
    
    return completions;
}

std::vector<json> VersionCompatibleAutoComplete::getMacroCompletions(const std::string& prefix) {
    std::vector<json> completions;
    
    std::vector<std::string> macros = {"UCLASS", "USTRUCT", "UFUNCTION", "UPROPERTY", "UENUM"};
    
    for (const auto& macro : macros) {
        if (prefix.empty() || macro.find(prefix) == 0) {
            json completion;
            completion["label"] = macro;
            completion["insertText"] = apiDatabase_.getMacroTemplate(macro, engineVersion_);
            completion["detail"] = "Unreal Engine " + engineVersion_.toString() + " Macro";
            completion["kind"] = 15;
            completion["sortText"] = "0_" + macro;
            
            completions.push_back(completion);
        }
    }
    
    return completions;
}

std::vector<json> VersionCompatibleAutoComplete::getMemberCompletions(const std::string& context, const std::string& prefix) {
    std::vector<json> completions;
    
    size_t pos = context.rfind("::");
    if (pos == std::string::npos) return completions;
    
    std::string className = context.substr(0, pos);
    size_t lastSpace = className.find_last_of(" \t");
    if (lastSpace != std::string::npos) {
        className = className.substr(lastSpace + 1);
    }
    
    auto apiMethods = apiDatabase_.getClassMethods(className, engineVersion_);
    auto scannedMethods = headerScanner_.getClassMethods(className);
    
    std::unordered_set<std::string> allMethods(apiMethods.begin(), apiMethods.end());
    allMethods.insert(scannedMethods.begin(), scannedMethods.end());
    
    for (const auto& method : allMethods) {
        if (prefix.empty() || method.find(prefix) == 0) {
            json completion;
            completion["label"] = method;
            completion["insertText"] = method;
            completion["detail"] = className + "::" + method + " (UE " + engineVersion_.toString() + ")";
            completion["kind"] = 2;
            completion["sortText"] = "1_" + method;
            
            completions.push_back(completion);
        }
    }
    
    return completions;
}

// =============================================================================
// UnrealEngineAnalyzer 구현
// =============================================================================

UnrealEngineAnalyzer::UnrealEngineAnalyzer(const std::string& enginePath, const std::string& projectPath)
    : enginePath_(enginePath), projectPath_(projectPath) {
    
    // 프로젝트 엔진 버전 감지
    UnrealEngineDetector detector;
    engineVersion_ = detector.detectProjectEngineVersion(projectPath);
    
    // 엔진 경로가 비어있으면 감지된 버전의 경로 사용
    if (enginePath_.empty() && !engineVersion_.installPath.empty()) {
        enginePath_ = engineVersion_.installPath;
    }
    
    // 서브시스템들 초기화
    logAnalyzer_ = std::make_unique<UnrealLogAnalyzer>();
    errorInterpreter_ = std::make_unique<CompileErrorInterpreter>();
    headerSourceLinker_ = std::make_unique<HeaderSourceLinker>();
    blueprintIntegration_ = std::make_unique<BlueprintIntegration>();
    codeGenerator_ = std::make_unique<UnrealCodeGenerator>();
    autoComplete_ = std::make_unique<VersionCompatibleAutoComplete>(engineVersion_);
    
    // 엔진 include 경로들 설정
    VersionSpecificAPI apiDB;
    engineIncludePaths_ = apiDB.getIncludePaths(engineVersion_);
    
    startBackgroundIndexing();
}

std::string UnrealEngineAnalyzer::generateUClassTemplate(const std::string& className, const std::string& baseClass) {
    ClassTemplate template_;
    template_.className = className;
    template_.baseClass = baseClass;
    template_.moduleName = "GAME";
    template_.isBlueprintType = true;
    template_.isBlueprintable = true;
    
    return codeGenerator_->generateUClass(template_);
}

std::string UnrealEngineAnalyzer::generateBlueprintFunction(const std::string& uri, int line, int character) {
    std::lock_guard<std::mutex> lock(dataMutex_);
    
    if (fileFunctions_.count(uri)) {
        for (const auto& func : fileFunctions_[uri]) {
            if (func.location.range.start.line <= line && func.location.range.end.line >= line) {
                return func.generateBlueprintWrapper();
            }
        }
    }
    
    return "// No function found at current position";
}

std::string UnrealEngineAnalyzer::syncHeaderSource(const std::string& uri) {
    if (isHeaderFile(uri)) {
        auto fileInfo = headerSourceLinker_->analyzeFilePair(uri);
        return headerSourceLinker_->generateMissingImplementations(fileInfo);
    } else if (isSourceFile(uri)) {
        return headerSourceLinker_->generateHeaderFromSource(uri);
    }
    
    return "// Unable to sync: not a valid header or source file";
}

std::string UnrealEngineAnalyzer::analyzeUnrealLogs(const std::string& projectPath) {
    auto issues = logAnalyzer_->analyzeProject(projectPath);
    return logAnalyzer_->generateAnalysisReport(issues);
}

std::string UnrealEngineAnalyzer::interpretCompileErrors(const std::string& projectPath) {
    auto errors = errorInterpreter_->analyzeErrors(projectPath);
    return errorInterpreter_->generateErrorReport(errors);
}

std::string UnrealEngineAnalyzer::executeCodeAction(const std::string& action, const nlohmann::json& params) {
    std::string uri = params["textDocument"]["uri"];
    int line = params["position"]["line"];
    int character = params["position"]["character"];
    
    if (action == "generateUClass") {
        std::string className = params.value("className", "MyActor");
        std::string baseClass = params.value("baseClass", "AActor");
        return generateUClassTemplate(className, baseClass);
    }
    else if (action == "generateBlueprintFunction") {
        return generateBlueprintFunction(uri, line, character);
    }
    else if (action == "syncHeaderSource") {
        return syncHeaderSource(uri);
    }
    else if (action == "analyzeLogs") {
        return analyzeUnrealLogs(projectPath_);
    }
    else if (action == "interpretErrors") {
        return interpretCompileErrors(projectPath_);
    }
    
    return "// Unknown action: " + action;
}

std::vector<CompletionItem> UnrealEngineAnalyzer::getCompletions(
    const std::string& uri,
    int line,
    int character,
    const std::string& text
) {
    std::lock_guard<std::mutex> lock(dataMutex_);
    
    std::string currentWord = getCurrentWord(text, line, character);
    std::string context = detectUnrealContext(text, line, character);
    
    std::vector<CompletionItem> completions;
    
    // 버전 호환 자동완성 사용
    auto jsonCompletions = autoComplete_->getCompletions(currentWord, context);
    
    for (const auto& jsonCompletion : jsonCompletions) {
        CompletionItem item;
        item.label = jsonCompletion["label"];
        item.insertText = jsonCompletion["insertText"];
        item.detail = jsonCompletion["detail"];
        item.kind = jsonCompletion["kind"];
        item.sortText = jsonCompletion["sortText"];
        
        completions.push_back(item);
    }
    
    return completions;
}

void UnrealEngineAnalyzer::startBackgroundIndexing() {
    // 백그라운드에서 프로젝트 인덱싱 시작
    std::thread([this]() {
        // 프로젝트 파일들 스캔 및 인덱싱
        // 실제 구현에서는 더 복잡한 로직이 필요
    }).detach();
}

std::string UnrealEngineAnalyzer::getCurrentWord(const std::string& text, int line, int character) {
    // 간단한 구현 - 실제로는 더 정교한 파싱 필요
    return "";
}

std::string UnrealEngineAnalyzer::detectUnrealContext(const std::string& text, int line, int character) {
    // 간단한 구현 - 실제로는 더 정교한 컨텍스트 감지 필요
    return "";
}

UnrealClass* UnrealEngineAnalyzer::findClassAtPosition(const std::string& uri, int line) {
    // 간단한 구현 - 실제로는 AST 파싱 필요
    return nullptr;
}

bool UnrealEngineAnalyzer::isHeaderFile(const std::string& uri) {
    return endsWith(uri, ".h") || endsWith(uri, ".hpp");
}

bool UnrealEngineAnalyzer::isSourceFile(const std::string& uri) {
    return endsWith(uri, ".cpp") || endsWith(uri, ".cc");
}

std::string UnrealEngineAnalyzer::getCorrespondingFile(const std::string& uri) {
    if (isHeaderFile(uri)) {
        std::string base = uri.substr(0, uri.find_last_of('.'));
        return base + ".cpp";
    } else if (isSourceFile(uri)) {
        std::string base = uri.substr(0, uri.find_last_of('.'));
        return base + ".h";
    }
    
    return "";
}

// =============================================================================
// LSPServer 구현
// =============================================================================

void LSPServer::initialize(const std::string& projectPath, const std::string& enginePath) {
    analyzer_ = std::make_unique<UnrealEngineAnalyzer>(enginePath, projectPath);
}

void LSPServer::run() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.find("Content-Length:") == 0) {
            int contentLength = std::stoi(line.substr(16));
            std::getline(std::cin, line); // 빈 줄 건너뛰기
            
            std::string message;
            message.resize(contentLength);
            std::cin.read(&message[0], contentLength);
            
            handleMessage(message);
        }
    }
}

void LSPServer::handleMessage(const std::string& message) {
    try {
        auto parsedMsg = parseMessage(message);
        
        if (parsedMsg.method == "initialize") {
            handleInitialize(parsedMsg);
        } else if (parsedMsg.method == "textDocument/didOpen") {
            handleTextDocumentDidOpen(parsedMsg);
        } else if (parsedMsg.method == "textDocument/didChange") {
            handleTextDocumentDidChange(parsedMsg);
        } else if (parsedMsg.method == "textDocument/completion") {
            handleTextDocumentCompletion(parsedMsg);
        } else if (parsedMsg.method == "workspace/executeCommand") {
            handleWorkspaceExecuteCommand(parsedMsg);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error handling message: " << e.what() << std::endl;
    }
}

void LSPServer::handleInitialize(const LSPMessage& msg) {
    json result = {
        {"capabilities", {
            {"textDocumentSync", 1},
            {"completionProvider", {
                {"triggerCharacters", {".", "::", "U", "A", "F"}}
            }},
            {"executeCommandProvider", {
                {"commands", {
                    "unreal.generateUClass",
                    "unreal.generateBlueprintFunction",
                    "unreal.syncHeaderSource",
                    "unreal.analyzeLogs",
                    "unreal.interpretErrors"
                }}
            }}
        }}
    };
    
    sendResponse(msg.id.value(), result);
}

void LSPServer::handleTextDocumentDidOpen(const LSPMessage& msg) {
    std::string uri = msg.params["textDocument"]["uri"];
    std::string text = msg.params["textDocument"]["text"];
    
    openFiles_[uri] = text;
}

void LSPServer::handleTextDocumentDidChange(const LSPMessage& msg) {
    std::string uri = msg.params["textDocument"]["uri"];
    auto changes = msg.params["contentChanges"];
    
    if (!changes.empty()) {
        openFiles_[uri] = changes[0]["text"];
    }
}

void LSPServer::handleTextDocumentCompletion(const LSPMessage& msg) {
    std::string uri = msg.params["textDocument"]["uri"];
    int line = msg.params["position"]["line"];
    int character = msg.params["position"]["character"];
    
    if (openFiles_.find(uri) != openFiles_.end()) {
        auto completions = analyzer_->getCompletions(uri, line, character, openFiles_[uri]);
        
        json items = json::array();
        for (const auto& completion : completions) {
            json item;
            item["label"] = completion.label;
            item["insertText"] = completion.insertText;
            item["detail"] = completion.detail;
            item["kind"] = completion.kind;
            item["sortText"] = completion.sortText;
            
            items.push_back(item);
        }
        
        sendResponse(msg.id.value(), items);
    }
}

void LSPServer::handleWorkspaceExecuteCommand(const LSPMessage& msg) {
    std::string command = msg.params["command"];
    nlohmann::json arguments = msg.params.value("arguments", nlohmann::json::array());
    
    std::string result;
    
    if (command == "unreal.generateUClass") {
        result = analyzer_->executeCodeAction("generateUClass", arguments[0]);
    }
    else if (command == "unreal.generateBlueprintFunction") {
        result = analyzer_->executeCodeAction("generateBlueprintFunction", arguments[0]);
    }
    else if (command == "unreal.syncHeaderSource") {
        result = analyzer_->executeCodeAction("syncHeaderSource", arguments[0]);
    }
    else if (command == "unreal.analyzeLogs") {
        result = analyzer_->executeCodeAction("analyzeLogs", arguments[0]);
    }
    else if (command == "unreal.interpretErrors") {
        result = analyzer_->executeCodeAction("interpretErrors", arguments[0]);
    }
    
    sendResponse(msg.id.value(), nlohmann::json(result));
}

void LSPServer::sendResponse(int id, const json& result) {
    json response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = result;
    
    std::string responseStr = response.dump();
    std::cout << "Content-Length: " << responseStr.length() << "\r\n\r\n" << responseStr;
    std::cout.flush();
}

void LSPServer::sendNotification(const std::string& method, const json& params) {
    json notification;
    notification["jsonrpc"] = "2.0";
    notification["method"] = method;
    notification["params"] = params;
    
    std::string notificationStr = notification.dump();
    std::cout << "Content-Length: " << notificationStr.length() << "\r\n\r\n" << notificationStr;
    std::cout.flush();
}

LSPMessage LSPServer::parseMessage(const std::string& message) {
    LSPMessage msg;
    
    try {
        json jsonMsg = json::parse(message);
        
        if (jsonMsg.find("id") != jsonMsg.end()) {
            msg.id = jsonMsg["id"];
        }
        
        msg.method = jsonMsg["method"];
        msg.params = jsonMsg.value("params", json::object());
        
    } catch (const json::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }
    
    return msg;
}

std::string LSPServer::getCurrentWord(const std::string& text, int line, int character) {
    // 더 정교한 파싱 필요
    return "";
}

std::string LSPServer::getContext(const std::string& text, int line, int character) {
    // 더 정교한 컨텍스트 감지 필요
    return "";
}

// =============================================================================
// 나머지 클래스들의 기본 구현
// =============================================================================

FilePairInfo HeaderSourceLinker::analyzeFilePair(const std::string& headerPath) {
    FilePairInfo info;
    info.headerPath = headerPath;
    // 기본 구현
    return info;
}

std::string HeaderSourceLinker::generateMissingImplementations(const FilePairInfo& fileInfo) {
    return "// Missing implementations would be generated here";
}

std::string HeaderSourceLinker::generateHeaderFromSource(const std::string& sourcePath) {
    return "// Header from source would be generated here";
}

std::vector<std::string> HeaderSourceLinker::extractFunctionDeclarations(const std::string& content) {
    return {};
}

std::vector<std::string> HeaderSourceLinker::extractFunctionImplementations(const std::string& content) {
    return {};
}

std::string HeaderSourceLinker::getCorrespondingFile(const std::string& filePath) {
    return "";
}

std::string BlueprintIntegration::generateBlueprintNode(const std::string& functionName, const std::vector<std::string>& parameters) {
    return "// Blueprint node generation";
}

std::vector<std::string> BlueprintIntegration::findBlueprintCallableFunction(const std::string& projectPath) {
    return {};
}

std::string BlueprintIntegration::generateBlueprintWrapper(const FunctionInfo& func) {
    return func.generateBlueprintWrapper();
}

std::vector<CompletionItem> UnrealEngineAnalyzer::generateUnrealMacroCompletions(const std::string& currentWord, const std::string& context) {
    return {};
}

std::vector<CompletionItem> UnrealEngineAnalyzer::generateClassMemberCompletions(const std::string& className, const std::string& currentWord) {
    return {};
}

std::vector<CompletionItem> UnrealEngineAnalyzer::generateEngineClassCompletions(const std::string& currentWord) {
    return {};
}

std::vector<CompletionItem> UnrealEngineAnalyzer::generateIncludeCompletions(const std::string& currentWord) {
    return {};
}

} // namespace UnrealEngine
