cmake_minimum_required(VERSION 3.25)

project(KeyExtractor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

qt_standard_project_setup()

qt_add_executable(KeyExtractor
    src/main.cpp
)

target_link_libraries(KeyExtractor
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

set_target_properties(KeyExtractor PROPERTIES
    WIN32_EXECUTABLE TRUE
)
