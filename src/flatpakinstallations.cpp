// SPDX-License-Identifier: GPL-3.0-or-later
#include "flatpakinstallations.h"
#include "keyfile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <algorithm>

FlatpakInstallations::FlatpakInstallations()
{
    // User installation.
    m_userPath = envOr("FLATPAK_USER_DIR", QString());
    if (m_userPath.isEmpty()) {
        QString dataHome = qEnvironmentVariable("XDG_DATA_HOME");
        if (dataHome.isEmpty()) {
            dataHome = QDir::homePath() + QStringLiteral("/.local/share");
        }
        m_userPath = dataHome + QStringLiteral("/flatpak");
    }

    // System installation.
    const QString systemPath = envOr("FLATPAK_SYSTEM_DIR", QStringLiteral("/var/lib/flatpak"));

    // Priority: custom (highest priority first), then user, then system.
    m_paths = customInstallationPaths();
    m_paths.prepend(m_userPath);
    m_paths.append(systemPath);
}

QString FlatpakInstallations::envOr(const char *name, const QString &fallback)
{
    const QString v = qEnvironmentVariable(name);
    return v.isEmpty() ? fallback : v;
}

QStringList FlatpakInstallations::customInstallationPaths() const
{
    const QString configPath = envOr("FLATPAK_CONFIG_DIR", QStringLiteral("/etc/flatpak"));
    const QString dirPath = configPath + QStringLiteral("/installations.d");

    QDir dir(dirPath);
    if (!dir.exists()) {
        return {};
    }

    struct Entry {
        QString path;
        int priority;
    };
    QList<Entry> entries;

    const QStringList files = dir.entryList(QDir::Files, QDir::Name);
    for (const QString &f : files) {
        KeyFile kf;
        if (!kf.load(dir.absoluteFilePath(f))) {
            continue;
        }
        for (const QString &group : kf.groups()) {
            if (!kf.hasKey(group, QStringLiteral("Path"))) {
                continue;
            }
            const QString path = kf.value(group, QStringLiteral("Path"));
            const int priority = kf.value(group, QStringLiteral("Priority"), QStringLiteral("0")).toInt();
            entries.append({path, priority});
        }
    }

    std::stable_sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        return a.priority > b.priority;
    });

    QStringList paths;
    for (const Entry &e : entries) {
        paths.append(e.path);
    }
    return paths;
}

QStringList FlatpakInstallations::appDirsToWatch() const
{
    QStringList dirs;
    for (const QString &p : m_paths) {
        dirs.append(p + QStringLiteral("/app"));
    }
    return dirs;
}

QStringList FlatpakInstallations::listApplications() const
{
    QStringList result;
    for (const QString &installation : m_paths) {
        QDir appDir(installation + QStringLiteral("/app"));
        if (!appDir.exists()) {
            continue;
        }
        const QStringList ids = appDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &id : ids) {
            if (id.endsWith(QStringLiteral(".BaseApp"))) {
                continue;
            }
            const QString active = appDir.absoluteFilePath(id) + QStringLiteral("/current/active");
            if (QFileInfo::exists(active) && !result.contains(id)) {
                result.append(id);
            }
        }
    }
    result.sort();
    return result;
}

QString FlatpakInstallations::bundlePath(const QString &appId) const
{
    for (const QString &installation : m_paths) {
        const QString candidate = installation + QStringLiteral("/app/") + appId + QStringLiteral("/current/active");
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return QString();
}

QString FlatpakInstallations::metadataPath(const QString &appId) const
{
    const QString bundle = bundlePath(appId);
    if (bundle.isEmpty()) {
        return QString();
    }
    return bundle + QStringLiteral("/metadata");
}

QString FlatpakInstallations::prettifyAppId(const QString &appId)
{
    const QString last = appId.section(QLatin1Char('.'), -1);
    if (last.isEmpty()) {
        return appId;
    }
    return last.left(1).toUpper() + last.mid(1);
}

QString FlatpakInstallations::readMetainfoName(const QString &bundle, const QString &appId) const
{
    QStringList candidates = {
        bundle + QStringLiteral("/files/share/metainfo/") + appId + QStringLiteral(".metainfo.xml"),
        bundle + QStringLiteral("/files/share/appdata/") + appId + QStringLiteral(".appdata.xml"),
    };

    for (const QString &path : candidates) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        QXmlStreamReader xml(&file);
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QLatin1String("name")) {
                // Skip translated <name xml:lang="..."> entries; take the C locale one.
                if (xml.attributes().hasAttribute(QStringLiteral("xml:lang"))) {
                    continue;
                }
                const QString name = xml.readElementText().trimmed();
                if (!name.isEmpty()) {
                    return name;
                }
            }
        }
    }
    return QString();
}

QString FlatpakInstallations::iconForApp(const QString &appId, const QString &bundle) const
{
    // Resolve the icon name from the exported desktop entry, then look for a
    // matching file under the bundle's exported icons; fall back to the themed name.
    QString iconName = appId;
    const QString desktopPath = bundle + QStringLiteral("/export/share/applications/") + appId + QStringLiteral(".desktop");
    KeyFile desktop;
    if (desktop.load(desktopPath)) {
        const QString fromEntry = desktop.value(QStringLiteral("Desktop Entry"), QStringLiteral("Icon"));
        if (!fromEntry.isEmpty()) {
            iconName = fromEntry;
        }
    }

    // If the icon name is already an absolute path, use it directly.
    if (QFileInfo(iconName).isAbsolute() && QFileInfo::exists(iconName)) {
        return iconName;
    }

    const QString iconsRoot = bundle + QStringLiteral("/export/share/icons/hicolor");
    static const QStringList sizes = {
        QStringLiteral("scalable"),
        QStringLiteral("512x512"),
        QStringLiteral("256x256"),
        QStringLiteral("128x128"),
        QStringLiteral("96x96"),
        QStringLiteral("64x64"),
        QStringLiteral("48x48"),
    };
    static const QStringList exts = {QStringLiteral("svg"), QStringLiteral("png")};
    for (const QString &size : sizes) {
        for (const QString &ext : exts) {
            const QString candidate = iconsRoot + QLatin1Char('/') + size + QStringLiteral("/apps/") + iconName + QLatin1Char('.') + ext;
            if (QFileInfo::exists(candidate)) {
                return candidate;
            }
        }
    }

    return iconName.isEmpty() ? QStringLiteral("application-x-executable") : iconName;
}

AppInfo FlatpakInstallations::appInfo(const QString &appId) const
{
    AppInfo info;
    info.appId = appId;
    info.name = prettifyAppId(appId);
    info.iconSource = QStringLiteral("application-x-executable");
    info.runtime = QStringLiteral("Unknown");
    info.version = QStringLiteral("Unknown");

    const QString bundle = bundlePath(appId);
    if (bundle.isEmpty()) {
        return info;
    }

    const QString name = readMetainfoName(bundle, appId);
    if (!name.isEmpty()) {
        info.name = name;
    }

    info.iconSource = iconForApp(appId, bundle);

    KeyFile metadata;
    if (metadata.load(bundle + QStringLiteral("/metadata"))) {
        const QString rt = metadata.value(QStringLiteral("Application"), QStringLiteral("runtime"));
        if (!rt.isEmpty()) {
            info.runtime = rt;
        }
    }

    return info;
}

QString FlatpakInstallations::overridePath(const QString &appId) const
{
    return m_userPath + QStringLiteral("/overrides/") + appId;
}

QString FlatpakInstallations::globalOverridePath() const
{
    return m_userPath + QStringLiteral("/overrides/global");
}
