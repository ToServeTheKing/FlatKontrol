# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(flatkontrol
    main.cpp
    keyfile.cpp
    flatpakinstallations.cpp
    portalsbackend.cpp
    permissioncatalog.cpp
)

qt_add_qml_module(flatkontrol
    URI io.github.toservetheking.FlatKontrol
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        qml/Main.qml
        qml/PermissionsPage.qml
    SOURCES
        applicationsmodel.cpp
        applicationsmodel.h
        permissionscontroller.cpp
        permissionscontroller.h
)

target_include_directories(flatkontrol PRIVATE ${CMAKE_BINARY_DIR})

target_link_libraries(flatkontrol PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::DBus
    KF6::I18n
    KF6::I18nQml
    KF6::CoreAddons
    KF6::IconThemes
    KF6::Crash
    KF6::DBusAddons
)

install(TARGETS flatkontrol ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
