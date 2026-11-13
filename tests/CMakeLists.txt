# SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

find_package(GTest REQUIRED)
include(GoogleTest)

# The rotation tests write their files with the codecs' own encoders.
pkg_check_modules(TestEncoders REQUIRED IMPORTED_TARGET libjpeg libpng)

add_executable(preview-core-tests
    main.cpp
    core_test.cpp
    rotation_test.cpp
)
target_link_libraries(preview-core-tests PRIVATE preview-ui GTest::gtest PkgConfig::TestEncoders)
target_compile_definitions(preview-core-tests PRIVATE PREVIEW_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
gtest_discover_tests(preview-core-tests)

add_executable(preview-ui-tests
    main.cpp
    pdf_test.cpp
    ui_test.cpp
)
target_link_libraries(preview-ui-tests PRIVATE preview-ui GTest::gtest)
target_compile_definitions(preview-ui-tests PRIVATE PREVIEW_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
gtest_discover_tests(preview-ui-tests)
