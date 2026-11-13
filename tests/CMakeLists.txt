# SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
# SPDX-License-Identifier: BSD-3-Clause

find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(preview-core-tests
    main.cpp
    core_test.cpp
)
target_link_libraries(preview-core-tests PRIVATE preview-ui GTest::gtest)
gtest_discover_tests(preview-core-tests)
