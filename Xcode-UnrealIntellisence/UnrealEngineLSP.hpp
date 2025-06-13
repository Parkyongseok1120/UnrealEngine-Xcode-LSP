#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <regex>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <memory>
#include <mutex>
#include <thread>
#include <chrono>
#include <sstream>
#include <optional>
#include <cstdlib>
#include "json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace UnrealEngine {

// =============================================================================
// 엔진 버전 관리
// =============================================================================

struct EngineVersion {
    int major;
    int minor;
    int patch;
    std::string fullVersion;
    std::string installPath;
    
    bool isUE5() const { return major >= 5; }
    bool isUE4() const { return major == 4; }
    
    std::string toString() const {
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }
    
    bool operator<(const EngineVersion& other) const {
        if (major != other.major) return major < other.major;
        if (minor != other.minor) return minor < other.minor;
        return patch < other.patch;
    }
    
    bool operator>=(const EngineVersion& other) const {
        return !(*this < other);
    }
    
    bool operator==(const EngineVersion& other) const {
        return major == other.major && minor == other.minor && patch == other.patch;
    }
};

class UnrealEngineDetector {
private:
    std::vector<std::string> commonInstallPaths_;
    
public:
    UnrealEngineDetector();
    
    std::vector<EngineVersion> findAllEngineVersions();
    EngineVersion detectProjectEngineVersion(const std::string& projectPath);
    
private:
    EngineVersion detectEngineVersion(const std::string& enginePath);
    EngineVersion parseEngineAssociation(const std::string& engineAssoc);
};

// =============================================================================
// 버전별 API 데이터베이스
// =============================================================================

class VersionSpecificAPI {
private:
    std::unordered_map<std::string, json> apiDatabase_;
    
public:
    VersionSpecificAPI();
    
    std::vector<std::string> getClassMethods(const std::string& className, const EngineVersion& version);
    std::string getMacroTemplate(const std::string& macroName, const EngineVersion& version);
    std::vector<std::string> getIncludePaths(const EngineVersion& version);
    
private:
    void initializeAPIDatabase();
    std::string getVersionKey(const EngineVersion& version);
    std::vector<std::string> getDefaultClassMethods(const std::string& className);
    std::string getDefaultMacroTemplate(const std::string& macroName, const EngineVersion& version);
    std::vector<std::string> getDefaultIncludePaths(const EngineVersion& version);
};

// =============================================================================
// 동적 헤더 스캐너
// =============================================================================

class DynamicHeaderScanner {
private:
    EngineVersion engineVersion_;
    std::string enginePath_;
    std::unordered_map<std::string, std::vector<std::string>> scannedClasses_;
    
public:
    DynamicHeaderScanner(const EngineVersion& version);
    
    void scanEngineHeaders();
    std::vector<std::string> getClassMethods(const std::string& className);
    
private:
    std::vector<std::string> getEnginePaths();
    void scanDirectory(const std::string& dirPath);
    void scanHeaderFile(const std::string& filePath);
    std::vector<std::string> extractClassMethods(const std::string& content, const std::string& className);
};

// =============================================================================
// LSP 관련 구조체
// =============================================================================

struct LSPMessage {
    std::optional<int> id;
    std::string method;
    json params;
};

struct CompletionItem {
    std::string label;
    std::string insertText;
    std::string detail;
    int kind;
    std::string sortText;
};

struct Location {
    std::string uri;
    struct Range {
        struct Position {
            int line;
            int character;
        } start, end;
    } range;
};

struct FunctionInfo {
    std::string name;
    std::string signature;
    Location location;
    std::vector<std::string> parameters;
    std::string returnType;
    
    std::string generateBlueprintWrapper() const;
};

struct UnrealClass {
    std::string name;
    std::string baseClass;
    std::vector<std::string> includes;
    std::vector<FunctionInfo> functions;
    std::vector<std::string> properties;
    Location location;
};

// =============================================================================
// 로그 분석 시스템
// =============================================================================

enum class LogType {
    Performance,
    Memory,
    Error,
    Blueprint,
    Warning
};

enum class LogSeverity {
    Critical = 0,
    High = 1,
    Medium = 2,
    Low = 3
};

struct LogIssue {
    LogType type;
    LogSeverity severity;
    std::string message;
    std::string file;
    int line;
    std::string suggestion;
    
    std::string formatForDisplay() const;
};

class UnrealLogAnalyzer {
private:
    std::unordered_map<LogType, std::vector<std::regex>> patterns_;
    
public:
    UnrealLogAnalyzer();
    
    std::vector<LogIssue> analyzeProject(const std::string& projectPath);
    std::string generateAnalysisReport(const std::vector<LogIssue>& issues);
    
private:
    void initializePatterns();
    std::vector<std::string> findLogFiles(const std::string& projectPath);
    std::vector<LogIssue> analyzeLogFile(const std::string& logFile);
};

// =============================================================================
// 컴파일 에러 해석기
// =============================================================================

enum class ErrorCategory {
    MissingInclude,
    MemberNotFound,
    UnrealMacro,
    ModuleNotFound,
    SyntaxError,
    Unknown
};

struct ErrorPattern {
    std::regex pattern;
    ErrorCategory category;
    std::string solution;
    double confidence;
};

struct CompileError {
    std::string message;
    std::string file;
    int line;
    ErrorCategory category;
    std::string solution;
    double confidence;
    
    std::string formatSolution() const;
};

class CompileErrorInterpreter {
private:
    std::vector<ErrorPattern> patterns_;
    
public:
    CompileErrorInterpreter();
    
    std::vector<CompileError> analyzeErrors(const std::string& projectPath);
    std::string generateErrorReport(const std::vector<CompileError>& errors);
    
private:
    void initializePatterns();
    std::vector<std::string> extractCompileErrors(const std::string& projectPath);
    CompileError interpretError(const std::string& errorMessage);
};

// =============================================================================
// 헤더-소스 링커
// =============================================================================

struct FilePairInfo {
    std::string headerPath;
    std::string sourcePath;
    std::vector<std::string> headerFunctions;
    std::vector<std::string> sourceFunctions;
    std::vector<std::string> missingImplementations;
};

class HeaderSourceLinker {
public:
    FilePairInfo analyzeFilePair(const std::string& headerPath);
    std::string generateMissingImplementations(const FilePairInfo& fileInfo);
    std::string generateHeaderFromSource(const std::string& sourcePath);
    
private:
    std::vector<std::string> extractFunctionDeclarations(const std::string& content);
    std::vector<std::string> extractFunctionImplementations(const std::string& content);
    std::string getCorrespondingFile(const std::string& filePath);
};

// =============================================================================
// 블루프린트 통합
// =============================================================================

class BlueprintIntegration {
public:
    std::string generateBlueprintNode(const std::string& functionName, const std::vector<std::string>& parameters);
    std::vector<std::string> findBlueprintCallableFunction(const std::string& projectPath);
    std::string generateBlueprintWrapper(const FunctionInfo& func);
};

// =============================================================================
// 언리얼 코드 생성기
// =============================================================================

struct ClassTemplate {
    std::string className;
    std::string baseClass;
    std::string moduleName;
    bool isBlueprintType;
    bool isBlueprintable;
    std::vector<std::string> components;
    std::vector<std::string> customFunctions;
};

class UnrealCodeGenerator {
public:
    std::string generateUClass(const ClassTemplate& template_);
    std::string generateUStruct(const std::string& structName, const std::vector<std::string>& members);
    std::string generateUFunction(const std::string& functionName, const std::vector<std::string>& parameters);
    std::string generateUProperty(const std::string& propertyName, const std::string& type);
};

// =============================================================================
// 버전 호환 자동완성
// =============================================================================

class VersionCompatibleAutoComplete {
private:
    EngineVersion engineVersion_;
    VersionSpecificAPI apiDatabase_;
    DynamicHeaderScanner headerScanner_;
    
public:
    VersionCompatibleAutoComplete(const EngineVersion& version);
    
    std::vector<json> getCompletions(const std::string& prefix, const std::string& context);
    
private:
    std::vector<json> getMacroCompletions(const std::string& prefix);
    std::vector<json> getMemberCompletions(const std::string& context, const std::string& prefix);
};

// =============================================================================
// 통합 언리얼 엔진 분석기
// =============================================================================

class UnrealEngineAnalyzer {
private:
    std::string enginePath_;
    std::string projectPath_;
    EngineVersion engineVersion_;
    
    // 서브시스템들
    std::unique_ptr<UnrealLogAnalyzer> logAnalyzer_;
    std::unique_ptr<CompileErrorInterpreter> errorInterpreter_;
    std::unique_ptr<HeaderSourceLinker> headerSourceLinker_;
    std::unique_ptr<BlueprintIntegration> blueprintIntegration_;
    std::unique_ptr<UnrealCodeGenerator> codeGenerator_;
    std::unique_ptr<VersionCompatibleAutoComplete> autoComplete_;
    
    // 데이터
    std::vector<std::string> engineIncludePaths_;
    std::unordered_map<std::string, std::vector<FunctionInfo>> fileFunctions_;
    std::unordered_map<std::string, UnrealClass> fileClasses_;
    std::mutex dataMutex_;
    
public:
    UnrealEngineAnalyzer(const std::string& enginePath, const std::string& projectPath);
    
    // 코드 생성 기능
    std::string generateUClassTemplate(const std::string& className, const std::string& baseClass);
    std::string generateBlueprintFunction(const std::string& uri, int line, int character);
    std::string syncHeaderSource(const std::string& uri);
    
    // 분석 기능
    std::string analyzeUnrealLogs(const std::string& projectPath);
    std::string interpretCompileErrors(const std::string& projectPath);
    
    // LSP 기능
    std::string executeCodeAction(const std::string& action, const nlohmann::json& params);
    std::vector<CompletionItem> getCompletions(const std::string& uri, int line, int character, const std::string& text);
    
    // 유틸리티
    std::vector<CompletionItem> generateUnrealMacroCompletions(const std::string& currentWord, const std::string& context);
    std::vector<CompletionItem> generateClassMemberCompletions(const std::string& className, const std::string& currentWord);
    std::vector<CompletionItem> generateEngineClassCompletions(const std::string& currentWord);
    std::vector<CompletionItem> generateIncludeCompletions(const std::string& currentWord);
    
private:
    void startBackgroundIndexing();
    std::string getCurrentWord(const std::string& text, int line, int character);
    std::string detectUnrealContext(const std::string& text, int line, int character);
    UnrealClass* findClassAtPosition(const std::string& uri, int line);
    bool isHeaderFile(const std::string& uri);
    bool isSourceFile(const std::string& uri);
    std::string getCorrespondingFile(const std::string& uri);
};

// =============================================================================
// LSP 서버
// =============================================================================

class LSPServer {
private:
    std::unique_ptr<UnrealEngineAnalyzer> analyzer_;
    std::unordered_map<std::string, std::string> openFiles_;
    
public:
    void initialize(const std::string& projectPath, const std::string& enginePath = "");
    void run();
    
    // LSP 메시지 핸들러
    void handleMessage(const std::string& message);
    void handleInitialize(const LSPMessage& msg);
    void handleTextDocumentDidOpen(const LSPMessage& msg);
    void handleTextDocumentDidChange(const LSPMessage& msg);
    void handleTextDocumentCompletion(const LSPMessage& msg);
    void handleWorkspaceExecuteCommand(const LSPMessage& msg);
    
    // 응답 전송
    void sendResponse(int id, const json& result);
    void sendNotification(const std::string& method, const json& params);
    
private:
    LSPMessage parseMessage(const std::string& message);
    std::string getCurrentWord(const std::string& text, int line, int character);
    std::string getContext(const std::string& text, int line, int character);
};

} // namespace UnrealEngine
