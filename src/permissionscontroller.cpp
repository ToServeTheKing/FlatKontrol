/*
    SPDX-FileCopyrightText: 2026 ToServeTheKing <austin@thebennett.net>

    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "permissionscontroller.h"

#include <KLocalizedString>

#include <QFile>

using namespace FlatKontrol;

namespace
{
QStringList splitTokens(const QString &value)
{
    QStringList out;
    const QStringList parts = value.split(QLatin1Char(';'));
    for (const QString &p : parts) {
        const QString t = p.trimmed();
        if (!t.isEmpty()) {
            out.append(t);
        }
    }
    return out;
}

QString joinSorted(const QStringList &tokens)
{
    QStringList copy = tokens;
    copy.sort();
    return copy.join(QLatin1Char(';'));
}

// Split a filesystem token like "~/games:ro" into its access mode and path.
// A missing/unknown suffix means read-write (flatpak's default).
bool isFsMode(const QString &s)
{
    return s == QLatin1String("ro") || s == QLatin1String("rw") || s == QLatin1String("create");
}
QString fsModeOf(const QString &token)
{
    const int idx = token.lastIndexOf(QLatin1Char(':'));
    if (idx > 0 && isFsMode(token.mid(idx + 1))) {
        return token.mid(idx + 1);
    }
    return QStringLiteral("rw");
}
QString fsPathOf(const QString &token)
{
    const int idx = token.lastIndexOf(QLatin1Char(':'));
    if (idx > 0 && isFsMode(token.mid(idx + 1))) {
        return token.left(idx);
    }
    return token;
}
QString fsToken(const QString &path, const QString &mode)
{
    return (mode.isEmpty() || mode == QLatin1String("rw")) ? path : path + QLatin1Char(':') + mode;
}

// Normalise a KeyFile to a canonical string so two override sets can be
// compared regardless of group/key/token ordering.
QString normalize(const KeyFile &kf)
{
    QStringList groups = kf.groups();
    groups.sort();
    QString out;
    for (const QString &g : std::as_const(groups)) {
        QStringList keys = kf.keys(g);
        keys.sort();
        for (const QString &k : std::as_const(keys)) {
            const QString v = kf.value(g, k);
            const QString nv = v.contains(QLatin1Char(';')) ? joinSorted(splitTokens(v)) : v;
            out += QLatin1Char('[') + g + QStringLiteral("]\n") + k + QLatin1Char('=') + nv + QLatin1Char('\n');
        }
    }
    return out;
}
} // namespace

PermissionsController::PermissionsController(QObject *parent)
    : QAbstractListModel(parent)
{
}

QVariantList PermissionsController::categories() const
{
    QVariantList list;
    for (const CategoryDef &cat : catalog()) {
        if (cat.type == RowType::Portal && isGlobal()) {
            continue; // portals are not applicable to the global override file
        }
        list.append(QVariantMap{
            {QStringLiteral("id"), cat.id},
            {QStringLiteral("title"), cat.title},
            {QStringLiteral("description"), cat.description},
            {QStringLiteral("type"), static_cast<int>(cat.type)},
        });
    }
    return list;
}

bool PermissionsController::isFilesystemPreset(const QString &bareToken)
{
    static const QSet<QString> presets = {
        QStringLiteral("host"),
        QStringLiteral("host-os"),
        QStringLiteral("host-etc"),
        QStringLiteral("home"),
    };
    return presets.contains(bareToken);
}

bool PermissionsController::fsIsNegated(const QString &t)
{
    return t.startsWith(QLatin1Char('!'));
}

QString PermissionsController::fsNegate(const QString &t)
{
    return fsIsNegated(t) ? t.mid(1) : (QLatin1Char('!') + t);
}

QString PermissionsController::fsStripMode(const QString &t)
{
    return t.section(QLatin1Char(':'), 0, 0);
}

bool PermissionsController::fsIsOverridden(const QSet<QString> &set, const QString &value)
{
    QString path = fsStripMode(value);
    if (path.startsWith(QLatin1Char('!'))) {
        path = path.mid(1);
    }
    static const char *suffixes[] = {"", ":ro", ":rw", ":create"};
    for (const char *s : suffixes) {
        if (set.contains(path + QString::fromLatin1(s)) || set.contains(QLatin1Char('!') + path + QString::fromLatin1(s))) {
            return true;
        }
    }
    return set.contains(QStringLiteral("!") + path + QStringLiteral(":reset"));
}

// --- application selection / loading -----------------------------------------

void PermissionsController::setAppId(const QString &appId)
{
    if (m_appId == appId) {
        return;
    }
    m_appId = appId;

    if (isGlobal()) {
        m_appName = i18n("All Applications");
        m_appIcon = QStringLiteral("preferences-desktop-default-applications");
        m_appVersion.clear();
        m_appRuntime.clear();
    } else {
        const AppInfo info = m_installations.appInfo(appId);
        m_appName = info.name;
        m_appIcon = info.iconSource;
        m_appVersion = info.version;
        m_appRuntime = info.runtime;
    }

    m_portals.setAppId(appId);
    m_canUndo = false;
    Q_EMIT canUndoChanged();
    Q_EMIT appIdChanged();

    reload();
}

void PermissionsController::reload()
{
    beginResetModel();
    loadBaselines();
    m_savedSnapshot = KeyFile();
    m_savedSnapshot.load(m_installations.overridePath(m_appId));
    m_portals.reload();
    rebuild();
    endResetModel();
    updateModified();
}

bool PermissionsController::toggleBaseline(const Baseline &b, const QString &option) const
{
    bool val = false;
    if (b.setOriginals.contains(option)) {
        val = true;
    }
    if (b.setOriginals.contains(QLatin1Char('!') + option)) {
        val = false;
    }
    if (b.setGlobals.contains(option)) {
        val = true;
    }
    if (b.setGlobals.contains(QLatin1Char('!') + option)) {
        val = false;
    }
    return val;
}

void PermissionsController::loadBaselines()
{
    const auto &cats = catalog();
    m_baselines.clear();
    m_baselines.resize(cats.size());

    if (isGlobal()) {
        return; // no per-app baselines for the global override file
    }

    KeyFile metadata;
    metadata.load(m_installations.metadataPath(m_appId));
    KeyFile globals;
    globals.load(m_installations.globalOverridePath());

    for (int i = 0; i < cats.size(); ++i) {
        const CategoryDef &cat = cats.at(i);
        Baseline &b = m_baselines[i];

        if (cat.type == RowType::VariableEntry) {
            for (const QString &k : metadata.keys(cat.group)) {
                const QString v = metadata.value(cat.group, k);
                if (!v.isEmpty()) {
                    b.mapOriginals.insert(k, v);
                }
            }
            for (const QString &k : globals.keys(cat.group)) {
                const QString v = globals.value(cat.group, k);
                if (!v.isEmpty()) {
                    b.mapGlobals.insert(k, v);
                }
            }
        } else if (cat.type == RowType::BusEntry) {
            for (const QString &k : metadata.keys(cat.group)) {
                b.mapOriginals.insert(k, metadata.value(cat.group, k));
            }
            for (const QString &k : globals.keys(cat.group)) {
                b.mapGlobals.insert(k, globals.value(cat.group, k));
            }
        } else if (cat.type == RowType::Portal) {
            continue;
        } else {
            // token categories (toggles, filesystem presets/other, persistent)
            const bool fsPresets = cat.id == QStringLiteral("filesystems-presets");
            const bool fsOther = cat.id == QStringLiteral("filesystems-other");
            const auto route = [&](const QStringList &tokens, QSet<QString> &dest) {
                for (const QString &t : tokens) {
                    if (fsPresets || fsOther) {
                        const QString bare = fsStripMode(t.startsWith(QLatin1Char('!')) ? t.mid(1) : t);
                        const bool preset = isFilesystemPreset(bare);
                        if (fsPresets && !preset) {
                            continue;
                        }
                        if (fsOther && preset) {
                            continue;
                        }
                    }
                    dest.insert(t);
                }
            };
            route(splitTokens(metadata.value(cat.group, cat.key)), b.setOriginals);
            route(splitTokens(globals.value(cat.group, cat.key)), b.setGlobals);
        }
    }
}

QSet<QString> PermissionsController::fsRender(const Baseline &b, const QSet<QString> &overrides) const
{
    const auto isReset = [](const QString &v) {
        const QString path = v.section(QLatin1Char(':'), 0, 0);
        const QString mode = v.section(QLatin1Char(':'), 1, 1);
        return path.startsWith(QLatin1Char('!')) && mode == QStringLiteral("reset");
    };

    QSet<QString> result;
    for (const QString &o : b.setOriginals) {
        if (!fsIsOverridden(b.setGlobals, o) && !fsIsOverridden(overrides, o)) {
            result.insert(o);
        }
    }
    for (const QString &g : b.setGlobals) {
        if (b.setOriginals.contains(fsNegate(g)) || fsIsOverridden(overrides, g)) {
            continue;
        }
        if (fsIsOverridden(b.setOriginals, g) && fsIsNegated(g) && !isReset(g)) {
            continue;
        }
        result.insert(g);
    }
    for (const QString &v : overrides) {
        if (b.setOriginals.contains(fsNegate(v)) || b.setGlobals.contains(fsNegate(v))) {
            continue;
        }
        if (fsIsOverridden(b.setOriginals, v) && fsIsNegated(v) && !isReset(v)) {
            continue;
        }
        if (fsIsOverridden(b.setGlobals, v) && fsIsNegated(v) && !isReset(v)) {
            continue;
        }
        result.insert(v);
    }

    // Only positive grants are shown as rows.
    QSet<QString> positive;
    for (const QString &t : std::as_const(result)) {
        if (!fsIsNegated(t)) {
            positive.insert(t);
        }
    }
    return positive;
}

// --- row construction --------------------------------------------------------

void PermissionsController::rebuild()
{
    const auto &cats = catalog();
    m_rows.clear();
    m_nextEntryId = 1;

    for (int i = 0; i < cats.size(); ++i) {
        const CategoryDef &cat = cats.at(i);
        const Baseline &b = m_baselines.at(i);

        if (cat.type == RowType::Toggle) {
            // Overrides relevant to this key (filesystem categories share a key).
            QSet<QString> ov;
            const QStringList toks = splitTokens(m_savedSnapshot.value(cat.group, cat.key));
            for (const QString &t : toks) {
                const QString bare = fsStripMode(t.startsWith(QLatin1Char('!')) ? t.mid(1) : t);
                if (cat.id == QStringLiteral("filesystems-presets") && !isFilesystemPreset(bare)) {
                    continue;
                }
                ov.insert(t);
            }
            for (const OptionDef &opt : cat.options) {
                Row r;
                r.catIndex = i;
                r.kind = Toggle;
                r.optionKey = opt.option;
                r.primary = opt.label;
                r.boolValue = toggleBaseline(b, opt.option);
                if (ov.contains(opt.option)) {
                    r.boolValue = true;
                }
                if (ov.contains(QLatin1Char('!') + opt.option)) {
                    r.boolValue = false;
                }
                m_rows.append(r);
            }
        } else if (cat.type == RowType::PathEntry) {
            QSet<QString> ov;
            for (const QString &t : splitTokens(m_savedSnapshot.value(cat.group, cat.key))) {
                const QString bare = fsStripMode(t.startsWith(QLatin1Char('!')) ? t.mid(1) : t);
                if (!isFilesystemPreset(bare)) {
                    ov.insert(t);
                }
            }
            QStringList rendered = fsRender(b, ov).values();
            rendered.sort();
            for (const QString &t : std::as_const(rendered)) {
                Row r;
                r.catIndex = i;
                r.kind = PathEntry;
                r.primary = fsPathOf(t);
                r.secondary = fsModeOf(t);
                r.removable = true;
                r.entryId = m_nextEntryId++;
                m_rows.append(r);
            }
        } else if (cat.type == RowType::RelativePathEntry) {
            QSet<QString> baseline = b.setOriginals;
            baseline.unite(b.setGlobals);
            QStringList rendered = baseline.values();
            for (const QString &t : splitTokens(m_savedSnapshot.value(cat.group, cat.key))) {
                if (!rendered.contains(t)) {
                    rendered.append(t);
                }
            }
            rendered.sort();
            for (const QString &t : std::as_const(rendered)) {
                Row r;
                r.catIndex = i;
                r.kind = RelativePathEntry;
                r.primary = t;
                r.removable = !baseline.contains(t);
                r.entryId = m_nextEntryId++;
                m_rows.append(r);
            }
        } else if (cat.type == RowType::VariableEntry) {
            QMap<QString, QString> effective = b.mapOriginals;
            for (auto it = b.mapGlobals.cbegin(); it != b.mapGlobals.cend(); ++it) {
                effective.insert(it.key(), it.value());
            }
            for (const QString &k : m_savedSnapshot.keys(cat.group)) {
                effective.insert(k, m_savedSnapshot.value(cat.group, k));
            }
            for (auto it = effective.cbegin(); it != effective.cend(); ++it) {
                if (it.value().isEmpty()) {
                    continue;
                }
                Row r;
                r.catIndex = i;
                r.kind = VariableEntry;
                r.primary = it.key();
                r.secondary = it.value();
                r.removable = true;
                r.entryId = m_nextEntryId++;
                m_rows.append(r);
            }
        } else if (cat.type == RowType::BusEntry) {
            QMap<QString, QString> effective = b.mapOriginals;
            for (auto it = b.mapGlobals.cbegin(); it != b.mapGlobals.cend(); ++it) {
                effective.insert(it.key(), it.value());
            }
            for (const QString &k : m_savedSnapshot.keys(cat.group)) {
                effective.insert(k, m_savedSnapshot.value(cat.group, k));
            }
            for (auto it = effective.cbegin(); it != effective.cend(); ++it) {
                if (it.value() != QStringLiteral("talk") && it.value() != QStringLiteral("own")) {
                    continue;
                }
                Row r;
                r.catIndex = i;
                r.kind = BusEntry;
                r.primary = it.key();
                r.secondary = it.value();
                r.removable = true;
                r.entryId = m_nextEntryId++;
                m_rows.append(r);
            }
        } else if (cat.type == RowType::Portal) {
            if (isGlobal()) {
                continue; // portals are meaningless for the global override file
            }
            for (const PortalDef &def : PortalsBackend::definitions()) {
                Row r;
                r.catIndex = i;
                r.kind = Portal;
                r.primary = def.property;
                r.supported = m_portals.isSupported(def);
                r.reason = m_portals.unsupportedReason(def);
                r.portalValue = static_cast<int>(m_portals.state(def));
                m_rows.append(r);
            }
        }

        // Trailing "add" affordance for the editable list categories.
        if (cat.type == RowType::PathEntry || cat.type == RowType::RelativePathEntry || cat.type == RowType::VariableEntry
            || cat.type == RowType::BusEntry) {
            Row r;
            r.catIndex = i;
            r.kind = AddRow;
            m_rows.append(r);
        }
    }

    for (int i = 0; i < m_baselines.size(); ++i) {
        recomputeStatusesFor(i);
    }
}

// --- model interface ---------------------------------------------------------

int PermissionsController::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant PermissionsController::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }
    const Row &r = m_rows.at(index.row());
    const CategoryDef &cat = catalog().at(r.catIndex);

    switch (role) {
    case CategoryIdRole:
        return cat.id;
    case CategoryTitleRole:
        return cat.title;
    case CategoryDescriptionRole:
        return cat.description;
    case IsFirstInCategoryRole:
        return index.row() == 0 || m_rows.at(index.row() - 1).catIndex != r.catIndex;
    case RowTypeRole:
        return static_cast<int>(r.kind);
    case OptionKeyRole:
        return r.optionKey;
    case LabelRole:
        if (r.kind == Toggle) {
            return r.primary;
        }
        if (r.kind == Portal) {
            for (const PortalDef &def : PortalsBackend::definitions()) {
                if (def.property == r.primary) {
                    return def.label;
                }
            }
        }
        return r.primary;
    case ExampleRole:
        if (r.kind == Toggle) {
            for (const OptionDef &opt : cat.options) {
                if (opt.option == r.optionKey) {
                    return opt.example;
                }
            }
        }
        if (r.kind == Portal) {
            for (const PortalDef &def : PortalsBackend::definitions()) {
                if (def.property == r.primary) {
                    return def.example;
                }
            }
        }
        return QString();
    case ValueRole:
        if (r.kind == Toggle) {
            return r.boolValue;
        }
        if (r.kind == Portal) {
            return r.portalValue;
        }
        return r.primary;
    case SecondaryRole:
        return r.secondary;
    case StatusRole:
        return r.status;
    case SupportedRole:
        return r.supported;
    case UnsupportedReasonRole:
        return r.reason;
    case RemovableRole:
        return r.removable;
    default:
        return {};
    }
}

QHash<int, QByteArray> PermissionsController::roleNames() const
{
    return {
        {CategoryIdRole, "categoryId"},
        {CategoryTitleRole, "categoryTitle"},
        {CategoryDescriptionRole, "categoryDescription"},
        {IsFirstInCategoryRole, "isFirstInCategory"},
        {RowTypeRole, "rowType"},
        {OptionKeyRole, "optionKey"},
        {LabelRole, "label"},
        {ExampleRole, "example"},
        {ValueRole, "value"},
        {SecondaryRole, "secondary"},
        {StatusRole, "status"},
        {SupportedRole, "supported"},
        {UnsupportedReasonRole, "unsupportedReason"},
        {RemovableRole, "removable"},
    };
}

// --- editing -----------------------------------------------------------------

void PermissionsController::setToggleValue(int row, bool value)
{
    if (row < 0 || row >= m_rows.size() || m_rows[row].kind != Toggle) {
        return;
    }
    m_rows[row].boolValue = value;
    recomputeStatusesFor(m_rows[row].catIndex);
    const QModelIndex idx = index(row);
    Q_EMIT dataChanged(idx, idx, {ValueRole, StatusRole});
    updateModified();
}

void PermissionsController::setEntryPrimary(int row, const QString &text)
{
    if (row < 0 || row >= m_rows.size()) {
        return;
    }
    m_rows[row].primary = text;
    recomputeStatusesFor(m_rows[row].catIndex);
    const QModelIndex idx = index(row);
    Q_EMIT dataChanged(idx, idx, {StatusRole});
    updateModified();
}

void PermissionsController::setEntrySecondary(int row, const QString &text)
{
    if (row < 0 || row >= m_rows.size()) {
        return;
    }
    m_rows[row].secondary = text;
    recomputeStatusesFor(m_rows[row].catIndex);
    const QModelIndex idx = index(row);
    Q_EMIT dataChanged(idx, idx, {SecondaryRole, StatusRole});
    updateModified();
}

void PermissionsController::addEntry(const QString &categoryId)
{
    const auto &cats = catalog();
    int catIndex = -1;
    for (int i = 0; i < cats.size(); ++i) {
        if (cats.at(i).id == categoryId) {
            catIndex = i;
            break;
        }
    }
    if (catIndex < 0) {
        return;
    }

    // Insert just before the category's trailing "add" row so the new entry
    // lands at the end of the right section.
    int insertAt = m_rows.size();
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).catIndex == catIndex && m_rows.at(i).kind == AddRow) {
            insertAt = i;
            break;
        }
    }

    Row r;
    r.catIndex = catIndex;
    switch (cats.at(catIndex).type) {
    case RowType::PathEntry:
        r.kind = PathEntry;
        break;
    case RowType::RelativePathEntry:
        r.kind = RelativePathEntry;
        break;
    case RowType::VariableEntry:
        r.kind = VariableEntry;
        break;
    case RowType::BusEntry:
        r.kind = BusEntry;
        r.secondary = QStringLiteral("talk");
        break;
    default:
        return;
    }
    r.removable = true;
    r.status = User;
    r.entryId = m_nextEntryId++;

    beginInsertRows(QModelIndex(), insertAt, insertAt);
    m_rows.insert(insertAt, r);
    endInsertRows();
    updateModified();
}

void PermissionsController::removeEntry(int row)
{
    if (row < 0 || row >= m_rows.size() || !m_rows.at(row).removable) {
        return;
    }
    const int catIndex = m_rows.at(row).catIndex;
    beginRemoveRows(QModelIndex(), row, row);
    m_rows.removeAt(row);
    endRemoveRows();
    recomputeStatusesFor(catIndex);
    updateModified();
}

void PermissionsController::setPortalValue(int row, int value)
{
    if (row < 0 || row >= m_rows.size() || m_rows[row].kind != Portal) {
        return;
    }
    m_rows[row].portalValue = value;
    const QModelIndex idx = index(row);
    Q_EMIT dataChanged(idx, idx, {ValueRole});
    updateModified();
}

// --- status & modified -------------------------------------------------------

void PermissionsController::recomputeStatusesFor(int catIndex)
{
    const Baseline &b = m_baselines.at(catIndex);

    for (int i = 0; i < m_rows.size(); ++i) {
        Row &r = m_rows[i];
        if (r.catIndex != catIndex) {
            continue;
        }
        int status = Original;
        switch (r.kind) {
        case Toggle: {
            const bool base = toggleBaseline(b, r.optionKey);
            if (r.boolValue != base) {
                status = User;
            } else if (b.setGlobals.contains(r.optionKey) || b.setGlobals.contains(QLatin1Char('!') + r.optionKey)) {
                status = Global;
            }
            break;
        }
        case PathEntry: {
            if (!b.setOriginals.contains(r.primary) && !b.setGlobals.contains(r.primary)) {
                status = User;
            } else if (b.setGlobals.contains(r.primary)) {
                status = Global;
            }
            break;
        }
        case RelativePathEntry: {
            if (!b.setOriginals.contains(r.primary) && !b.setGlobals.contains(r.primary)) {
                status = User;
            } else if (b.setGlobals.contains(r.primary)) {
                status = Global;
            }
            break;
        }
        case VariableEntry: {
            const bool inBase = b.mapOriginals.contains(r.primary) || b.mapGlobals.contains(r.primary);
            const QString baseVal = b.mapGlobals.contains(r.primary) ? b.mapGlobals.value(r.primary) : b.mapOriginals.value(r.primary);
            if (!inBase || baseVal != r.secondary) {
                status = User;
            } else if (b.mapGlobals.contains(r.primary)) {
                status = Global;
            }
            break;
        }
        case BusEntry: {
            const bool inBase = b.mapOriginals.contains(r.primary) || b.mapGlobals.contains(r.primary);
            const QString baseVal = b.mapGlobals.contains(r.primary) ? b.mapGlobals.value(r.primary) : b.mapOriginals.value(r.primary);
            if (!inBase || baseVal != r.secondary) {
                status = User;
            } else if (b.mapGlobals.contains(r.primary)) {
                status = Global;
            }
            break;
        }
        case Portal:
        case AddRow:
            continue;
        }
        if (r.status != status) {
            r.status = status;
            const QModelIndex idx = index(i);
            Q_EMIT dataChanged(idx, idx, {StatusRole});
        }
    }
}

KeyFile PermissionsController::buildOverrides() const
{
    const auto &cats = catalog();
    KeyFile kf;

    // Accumulate ';'-list override tokens per (group,key) so the shared
    // "filesystems" key receives both presets and custom paths.
    QMap<QString, QStringList> listOverrides; // "group\x1fkey" -> tokens
    const auto pushToken = [&](const QString &group, const QString &key, const QString &token) {
        listOverrides[group + QChar(0x1f) + key].append(token);
    };

    for (int i = 0; i < cats.size(); ++i) {
        const CategoryDef &cat = cats.at(i);
        const Baseline &b = m_baselines.at(i);

        if (cat.type == RowType::Toggle) {
            for (const Row &r : m_rows) {
                if (r.catIndex != i) {
                    continue;
                }
                const bool base = toggleBaseline(b, r.optionKey);
                if (r.boolValue != base) {
                    pushToken(cat.group, cat.key, r.boolValue ? r.optionKey : (QLatin1Char('!') + r.optionKey));
                }
            }
        } else if (cat.type == RowType::PathEntry) {
            QSet<QString> desired;
            for (const Row &r : m_rows) {
                if (r.catIndex == i && !r.primary.trimmed().isEmpty()) {
                    desired.insert(fsToken(r.primary.trimmed(), r.secondary));
                }
            }
            // Port of filesystemsOther.updateFromProxyProperty (presets separated).
            QSet<QString> added;
            for (const QString &p : std::as_const(desired)) {
                if (!b.setOriginals.contains(p) && !b.setGlobals.contains(p)) {
                    added.insert(p);
                }
            }
            QSet<QString> overrides = added;
            for (const QString &o : b.setOriginals) {
                if (!fsIsOverridden(b.setGlobals, o) && !fsIsOverridden(added, o) && !desired.contains(o)) {
                    overrides.insert(fsNegate(fsStripMode(o)));
                }
            }
            for (const QString &g : b.setGlobals) {
                if (!fsIsOverridden(b.setOriginals, g) && !fsIsOverridden(added, g) && !desired.contains(g)) {
                    overrides.insert(fsNegate(fsStripMode(g)));
                }
            }
            for (const QString &t : std::as_const(overrides)) {
                pushToken(cat.group, cat.key, t);
            }
        } else if (cat.type == RowType::RelativePathEntry) {
            QSet<QString> baseline = b.setOriginals;
            baseline.unite(b.setGlobals);
            for (const Row &r : m_rows) {
                if (r.catIndex != i) {
                    continue;
                }
                const QString t = r.primary.trimmed();
                if (!t.isEmpty() && !baseline.contains(t)) {
                    pushToken(cat.group, cat.key, t);
                }
            }
        } else if (cat.type == RowType::VariableEntry) {
            QMap<QString, QString> desired;
            for (const Row &r : m_rows) {
                if (r.catIndex != i) {
                    continue;
                }
                const QString k = r.primary.trimmed();
                if (k.isEmpty() || k.contains(QLatin1Char(';')) || k.contains(QLatin1Char(' ')) || r.secondary.isEmpty()) {
                    continue;
                }
                desired.insert(k, r.secondary);
            }
            QMap<QString, QString> baseline = b.mapOriginals;
            for (auto it = b.mapGlobals.cbegin(); it != b.mapGlobals.cend(); ++it) {
                baseline.insert(it.key(), it.value());
            }
            for (auto it = desired.cbegin(); it != desired.cend(); ++it) {
                if (!baseline.contains(it.key()) || baseline.value(it.key()) != it.value()) {
                    kf.setValue(cat.group, it.key(), it.value());
                }
            }
            for (auto it = baseline.cbegin(); it != baseline.cend(); ++it) {
                if (!desired.contains(it.key()) && !it.value().isEmpty()) {
                    kf.setValue(cat.group, it.key(), QString());
                }
            }
        } else if (cat.type == RowType::BusEntry) {
            QMap<QString, QString> desired;
            for (const Row &r : m_rows) {
                if (r.catIndex != i) {
                    continue;
                }
                const QString name = r.primary.trimmed();
                if (!name.isEmpty() && (r.secondary == QStringLiteral("talk") || r.secondary == QStringLiteral("own"))) {
                    desired.insert(name, r.secondary);
                }
            }
            QMap<QString, QString> baseline = b.mapOriginals;
            for (auto it = b.mapGlobals.cbegin(); it != b.mapGlobals.cend(); ++it) {
                baseline.insert(it.key(), it.value());
            }
            for (auto it = desired.cbegin(); it != desired.cend(); ++it) {
                if (!baseline.contains(it.key()) || baseline.value(it.key()) != it.value()) {
                    kf.setValue(cat.group, it.key(), it.value());
                }
            }
            for (auto it = baseline.cbegin(); it != baseline.cend(); ++it) {
                const QString v = it.value();
                if ((v == QStringLiteral("talk") || v == QStringLiteral("own")) && !desired.contains(it.key())) {
                    kf.setValue(cat.group, it.key(), QStringLiteral("none"));
                }
            }
        }
    }

    for (auto it = listOverrides.cbegin(); it != listOverrides.cend(); ++it) {
        if (it.value().isEmpty()) {
            continue;
        }
        const QString combined = it.key();
        const int sep = combined.indexOf(QChar(0x1f));
        const QString group = combined.left(sep);
        const QString key = combined.mid(sep + 1);
        QStringList tokens = it.value();
        tokens.removeDuplicates();
        kf.setValue(group, key, tokens.join(QLatin1Char(';')));
    }

    // Preserve any keys we do not model (e.g. newer flatpak options, or entries
    // written by other tools) so saving never drops data. We claim the six
    // Context keys and the whole Environment / bus-policy groups; everything else
    // is copied through verbatim from what was on disk.
    static const QSet<QString> claimedContextKeys = {
        QStringLiteral("shared"),
        QStringLiteral("sockets"),
        QStringLiteral("devices"),
        QStringLiteral("features"),
        QStringLiteral("filesystems"),
        QStringLiteral("persistent"),
    };
    static const QSet<QString> claimedGroups = {
        QStringLiteral("Environment"),
        QStringLiteral("Session Bus Policy"),
        QStringLiteral("System Bus Policy"),
    };
    for (const QString &group : m_savedSnapshot.groups()) {
        const bool wholeGroupClaimed = claimedGroups.contains(group);
        for (const QString &key : m_savedSnapshot.keys(group)) {
            const bool claimed = wholeGroupClaimed || (group == QStringLiteral("Context") && claimedContextKeys.contains(key));
            if (!claimed) {
                kf.setValue(group, key, m_savedSnapshot.value(group, key));
            }
        }
    }

    return kf;
}

void PermissionsController::updateModified()
{
    bool mod = normalize(buildOverrides()) != normalize(m_savedSnapshot);

    if (!mod) {
        for (const Row &r : m_rows) {
            if (r.kind != Portal || !r.supported) {
                continue;
            }
            for (const PortalDef &def : PortalsBackend::definitions()) {
                if (def.property != r.primary) {
                    continue;
                }
                const int actual = static_cast<int>(m_portals.state(def));
                if (r.portalValue != actual && r.portalValue != PortalUnknown && r.portalValue != PortalUnsupported) {
                    mod = true;
                }
            }
            if (mod) {
                break;
            }
        }
    }

    if (mod != m_modified) {
        m_modified = mod;
        Q_EMIT modifiedChanged();
    }
}

// --- persistence -------------------------------------------------------------

void PermissionsController::save()
{
    const QString path = m_installations.overridePath(m_appId);
    const KeyFile overrides = buildOverrides();

    if (overrides.isEmpty()) {
        QFile::remove(path);
    } else if (!overrides.save(path)) {
        Q_EMIT error(i18n("Could not write override file: %1", path));
        return;
    }

    // Apply batched portal changes.
    for (const Row &r : m_rows) {
        if (r.kind != Portal || !r.supported) {
            continue;
        }
        for (const PortalDef &def : PortalsBackend::definitions()) {
            if (def.property != r.primary) {
                continue;
            }
            const int actual = static_cast<int>(m_portals.state(def));
            if (r.portalValue != actual && r.portalValue != PortalUnknown && r.portalValue != PortalUnsupported) {
                m_portals.setState(def, static_cast<PortalState>(r.portalValue));
            }
        }
    }

    Q_EMIT saved();
    reload();
}

void PermissionsController::reset()
{
    const QString path = m_installations.overridePath(m_appId);

    // Capture undo state.
    m_undoSnapshot = KeyFile();
    m_undoSnapshot.load(path);
    m_undoPortals.clear();
    for (const PortalDef &def : PortalsBackend::definitions()) {
        if (m_portals.isSupported(def)) {
            m_undoPortals.insert(def.property, static_cast<int>(m_portals.state(def)));
        }
    }

    QFile::remove(path);
    m_portals.forget();

    m_canUndo = true;
    Q_EMIT canUndoChanged();
    reload();
}

void PermissionsController::undo()
{
    const QString path = m_installations.overridePath(m_appId);

    if (m_undoSnapshot.isEmpty()) {
        QFile::remove(path);
    } else {
        m_undoSnapshot.save(path);
    }
    for (auto it = m_undoPortals.cbegin(); it != m_undoPortals.cend(); ++it) {
        for (const PortalDef &def : PortalsBackend::definitions()) {
            if (def.property == it.key()) {
                m_portals.setState(def, static_cast<PortalState>(it.value()));
            }
        }
    }

    m_canUndo = false;
    Q_EMIT canUndoChanged();
    reload();
}
