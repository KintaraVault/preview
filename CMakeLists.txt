# SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.16)

project(cutefish-preview VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CTest)

find_package(CutefishToolkit REQUIRED)
find_package(CutefishFiles REQUIRED)

add_library(preview-core STATIC
    src/imagesequence.cpp
    src/previewsettings.cpp
)
target_include_directories(preview-core PUBLIC src)
target_link_libraries(preview-core PUBLIC Cutefish::Toolkit)

add_library(preview-ui STATIC
    src/application.cpp
    src/views/imagepage.cpp
    src/views/previewwindow.cpp
)
target_include_directories(preview-ui PUBLIC src src/views)
target_link_libraries(preview-ui PUBLIC preview-core Cutefish::Toolkit Cutefish::Files)

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE preview-ui)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
