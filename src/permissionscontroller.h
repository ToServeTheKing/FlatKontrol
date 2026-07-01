// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "flatpakinstallations.h"
#include "keyfile.h"
#include "permissioncatalog.h"
#include "portalsbackend.h"

#include <QAbstractListModel>
#include <QMap>
#include <QQmlEngine>
#include <QSet>
#include <QString>
#include <QVariantList>

/**
 * Owns the editable permission state for one application (or the global
 * pseudo-app) and exposes it as a flat list model of rows grouped by category.
 *
 * The display rows are the single source of truth for the UI; concrete override
 * deltas are recomputed from them on demand (port of Flatseal's per-model
 * original/global/user resolution). Portal permissions are batched and applied
 * on save() alongside the override file.
 */
class PermissionsController : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString appId READ appId WRITE setAppId NOTIFY appIdChanged)
    Q_PROPERTY(QString appName READ appName NOTIFY appIdChanged)
    Q_PROPERTY(QString appIcon READ appIcon NOTIFY appIdChanged)
    Q_PROPERTY(QString appVersion READ appVersion NOTIFY appIdChanged)
    Q_PROPERTY(QString appRuntime READ appRuntime NOTIFY appIdChanged)
    Q_PROPERTY(bool isGlobal READ isGlobal NOTIFY appIdChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY modifiedChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY canUndoChanged)
    // One entry per visible category: {id, title, description, type}.
    Q_PROPERTY(QVariantList categories READ categories NOTIFY appIdChanged)

public:
    enum RowKind {
        Toggle = 0,
        PathEntry = 1,
        RelativePathEntry = 2,
        VariableEntry = 3,
        BusEntry = 4,
        Portal = 5,
        AddRow = 6, // trailing "add entry" affordance for list categories
    };
    Q_ENUM(RowKind)

    enum OverrideStatus {
        Original = 0,
        Global = 1,
        User = 2,
    };
    Q_ENUM(OverrideStatus)

    enum PortalValue {
        PortalUnknown = 0,
        PortalUnsupported = 1,
        PortalUnset = 2,
        PortalDisallowed = 3,
        PortalAllowed = 4,
    };
    Q_ENUM(PortalValue)

    enum Roles {
        CategoryIdRole = Qt::UserRole + 1,
        CategoryTitleRole,
        CategoryDescriptionRole,
        IsFirstInCategoryRole,
        RowTypeRole,
        OptionKeyRole,
        LabelRole,
        ExampleRole,
        ValueRole, // bool (toggle), int (portal), or primary string (entries)
        SecondaryRole, // variable value / bus policy
        StatusRole,
        SupportedRole,
        UnsupportedReasonRole,
        RemovableRole,
    };

    explicit PermissionsController(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString appId() const
    {
        return m_appId;
    }
    void setAppId(const QString &appId);
    QString appName() const
    {
        return m_appName;
    }
    QString appIcon() const
    {
        return m_appIcon;
    }
    QString appVersion() const
    {
        return m_appVersion;
    }
    QString appRuntime() const
    {
        return m_appRuntime;
    }
    bool isGlobal() const
    {
        return m_appId == QStringLiteral("global");
    }
    bool modified() const
    {
        return m_modified;
    }
    bool canUndo() const
    {
        return m_canUndo;
    }
    QVariantList categories() const;

    // --- Editing API, called from QML ---
    Q_INVOKABLE void setToggleValue(int row, bool value);
    Q_INVOKABLE void setEntryPrimary(int row, const QString &text);
    Q_INVOKABLE void setEntrySecondary(int row, const QString &text);
    Q_INVOKABLE void addEntry(const QString &categoryId);
    Q_INVOKABLE void removeEntry(int row);
    Q_INVOKABLE void setPortalValue(int row, int value);

    Q_INVOKABLE void save();
    Q_INVOKABLE void reset();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void reload();

Q_SIGNALS:
    void appIdChanged();
    void modifiedChanged();
    void canUndoChanged();
    void saved();
    void error(const QString &message);

private:
    struct Row {
        int catIndex = -1; // index into FlatKontrol::catalog()
        PermissionsController::RowKind kind = Toggle;
        QString optionKey; // toggle option id
        QString primary; // toggle: option id; entries: path/key/name; portal: property
        QString secondary; // variable value / bus policy ("talk"/"own")
        bool boolValue = false; // toggle state
        int portalValue = PortalUnknown;
        int status = Original;
        bool supported = true;
        QString reason; // unsupported reason
        bool removable = false;
        int entryId = 0; // stable per-row id for entries
    };

    // Read-only baselines for one category (originals from metadata, globals
    // from the global override file).
    struct Baseline {
        QSet<QString> setOriginals; // token sets (toggles, paths, persistent)
        QSet<QString> setGlobals;
        QMap<QString, QString> mapOriginals; // key/value (environment, bus)
        QMap<QString, QString> mapGlobals;
    };

    void rebuild();
    void loadBaselines();
    bool toggleBaseline(const Baseline &b, const QString &option) const;
    static bool fsIsNegated(const QString &t);
    static QString fsNegate(const QString &t);
    static QString fsStripMode(const QString &t);
    static bool fsIsOverridden(const QSet<QString> &set, const QString &value);
    QSet<QString> fsRender(const Baseline &b, const QSet<QString> &overrides) const;

    // Build a KeyFile holding the current override deltas (empty groups omitted).
    KeyFile buildOverrides() const;
    void recomputeStatusesFor(int catIndex);
    void updateModified();
    QList<Row> rowsForCategory(int catIndex) const;

    static bool isFilesystemPreset(const QString &bareToken);

    FlatpakInstallations m_installations;
    FlatKontrol::PortalsBackend m_portals;

    QString m_appId;
    QString m_appName;
    QString m_appIcon;
    QString m_appVersion;
    QString m_appRuntime;

    QList<Baseline> m_baselines; // aligned with catalog()
    QList<Row> m_rows;
    int m_nextEntryId = 1;

    KeyFile m_savedSnapshot; // per-app override file as last persisted
    bool m_modified = false;
    bool m_canUndo = false;

    // Backups captured by reset(), restored by undo().
    KeyFile m_undoSnapshot;
    QMap<QString, int> m_undoPortals; // property -> PortalValue
};
