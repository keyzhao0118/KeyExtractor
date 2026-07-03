cmake_minimum_required(VERSION 3.25)

project(KeyExtractor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(7zip CONFIG REQUIRED)

qt_standard_project_setup()

qt_add_executable(KeyExtractor
    src/main.cpp
    src/core/ArchiveItem.h
    src/core/ArchiveListWorker.cpp
    src/core/ArchiveListWorker.h
    src/engine/InStreamWrapper.cpp
    src/engine/InStreamWrapper.h
    src/engine/SevenZipEngine.cpp
    src/engine/SevenZipEngine.h
    src/engine/SevenZipGuids.cpp
    src/ui/FileTreeModel.cpp
    src/ui/FileTreeModel.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
)
target_include_directories(KeyExtractor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(KeyExtractor
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        7zip::7zip
)


add_custom_command(TARGET KeyExtractor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:7zip::7zip>"
        "$<TARGET_FILE_DIR:KeyExtractor>"
    COMMENT "Copying 7zip.dll to output directory"
    VERBATIM
)
set_target_properties(KeyExtractor PROPERTIES
    WIN32_EXECUTABLE TRUE
)

set(_qt_tools_dir "$ENV{VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/tools/Qt6/bin")
set(_windeployqt_release "${_qt_tools_dir}/windeployqt.exe")
set(_windeployqt_debug "${_qt_tools_dir}/windeployqt.debug.bat")

if(EXISTS "${_windeployqt_release}")
    message(STATUS "windeployqt.exe found: ${_windeployqt_release}")
    if(EXISTS "${_windeployqt_debug}")
        set(_windeployqt "$<IF:$<CONFIG:Debug>,${_windeployqt_debug},${_windeployqt_release}>")
    else()
        set(_windeployqt "${_windeployqt_release}")
    endif()

    add_custom_command(TARGET KeyExtractor POST_BUILD
        COMMAND
            "${_windeployqt}"
            "$<TARGET_FILE:KeyExtractor>"
        COMMENT "Deploying Qt runtime files"
        VERBATIM
    )
else()
    message(WARNING "windeployqt.exe was not found at ${_windeployqt_release}. Install qtbase with the windeployqt feature.")
endif()
