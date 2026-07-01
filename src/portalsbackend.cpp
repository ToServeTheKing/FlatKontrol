// SPDX-License-Identifier: GPL-3.0-or-later
#include "portalsbackend.h"

#include <KLocalizedString>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>

namespace FlatKontrol
{

static constexpr uint SUPPORTED_SERVICE_VERSION = 2;
static const char *DBUS_PATH = "/org/freedesktop/impl/portal/PermissionStore";
static const char *DBUS_IFACE = "org.freedesktop.impl.portal.PermissionStore";

const QList<PortalDef> &PortalsBackend::definitions()
{
    static const QList<PortalDef> defs = {
        {QStringLiteral("portals-background"),
         i18n("Background"),
         i18n("Can run in the background"),
         QStringLiteral("background"),
         QStringLiteral("background"),
         {QStringLiteral("yes")},
         {QStringLiteral("no")}},
        {QStringLiteral("portals-notification"),
         i18n("Notifications"),
         i18n("Can send notifications"),
         QStringLiteral("notifications"),
         QStringLiteral("notification"),
         {QStringLiteral("yes")},
         {QStringLiteral("no")}},
        {QStringLiteral("portals-microphone"),
         i18n("Microphone"),
         i18n("Can listen to your microphone"),
         QStringLiteral("devices"),
         QStringLiteral("microphone"),
         {QStringLiteral("yes")},
         {QStringLiteral("no")}},
        {QStringLiteral("portals-speakers"),
         i18n("Speakers"),
         i18n("Can play sounds to your speakers"),
         QStringLiteral("devices"),
         QStringLiteral("speakers"),
         {QStringLiteral("yes")},
         {QStringLiteral("no")}},
        {QStringLiteral("portals-camera"),
         i18n("Camera"),
         i18n("Can record videos with your camera"),
         QStringLiteral("devices"),
         QStringLiteral("camera"),
         {QStringLiteral("yes")},
         {QStringLiteral("no")}},
        {QStringLiteral("portals-location"),
         i18n("Location"),
         i18n("Can access your location"),
         QStringLiteral("location"),
         QStringLiteral("location"),
         {QStringLiteral("EXACT"), QStringLiteral("0")},
         {QStringLiteral("NONE"), QStringLiteral("0")}},
    };
    return defs;
}

PortalsBackend::PortalsBackend() = default;

PortalsBackend::~PortalsBackend()
{
    delete m_proxy;
}

void PortalsBackend::ensureProxy()
{
    if (m_proxy) {
        return;
    }
    QString busName = qEnvironmentVariable("FLATKONTROL_PORTAL_BUS_NAME");
    if (busName.isEmpty()) {
        busName = qEnvironmentVariable("FLATSEAL_PORTAL_BUS_NAME");
    }
    if (busName.isEmpty()) {
        busName = QString::fromLatin1(DBUS_IFACE);
    }
    m_proxy = new QDBusInterface(busName, QString::fromLatin1(DBUS_PATH), QString::fromLatin1(DBUS_IFACE), QDBusConnection::sessionBus());
}

uint PortalsBackend::serviceVersion()
{
    ensureProxy();
    const QVariant v = m_proxy->property("version");
    return v.isValid() ? v.toUInt() : 0;
}

void PortalsBackend::setAppId(const QString &appId)
{
    m_appId = appId;
    reload();
}

QStringList PortalsBackend::lookupApps(const QString &table, const QString &id, bool *ok)
{
    ensureProxy();
    QDBusMessage reply = m_proxy->call(QStringLiteral("Lookup"), table, id);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        if (ok) {
            *ok = false;
        }
        return {};
    }
    if (ok) {
        *ok = true;
    }

    // First out-arg is a{sas}: app id -> permission strings.
    const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
    QMap<QString, QStringList> perms;
    arg >> perms;

    QStringList apps;
    apps.reserve(perms.size());
    for (auto it = perms.cbegin(); it != perms.cend(); ++it) {
        apps.append(it.key());
    }
    return apps;
}

bool PortalsBackend::isSupported(const PortalDef &def)
{
    if (m_supported.contains(def.property)) {
        return m_supported.value(def.property);
    }

    auto markUnsupported = [&](const QString &reason) {
        m_reasons[def.property] = reason;
        m_supported[def.property] = false;
        return false;
    };

    if (m_appId.isEmpty() || m_appId == QStringLiteral("global")) {
        return markUnsupported(i18n("Not available for global overrides"));
    }

    ensureProxy();
    if (!m_proxy->isValid()) {
        return markUnsupported(i18n("The permission store service is not available"));
    }
    if (serviceVersion() < SUPPORTED_SERVICE_VERSION) {
        return markUnsupported(i18n("Requires permission store version 2 or newer"));
    }

    bool ok = false;
    lookupApps(def.table, def.id, &ok);
    if (!ok) {
        return markUnsupported(i18n("Portal data has not been set up yet"));
    }

    m_reasons[def.property] = QString();
    m_supported[def.property] = true;
    return true;
}

QString PortalsBackend::unsupportedReason(const PortalDef &def) const
{
    return m_reasons.value(def.property);
}

PortalState PortalsBackend::state(const PortalDef &def)
{
    if (!isSupported(def)) {
        return PortalState::Unsupported;
    }

    ensureProxy();
    QDBusMessage reply = m_proxy->call(QStringLiteral("Lookup"), def.table, def.id);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return PortalState::Unsupported;
    }

    const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
    QMap<QString, QStringList> perms;
    arg >> perms;

    if (!perms.contains(m_appId)) {
        return PortalState::Unset;
    }
    const QStringList current = perms.value(m_appId);
    if (!current.isEmpty() && current.first() == def.allowed.first()) {
        return PortalState::Allowed;
    }
    return PortalState::Disallowed;
}

void PortalsBackend::setPermission(const QString &table, const QString &id, const QString &app, const QStringList &perms)
{
    ensureProxy();
    m_proxy->call(QStringLiteral("SetPermission"), table, false, id, app, perms);
}

void PortalsBackend::deletePermission(const QString &table, const QString &id, const QString &app)
{
    ensureProxy();
    m_proxy->call(QStringLiteral("DeletePermission"), table, id, app);
}

void PortalsBackend::unset(const PortalDef &def)
{
    bool ok = false;
    const QStringList apps = lookupApps(def.table, def.id, &ok);
    if (!ok || !apps.contains(m_appId)) {
        return;
    }
    // Work around xdg-desktop-portal#573: deleting the only app drops the table.
    if (apps.size() == 1) {
        setPermission(def.table, def.id, QString(), {});
    }
    deletePermission(def.table, def.id, m_appId);
}

void PortalsBackend::setState(const PortalDef &def, PortalState newState)
{
    if (!isSupported(def)) {
        return;
    }
    switch (newState) {
    case PortalState::Unset:
        unset(def);
        break;
    case PortalState::Allowed:
        setPermission(def.table, def.id, m_appId, def.allowed);
        break;
    case PortalState::Disallowed:
        setPermission(def.table, def.id, m_appId, def.disallowed);
        break;
    default:
        break;
    }
}

void PortalsBackend::forget()
{
    for (const PortalDef &def : definitions()) {
        if (isSupported(def)) {
            unset(def);
        }
    }
}

void PortalsBackend::reload()
{
    m_supported.clear();
    m_reasons.clear();
}

} // namespace FlatKontrol
