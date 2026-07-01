// SPDX-License-Identifier: GPL-3.0-or-later
#include "flatkontrol-version.h"

#include "applicationsmodel.h"
#include "permissionscontroller.h"

#include <KAboutData>
#include <KCrash>
#include <KIconTheme>
#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTextStream>

using namespace Qt::Literals::StringLiterals;

// Filter out a single, well-known benign framework artifact: Qt Quick emits
// "Created graphical object was not placed in the graphics scene" while Kirigami's
// PageRow incubates pages. It fires even for a trivial empty page, is harmless,
// and cannot be avoided from application code. Everything else is passed through.
static QtMessageHandler s_defaultMessageHandler = nullptr;
static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    // Qt Quick emits this while Kirigami's PageRow incubates pages. It fires even
    // for a trivial empty page, is harmless, and cannot be avoided from app code.
    if (message.contains(QLatin1String("was not placed in the graphics scene"))) {
        return;
    }
    // Malformed path data in third-party icon SVGs (system/app icon themes) is not
    // actionable from here; drop the noise rather than spam every render.
    if (context.category && qstrcmp(context.category, "qt.svg") == 0) {
        return;
    }
    // Qt's Wayland integration tries to self-register with xdg-desktop-portal for
    // optional desktop features (global shortcuts, background). It is unrelated to
    // our PermissionStore client use and fires harmlessly on hosts where portal
    // app-info resolution is finicky.
    if (message.contains(QLatin1String("Failed to register with host portal"))) {
        return;
    }
    if (s_defaultMessageHandler) {
        s_defaultMessageHandler(type, context, message);
    }
}

// Headless verification helper: --selftest <app-id> [--write]
static int runSelfTest(const QString &appId, bool doWrite)
{
    QTextStream out(stdout);

    ApplicationsModel apps;
    out << "Applications found: " << apps.rowCount() << Qt::endl;
    for (int i = 0; i < qMin(apps.rowCount(), 6); ++i) {
        const QModelIndex idx = apps.index(i);
        out << "  - " << apps.data(idx, ApplicationsModel::AppIdRole).toString() << " / " << apps.data(idx, ApplicationsModel::NameRole).toString() << Qt::endl;
    }

    PermissionsController c;
    c.setAppId(appId);
    out << "\nLoaded " << appId << " => " << c.appName() << " | version " << c.appVersion() << " | runtime " << c.appRuntime() << Qt::endl;
    out << "Rows: " << c.rowCount() << "  modified: " << c.modified() << Qt::endl;

    int shareNetworkRow = -1;
    for (int i = 0; i < c.rowCount(); ++i) {
        const QModelIndex idx = c.index(i);
        const int kind = c.data(idx, PermissionsController::RowTypeRole).toInt();
        const QString catId = c.data(idx, PermissionsController::CategoryIdRole).toString();
        if (kind == PermissionsController::Toggle) {
            out << "  [" << catId << "] toggle " << c.data(idx, PermissionsController::OptionKeyRole).toString() << " = " << c.data(idx, PermissionsController::ValueRole).toBool()
                << "  status=" << c.data(idx, PermissionsController::StatusRole).toInt() << Qt::endl;
        } else if (kind == PermissionsController::PathEntry || kind == PermissionsController::RelativePathEntry) {
            out << "  [" << catId << "] path '" << c.data(idx, PermissionsController::ValueRole).toString() << "' status=" << c.data(idx, PermissionsController::StatusRole).toInt() << Qt::endl;
        } else if (kind == PermissionsController::VariableEntry || kind == PermissionsController::BusEntry) {
            out << "  [" << catId << "] entry '" << c.data(idx, PermissionsController::ValueRole).toString() << "' = '" << c.data(idx, PermissionsController::SecondaryRole).toString()
                << "' status=" << c.data(idx, PermissionsController::StatusRole).toInt() << Qt::endl;
        }
        if (catId == u"share"_s && c.data(idx, PermissionsController::OptionKeyRole).toString() == u"network"_s) {
            shareNetworkRow = i;
        }
    }

    if (doWrite && shareNetworkRow >= 0) {
        const QString path = QDir::homePath() + u"/.local/share/flatpak/overrides/"_s + appId;
        const bool current = c.data(c.index(shareNetworkRow), PermissionsController::ValueRole).toBool();
        out << "\nToggling share=network from " << current << " to " << !current << Qt::endl;
        c.setToggleValue(shareNetworkRow, !current);
        out << "modified now: " << c.modified() << Qt::endl;
        c.save();
        out << "Saved. modified after save: " << c.modified() << Qt::endl;
        out << "--- override file contents (" << path << ") ---" << Qt::endl;
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            out << QString::fromUtf8(f.readAll()).trimmed() << Qt::endl;
            f.close();
        } else {
            out << "(no file)" << Qt::endl;
        }
        out << "--- resetting (removes overrides) ---" << Qt::endl;
        c.reset();
        out << "After reset, file exists: " << QFile::exists(path) << Qt::endl;
    }

    out.flush();
    return 0;
}

int main(int argc, char *argv[])
{
    s_defaultMessageHandler = qInstallMessageHandler(messageHandler);

    if (argc >= 3 && QString::fromLatin1(argv[1]) == u"--selftest"_s) {
        QCoreApplication app(argc, argv);
        const bool doWrite = QCoreApplication::arguments().contains(u"--write"_s);
        return runSelfTest(QString::fromLatin1(argv[2]), doWrite);
    }

    KIconTheme::initTheme();

    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("flatkontrol"));
    QCoreApplication::setOrganizationName(u"toservetheking"_s);

    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(u"org.kde.desktop"_s);
        QQuickStyle::setFallbackStyle(u"Fusion"_s);
    }

    KAboutData aboutData(u"flatkontrol"_s,
                         i18nc("@title", "FlatKontrol"),
                         QStringLiteral(FLATKONTROL_VERSION_STRING),
                         i18n("Manage Flatpak permissions, the KDE way"),
                         KAboutLicense::GPL_V3,
                         i18n("© 2026 FlatKontrol contributors"));
    aboutData.addAuthor(u"toservetheking"_s, i18nc("@label", "Author"), u"austin@thebennett.net"_s);
    aboutData.setDesktopFileName(u"io.github.toservetheking.FlatKontrol"_s);
    KAboutData::setApplicationData(aboutData);

    QApplication::setWindowIcon(QIcon::fromTheme(u"io.github.toservetheking.FlatKontrol"_s, QIcon::fromTheme(u"preferences-desktop"_s)));

    KCrash::initialize();

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);

    engine.loadFromModule("io.github.toservetheking.FlatKontrol", u"Main"_s);
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
