cmake_minimum_required(VERSION 3.20)

project(UnrealLSPServer
    VERSION 1.0.0
    DESCRIPTION "Unreal Engine Language Server Protocol Server for macOS & Xcode"
    LANGUAGES CXX
)

# macOS 최소 버전 설정
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment version")

# C++ 표준 설정 (C++17로 변경하여 호환성 향상)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# macOS 전용 체크
if(NOT APPLE)
    message(FATAL_ERROR "This project is designed for macOS only")
endif()

# 의존성 찾기
find_package(Threads REQUIRED)

# 소스 파일들
set(HEADERS
    include/UnrealEngineLSP.hpp
    include/json.hpp
)

set(SOURCES
    src/UnrealEngineLSP.cpp
    src/main.cpp
)

# 실행 파일 생성
add_executable(unreal-lsp-server ${SOURCES} ${HEADERS})

# Include 디렉토리 설정
target_include_directories(unreal-lsp-server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 링크 라이브러리
target_link_libraries(unreal-lsp-server
    PRIVATE
        Threads::Threads
)

# macOS 전용 컴파일 옵션
target_compile_options(unreal-lsp-server
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -O3
        -march=native
        -DNDEBUG
        -stdlib=libc++
)

# macOS 전용 링크 옵션
target_link_options(unreal-lsp-server
    PRIVATE
        -stdlib=libc++
)

# 디버그 빌드용 추가 옵션
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(unreal-lsp-server
        PRIVATE
            -g
            -O0
            -DDEBUG
    )
    target_compile_definitions(unreal-lsp-server
        PRIVATE
            _DEBUG
    )
endif()

# 릴리스 빌드용 추가 최적화
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(unreal-lsp-server
        PRIVATE
            -flto
            -ffast-math
    )
    target_link_options(unreal-lsp-server
        PRIVATE
            -flto
    )
endif()

# macOS 설치 설정
install(TARGETS unreal-lsp-server
    RUNTIME DESTINATION /usr/local/bin
    COMPONENT Runtime
)

# Xcode 통합을 위한 스크립트 설치
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/xcode-integration.sh"
    DESTINATION /usr/local/bin
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    COMPONENT XcodeIntegration
    OPTIONAL
)

# sourcekit-lsp 설정 파일 설치
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/config/sourcekit-lsp-config.json"
    DESTINATION "$ENV{HOME}/.config/sourcekit-lsp"
    RENAME "config.json"
    COMPONENT Configuration
)

# 헤더 파일 설치 (개발용)
install(FILES ${HEADERS}
    DESTINATION /usr/local/include/UnrealLSP
    COMPONENT Development
)

# macOS 전용 패키지 설정
set(CPACK_PACKAGE_NAME "UnrealLSPServer-macOS")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VENDOR "Unreal Engine LSP Server for macOS")
set(CPACK_PACKAGE_CONTACT "support@unreallsp-macos.com")

# macOS 패키지 생성
set(CPACK_GENERATOR "ZIP;DragNDrop;productbuild")
set(CPACK_DMG_VOLUME_NAME "UnrealLSPServer")
set(CPACK_DMG_FORMAT "UDZO")

include(CPack)

# Homebrew Formula 생성 (선택사항)
option(GENERATE_HOMEBREW_FORMULA "Generate Homebrew formula" OFF)
if(GENERATE_HOMEBREW_FORMULA)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/homebrew/unreal-lsp-server.rb.in"
        "${CMAKE_CURRENT_BINARY_DIR}/unreal-lsp-server.rb"
        @ONLY
    )
endif()

# 정보 출력
message(STATUS "=== Unreal LSP Server for macOS Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "macOS Deployment Target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Xcode Integration: Enabled")
message(STATUS "=====================================================")

# Post-install 메시지
install(CODE "
    message(STATUS \"=== Installation Complete ===\")
    message(STATUS \"LSP Server installed to: /usr/local/bin/unreal-lsp-server\")
    message(STATUS \"Config file installed to: $ENV{HOME}/.config/sourcekit-lsp/config.json\")
    message(STATUS \"\"\"
Next steps:
1. Restart Xcode
2. Open your Unreal Engine project
3. Try typing 'UC' and pressing Tab for UCLASS completion
4. Use AActor:: for member function suggestions
\"\"\")")
