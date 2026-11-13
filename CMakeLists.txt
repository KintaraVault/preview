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
    src/views/imagepage.cpp
)
target_include_directories(preview-ui PUBLIC src src/views)
target_link_libraries(preview-ui PUBLIC preview-core Cutefish::Toolkit Cutefish::Files)
