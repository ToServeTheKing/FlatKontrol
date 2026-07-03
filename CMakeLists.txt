# SPDX-FileCopyrightText: 2026 ToServeTheKing <austin@thebennett.net>
#
# SPDX-License-Identifier: GPL-3.0-or-later
# FlatKontrol - a Kirigami/Qt Flatpak permissions manager

cmake_minimum_required(VERSION 3.16)

project(flatkontrol VERSION 0.2.2 LANGUAGES CXX)

set(REQUIRED_QT_VERSION 6.6.0)
set(REQUIRED_KF_VERSION 6.5.0)

find_package(ECM ${REQUIRED_KF_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMSetupVersion)
include(ECMInstallIcons)
include(FeatureSummary)
include(KDEClangFormat)
include(KDEGitCommitHooks)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Qml
    Quick
    QuickControls2
    DBus
    Test
)

find_package(KF6 ${REQUIRED_KF_VERSION} REQUIRED COMPONENTS
    I18n
    CoreAddons
    IconThemes
    Crash
    DBusAddons
    ColorScheme
)

find_package(KF6Kirigami ${REQUIRED_KF_VERSION} REQUIRED)
set_package_properties(KF6Kirigami PROPERTIES
    TYPE REQUIRED
    DESCRIPTION "KDE's next-gen UI framework"
)

find_package(KF6KirigamiAddons REQUIRED)
set_package_properties(KF6KirigamiAddons PROPERTIES
    TYPE REQUIRED
    DESCRIPTION "Add-on components for Kirigami"
)

ecm_setup_version(${PROJECT_VERSION}
    VARIABLE_PREFIX FLATKONTROL
    VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/flatkontrol-version.h"
)

add_subdirectory(icons)
add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()

install(PROGRAMS io.github.toservetheking.FlatKontrol.desktop
        DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES io.github.toservetheking.FlatKontrol.metainfo.xml
        DESTINATION ${KDE_INSTALL_METAINFODIR})
install(FILES LICENSES/GPL-3.0-or-later.txt
        DESTINATION ${KDE_INSTALL_DATAROOTDIR}/licenses/${PROJECT_NAME})

# Translations: add a po/ directory and re-enable ki18n_install(po) once present.

kde_configure_git_pre_commit_hook(CHECKS CLANG-FORMAT)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
