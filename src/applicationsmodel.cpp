/*
    SPDX-FileCopyrightText: 2026 ToServeTheKing <austin@thebennett.net>

    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "applicationsmodel.h"

#include <KDirWatch>
#include <KLocalizedString>

ApplicationsModel::ApplicationsModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_watch(new KDirWatch(this))
{
    refresh();

    const QStringList dirs = m_installations.appDirsToWatch();
    for (const QString &dir : dirs) {
        m_watch->addDir(dir, KDirWatch::WatchDirOnly);
    }
    connect(m_watch, &KDirWatch::dirty, this, &ApplicationsModel::refresh);
    connect(m_watch, &KDirWatch::created, this, &ApplicationsModel::refresh);
    connect(m_watch, &KDirWatch::deleted, this, &ApplicationsModel::refresh);
}

void ApplicationsModel::refresh()
{
    beginResetModel();
    m_apps.clear();

    AppInfo global;
    global.appId = QStringLiteral("global");
    global.name = i18n("All Applications");
    global.iconSource = QStringLiteral("preferences-desktop-default-applications");
    m_apps.append(global);

    const QStringList ids = m_installations.listApplications();
    for (const QString &id : ids) {
        m_apps.append(m_installations.appInfo(id));
    }

    endResetModel();
    Q_EMIT countChanged();
}

int ApplicationsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_apps.size();
}

QVariant ApplicationsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_apps.size()) {
        return {};
    }
    const AppInfo &app = m_apps.at(index.row());
    switch (role) {
    case AppIdRole:
        return app.appId;
    case NameRole:
        return app.name;
    case IconSourceRole:
        return app.iconSource;
    case IsGlobalRole:
        return app.appId == QStringLiteral("global");
    default:
        return {};
    }
}

QHash<int, QByteArray> ApplicationsModel::roleNames() const
{
    return {
        {AppIdRole, "appId"},
        {NameRole, "name"},
        {IconSourceRole, "iconSource"},
        {IsGlobalRole, "isGlobal"},
    };
}

QVariantMap ApplicationsModel::infoFor(const QString &appId) const
{
    const AppInfo info = m_installations.appInfo(appId);
    return {
        {QStringLiteral("appId"), info.appId},
        {QStringLiteral("name"), info.name},
        {QStringLiteral("iconSource"), info.iconSource},
        {QStringLiteral("version"), info.version},
        {QStringLiteral("runtime"), info.runtime},
    };
}
