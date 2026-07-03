/*
    SPDX-FileCopyrightText: 2026 ToServeTheKing <austin@thebennett.net>

    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

class QDBusInterface;

/// Tri-state (plus availability) of a portal permission, matching Flatseal.
enum class PortalState {
    Unknown = 0,
    Unsupported = 1,
    Unset = 2,
    Disallowed = 3,
    Allowed = 4,
};

struct PortalDef {
    QString property; // stable id, e.g. "portals-notification"
    QString label;
    QString example;
    QString table; // PermissionStore table, e.g. "notifications"
    QString id; // PermissionStore id, e.g. "notification"
    QStringList allowed;
    QStringList disallowed;
};

/**
 * Talks directly to org.freedesktop.impl.portal.PermissionStore on the session
 * bus to read and edit dynamic portal permissions. Faithful port of Flatseal's
 * portals.js (file-less, D-Bus backed).
 */
class PortalsBackend
{
public:
    PortalsBackend();
    ~PortalsBackend();

    static const QList<PortalDef> &definitions();

    void setAppId(const QString &appId);

    /// (Re)reads supported-state of every portal; call on app change.
    void reload();

    bool isSupported(const PortalDef &def);
    QString unsupportedReason(const PortalDef &def) const;
    PortalState state(const PortalDef &def);

    /// Apply a new state for one portal permission.
    void setState(const PortalDef &def, PortalState newState);

    /// Unset every portal permission for the current app (used by reset).
    void forget();

private:
    void ensureProxy();
    QStringList lookupApps(const QString &table, const QString &id, bool *ok = nullptr);
    void setPermission(const QString &table, const QString &id, const QString &app, const QStringList &perms);
    void deletePermission(const QString &table, const QString &id, const QString &app);
    void unset(const PortalDef &def);
    uint serviceVersion();

    QDBusInterface *m_proxy = nullptr;
    QString m_appId;
    QHash<QString, bool> m_supported; // property -> supported
    QHash<QString, QString> m_reasons; // property -> reason
};
