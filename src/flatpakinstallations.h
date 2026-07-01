// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

/**
 * Plain per-application display/metadata, gathered from the bundle on disk.
 */
struct AppInfo {
    QString appId;
    QString name; // human readable, falls back to a prettified app id
    QString iconSource; // absolute file path or a themed icon name
    QString version;
    QString runtime;
};

/**
 * Locates flatpak installations and applications purely by scanning the
 * filesystem - the same approach Flatseal takes (no libflatpak linkage).
 *
 * Installation priority order mirrors Flatseal: custom installations (from
 * /etc/flatpak/installations.d, highest Priority first), then the user
 * installation, then the system installation.
 */
class FlatpakInstallations
{
public:
    FlatpakInstallations();

    /// Installation roots in priority order (e.g. ~/.local/share/flatpak).
    QStringList installationPaths() const
    {
        return m_paths;
    }

    /// The user installation root - where per-app overrides are written.
    QString userInstallation() const
    {
        return m_userPath;
    }

    /// The "app" directories that should be watched for install/uninstall.
    QStringList appDirsToWatch() const;

    /// Sorted, de-duplicated list of installed application ids (BaseApps filtered out).
    QStringList listApplications() const;

    /// "<installation>/app/<id>/current/active" for the first installation that has it.
    QString bundlePath(const QString &appId) const;

    /// Path to the bundle's "metadata" keyfile (baseline permissions).
    QString metadataPath(const QString &appId) const;

    /// Gather display info (name/icon/version/runtime) for an app.
    AppInfo appInfo(const QString &appId) const;

    /// Path of the user override file for an app id ("global" is the special id).
    QString overridePath(const QString &appId) const;
    /// Path of the global override file.
    QString globalOverridePath() const;

    static QString prettifyAppId(const QString &appId);

private:
    static QString envOr(const char *name, const QString &fallback);
    QStringList customInstallationPaths() const;
    QString iconForApp(const QString &appId, const QString &bundle) const;
    QString readMetainfoName(const QString &bundle, const QString &appId) const;

    QStringList m_paths;
    QString m_userPath;
};
