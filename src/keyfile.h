// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

/**
 * A minimal, GKeyFile/flatpak-compatible INI reader/writer.
 *
 * We deliberately avoid KConfig here: flatpak parses these files with GLib's
 * GKeyFile, and we want byte-for-byte compatible output (no value escaping,
 * spaces in group names like "Session Bus Policy", ';'-separated lists, and
 * keys containing dots such as D-Bus well-known names). KConfig would re-escape
 * and normalise the file in ways flatpak does not expect.
 *
 * Group insertion order and key insertion order are preserved.
 */
class KeyFile
{
public:
    KeyFile() = default;

    /// Parse from disk. Returns false if the file does not exist / cannot be read.
    bool load(const QString &path);
    /// Serialise to disk (creating parent dirs). Returns false on write failure.
    bool save(const QString &path) const;

    bool isEmpty() const;

    QStringList groups() const
    {
        return m_groupOrder;
    }
    bool hasGroup(const QString &group) const
    {
        return m_groups.contains(group);
    }
    QStringList keys(const QString &group) const;
    bool hasKey(const QString &group, const QString &key) const;

    /// Raw value for group/key, or \a fallback when absent.
    QString value(const QString &group, const QString &key, const QString &fallback = QString()) const;

    void setValue(const QString &group, const QString &key, const QString &value);

private:
    struct Group {
        QStringList keyOrder;
        QMap<QString, QString> values;
    };

    QStringList m_groupOrder;
    QMap<QString, Group> m_groups;
};
