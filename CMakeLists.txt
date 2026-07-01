cmake_minimum_required(VERSION 3.25)

project(KeyExtractor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg)
find_package(7zip CONFIG REQUIRED)

qt_standard_project_setup()

qt_add_executable(KeyExtractor
    src/main.cpp
)

target_link_libraries(KeyExtractor
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Svg
        7zip::7zip
)

set_target_properties(KeyExtractor PROPERTIES
    WIN32_EXECUTABLE TRUE
)

set(QT_DEPLOY_TOOL_DEBUG
    "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/tools/Qt6/bin/windeployqt.debug.bat"
)
set(QT_DEPLOY_TOOL_RELEASE
    "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/tools/Qt6/bin/windeployqt.exe"
)

add_custom_command(TARGET KeyExtractor POST_BUILD
    COMMAND
        "$<IF:$<CONFIG:Debug>,${QT_DEPLOY_TOOL_DEBUG},${QT_DEPLOY_TOOL_RELEASE}>"
        --no-translations
        "$<TARGET_FILE:KeyExtractor>"
    COMMENT "Deploying Qt runtime files"
    VERBATIM
)
