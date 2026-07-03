/*
    SPDX-FileCopyrightText: 2026 ToServeTheKing <austin@thebennett.net>

    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include "flatpakinstallations.h"

#include <QAbstractListModel>
#include <QList>
#include <QQmlEngine>

class KDirWatch;

/**
 * Sidebar model of installed flatpak applications. The first row is the special
 * "All Applications" entry (app id "global") that edits the global override file.
 * Refreshes automatically when applications are installed or removed.
 */
class ApplicationsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        AppIdRole = Qt::UserRole + 1,
        NameRole,
        IconSourceRole,
        IsGlobalRole,
    };

    explicit ApplicationsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Look up the AppInfo for an app id (for the details sheet etc.).
    Q_INVOKABLE QVariantMap infoFor(const QString &appId) const;

public Q_SLOTS:
    void refresh();

Q_SIGNALS:
    void countChanged();

private:
    FlatpakInstallations m_installations;
    QList<AppInfo> m_apps;
    KDirWatch *m_watch = nullptr;
};
