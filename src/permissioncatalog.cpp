/*
    SPDX-FileCopyrightText: 2026 ToServeTheKing <austin@thebennett.net>

    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "permissioncatalog.h"

#include <KLocalizedString>

namespace FlatKontrol
{

const QList<CategoryDef> &catalog()
{
    static const QList<CategoryDef> cats = {
        {
            QStringLiteral("share"),
            i18nc("@title permission section", "Share"),
            i18n("Subsystems shared with the host system"),
            QStringLiteral("Context"),
            QStringLiteral("shared"),
            RowType::Toggle,
            {
                {QStringLiteral("network"), i18n("Network"), QStringLiteral("share=network")},
                {QStringLiteral("ipc"), i18n("Inter-process communication"), QStringLiteral("share=ipc")},
            },
        },
        {
            QStringLiteral("sockets"),
            i18nc("@title permission section", "Socket"),
            i18n("Well-known sockets available in the sandbox"),
            QStringLiteral("Context"),
            QStringLiteral("sockets"),
            RowType::Toggle,
            {
                {QStringLiteral("x11"), i18n("X11 windowing system"), QStringLiteral("socket=x11")},
                {QStringLiteral("fallback-x11"), i18n("Fallback to X11 windowing system"), QStringLiteral("socket=fallback-x11")},
                {QStringLiteral("wayland"), i18n("Wayland windowing system"), QStringLiteral("socket=wayland")},
                {QStringLiteral("inherit-wayland-socket"), i18n("Inherited Wayland socket"), QStringLiteral("socket=inherit-wayland-socket")},
                {QStringLiteral("pulseaudio"), i18n("PulseAudio sound server"), QStringLiteral("socket=pulseaudio")},
                {QStringLiteral("session-bus"), i18n("D-Bus session bus"), QStringLiteral("socket=session-bus")},
                {QStringLiteral("system-bus"), i18n("D-Bus system bus"), QStringLiteral("socket=system-bus")},
                {QStringLiteral("ssh-auth"), i18n("Secure Shell agent"), QStringLiteral("socket=ssh-auth")},
                {QStringLiteral("pcsc"), i18n("Smart cards"), QStringLiteral("socket=pcsc")},
                {QStringLiteral("cups"), i18n("Printing system"), QStringLiteral("socket=cups")},
                {QStringLiteral("gpg-agent"), i18n("GPG agent"), QStringLiteral("socket=gpg-agent")},
            },
        },
        {
            QStringLiteral("devices"),
            i18nc("@title permission section", "Device"),
            i18n("Device files available in the sandbox"),
            QStringLiteral("Context"),
            QStringLiteral("devices"),
            RowType::Toggle,
            {
                {QStringLiteral("dri"), i18n("GPU acceleration"), QStringLiteral("device=dri")},
                {QStringLiteral("input"), i18n("Input devices"), QStringLiteral("device=input")},
                {QStringLiteral("kvm"), i18n("Virtualization"), QStringLiteral("device=kvm")},
                {QStringLiteral("shm"), i18n("Shared memory"), QStringLiteral("device=shm")},
                {QStringLiteral("usb"), i18n("USB devices"), QStringLiteral("device=usb")},
                {QStringLiteral("all"), i18n("All devices (e.g. webcam)"), QStringLiteral("device=all")},
            },
        },
        {
            QStringLiteral("features"),
            i18nc("@title permission section", "Features"),
            i18n("Extra features available to the application"),
            QStringLiteral("Context"),
            QStringLiteral("features"),
            RowType::Toggle,
            {
                {QStringLiteral("devel"), i18n("Development syscalls (e.g. ptrace)"), QStringLiteral("feature=devel")},
                {QStringLiteral("multiarch"), i18n("Programs from other architectures"), QStringLiteral("feature=multiarch")},
                {QStringLiteral("bluetooth"), i18n("Bluetooth"), QStringLiteral("feature=bluetooth")},
                {QStringLiteral("canbus"), i18n("Controller Area Network bus"), QStringLiteral("feature=canbus")},
                {QStringLiteral("per-app-dev-shm"), i18n("Application shared memory"), QStringLiteral("feature=per-app-dev-shm")},
            },
        },
        {
            QStringLiteral("filesystems-presets"),
            i18nc("@title permission section", "Filesystem"),
            i18n("Predefined filesystem subsets available to the application"),
            QStringLiteral("Context"),
            QStringLiteral("filesystems"),
            RowType::Toggle,
            {
                {QStringLiteral("host"), i18n("All system files"), QStringLiteral("filesystem=host")},
                {QStringLiteral("host-os"), i18n("All system libraries, executables and static data"), QStringLiteral("filesystem=host-os")},
                {QStringLiteral("host-etc"), i18n("All system configuration"), QStringLiteral("filesystem=host-etc")},
                {QStringLiteral("home"), i18n("All user files"), QStringLiteral("filesystem=home")},
            },
        },
        {
            QStringLiteral("filesystems-other"),
            i18nc("@title permission section", "Other files"),
            i18n("Other filesystem paths available to the application"),
            QStringLiteral("Context"),
            QStringLiteral("filesystems"),
            RowType::PathEntry,
            {},
        },
        {
            QStringLiteral("persistent"),
            i18nc("@title permission section", "Persistent"),
            i18n("Home-relative paths created inside the sandbox"),
            QStringLiteral("Context"),
            QStringLiteral("persistent"),
            RowType::RelativePathEntry,
            {},
        },
        {
            QStringLiteral("environment"),
            i18nc("@title permission section", "Environment"),
            i18n("Variables exported to the application"),
            QStringLiteral("Environment"),
            QString(),
            RowType::VariableEntry,
            {},
        },
        {
            QStringLiteral("session-bus"),
            i18nc("@title permission section", "Session Bus"),
            i18n("Well-known names on the session bus"),
            QStringLiteral("Session Bus Policy"),
            QString(),
            RowType::BusEntry,
            {},
        },
        {
            QStringLiteral("system-bus"),
            i18nc("@title permission section", "System Bus"),
            i18n("Well-known names on the system bus"),
            QStringLiteral("System Bus Policy"),
            QString(),
            RowType::BusEntry,
            {},
        },
        {
            QStringLiteral("portals"),
            i18nc("@title permission section", "Portals"),
            i18n("Resources selectively granted through portals"),
            QString(),
            QString(),
            RowType::Portal,
            {},
        },
    };
    return cats;
}

} // namespace FlatKontrol
