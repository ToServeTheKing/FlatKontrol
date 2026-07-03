# SPDX-FileCopyrightText: 2026 ToServeTheKing <austin@thebennett.net>
#
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(resolutiontest
    resolutiontest.cpp
    ../src/keyfile.cpp
    ../src/flatpakinstallations.cpp
    ../src/permissioncatalog.cpp
    ../src/portalsbackend.cpp
    ../src/permissionscontroller.cpp
)

target_include_directories(resolutiontest PRIVATE ../src)

target_link_libraries(resolutiontest PRIVATE
    Qt6::Core
    Qt6::Qml
    Qt6::DBus
    Qt6::Test
    KF6::I18n
)

add_test(NAME resolutiontest COMMAND resolutiontest)
