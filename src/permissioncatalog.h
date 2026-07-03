/*
    SPDX-FileCopyrightText: 2026 ToServeTheKing <austin@thebennett.net>

    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QList>
#include <QString>

namespace FlatKontrol
{

/// Where a permission's current value comes from (drives the status badge).
enum class Status {
    Original = 0, // baseline, from the app's metadata
    Global = 1, // from the global override file
    User = 2, // from the per-app override file
};

/// The kind of delegate a permission row should be rendered with.
enum class RowType {
    Toggle = 0, // boolean switch (share/sockets/devices/features/filesystem presets)
    PathEntry = 1, // free-form filesystem path (with optional :ro/:rw/:create)
    RelativePathEntry = 2, // home-relative persistent path
    VariableEntry = 3, // KEY + VALUE environment variable
    BusEntry = 4, // D-Bus name + talk/own policy
    Portal = 5, // tri-state portal permission (unset/allowed/disallowed)
};

/// A single fixed toggle within a category (e.g. "network" within Share).
struct OptionDef {
    QString option;
    QString label;
    QString example;
};

/// A category groups related permissions under one FormCard section.
struct CategoryDef {
    QString id; // stable id, e.g. "share"
    QString title; // section header, e.g. "Share"
    QString description;
    QString group; // keyfile group, e.g. "Context"
    QString key; // keyfile key, e.g. "shared" (empty for key-per-entry groups)
    RowType type;
    QList<OptionDef> options; // populated only for Toggle categories
};

/// The full, ordered catalog of categories FlatKontrol presents.
const QList<CategoryDef> &catalog();

/// Convenience: the toggle category ids, in catalog order.
inline QString tr_noop(const char *s)
{
    return QString::fromUtf8(s);
}

} // namespace FlatKontrol
