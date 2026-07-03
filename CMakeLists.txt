# SPDX-License-Identifier: GPL-3.0-or-later
# FlatKontrol - a Kirigami/Qt Flatpak permissions manager

cmake_minimum_required(VERSION 3.16)

project(flatkontrol VERSION 0.2.1 LANGUAGES CXX)

set(REQUIRED_QT_VERSION 6.6.0)
set(REQUIRED_KF_VERSION 6.5.0)

find_package(ECM ${REQUIRED_KF_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMSetupVersion)
include(FeatureSummary)

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
)

ecm_setup_version(${PROJECT_VERSION}
    VARIABLE_PREFIX FLATKONTROL
    VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/flatkontrol-version.h"
)

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()

install(PROGRAMS io.github.toservetheking.FlatKontrol.desktop
        DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES io.github.toservetheking.FlatKontrol.metainfo.xml
        DESTINATION ${KDE_INSTALL_METAINFODIR})
install(FILES icons/io.github.toservetheking.FlatKontrol.svg
        DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)
install(FILES LICENSES/GPL-3.0-or-later.txt
        DESTINATION ${KDE_INSTALL_DATAROOTDIR}/licenses/${PROJECT_NAME})

# Translations: add a po/ directory and re-enable ki18n_install(po) once present.

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
