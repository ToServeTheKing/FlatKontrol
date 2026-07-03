/*
    SPDX-FileCopyrightText: 2026 ToServeTheKing <austin@thebennett.net>

    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "keyfile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

bool KeyFile::load(const QString &path)
{
    m_groupOrder.clear();
    m_groups.clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    QString currentGroup;
    while (!in.atEnd()) {
        QString line = in.readLine();
        const QString trimmed = line.trimmed();

        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
            continue;
        }

        if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))) {
            currentGroup = trimmed.mid(1, trimmed.length() - 2);
            if (!m_groups.contains(currentGroup)) {
                m_groupOrder.append(currentGroup);
                m_groups.insert(currentGroup, Group{});
            }
            continue;
        }

        const int eq = trimmed.indexOf(QLatin1Char('='));
        if (eq < 0 || currentGroup.isEmpty()) {
            continue;
        }

        // Note: flatpak/GKeyFile allow "key[locale]=" but flatpak override files
        // never use locales, so a plain key split is correct here.
        const QString key = trimmed.left(eq).trimmed();
        const QString val = trimmed.mid(eq + 1).trimmed();
        setValue(currentGroup, key, val);
    }

    return true;
}

bool KeyFile::save(const QString &path) const
{
    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    bool firstGroup = true;
    for (const QString &group : m_groupOrder) {
        const Group &g = m_groups.value(group);
        if (g.keyOrder.isEmpty()) {
            continue;
        }
        if (!firstGroup) {
            out << '\n';
        }
        firstGroup = false;
        out << '[' << group << "]\n";
        for (const QString &key : g.keyOrder) {
            out << key << '=' << g.values.value(key) << '\n';
        }
    }

    return file.commit();
}

bool KeyFile::isEmpty() const
{
    for (const QString &group : m_groupOrder) {
        if (!m_groups.value(group).keyOrder.isEmpty()) {
            return false;
        }
    }
    return true;
}

QStringList KeyFile::keys(const QString &group) const
{
    return m_groups.value(group).keyOrder;
}

bool KeyFile::hasKey(const QString &group, const QString &key) const
{
    return m_groups.contains(group) && m_groups.value(group).values.contains(key);
}

QString KeyFile::value(const QString &group, const QString &key, const QString &fallback) const
{
    if (!m_groups.contains(group)) {
        return fallback;
    }
    const Group &g = m_groups.value(group);
    return g.values.value(key, fallback);
}

void KeyFile::setValue(const QString &group, const QString &key, const QString &value)
{
    if (!m_groups.contains(group)) {
        m_groupOrder.append(group);
        m_groups.insert(group, Group{});
    }
    Group &g = m_groups[group];
    if (!g.values.contains(key)) {
        g.keyOrder.append(key);
    }
    g.values[key] = value;
}
