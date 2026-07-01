// SPDX-License-Identifier: GPL-3.0-or-later
#include "keyfile.h"
#include "permissionscontroller.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using PC = PermissionsController;

class ResolutionTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString userBase() const
    {
        return m_dir.path() + QStringLiteral("/user");
    }

    void makeApp(const QString &appId, const QString &metadata)
    {
        const QString d = userBase() + QStringLiteral("/app/") + appId + QStringLiteral("/current/active");
        QVERIFY(QDir().mkpath(d));
        QFile f(d + QStringLiteral("/metadata"));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write(metadata.toUtf8());
    }

    void writeOverride(const QString &appId, const QString &content)
    {
        const QString d = userBase() + QStringLiteral("/overrides");
        QVERIFY(QDir().mkpath(d));
        QFile f(d + QLatin1Char('/') + appId);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write(content.toUtf8());
    }

    KeyFile readOverride(const QString &appId)
    {
        KeyFile kf;
        kf.load(userBase() + QStringLiteral("/overrides/") + appId);
        return kf;
    }

    static int findToggle(PC &c, const QString &catId, const QString &option)
    {
        for (int i = 0; i < c.rowCount(); ++i) {
            const QModelIndex idx = c.index(i);
            if (c.data(idx, PC::RowTypeRole).toInt() == PC::Toggle && c.data(idx, PC::CategoryIdRole).toString() == catId
                && c.data(idx, PC::OptionKeyRole).toString() == option) {
                return i;
            }
        }
        return -1;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        qputenv("FLATPAK_USER_DIR", userBase().toUtf8());
        qputenv("FLATPAK_SYSTEM_DIR", (m_dir.path() + QStringLiteral("/system")).toUtf8());
        // Point config at an empty dir so host custom installations don't interfere.
        qputenv("FLATPAK_CONFIG_DIR", (m_dir.path() + QStringLiteral("/config")).toUtf8());
    }

    void toggleRenderAndSave()
    {
        makeApp(QStringLiteral("org.test.Toggle"), QStringLiteral("[Context]\nshared=network;ipc\n"));
        PC c;
        c.setAppId(QStringLiteral("org.test.Toggle"));

        const int net = findToggle(c, QStringLiteral("share"), QStringLiteral("network"));
        QVERIFY(net >= 0);
        QCOMPARE(c.data(c.index(net), PC::ValueRole).toBool(), true);
        QCOMPARE(c.data(c.index(net), PC::StatusRole).toInt(), int(PC::Original));
        QVERIFY(!c.modified());

        c.setToggleValue(net, false);
        QVERIFY(c.modified());
        QCOMPARE(c.data(c.index(net), PC::StatusRole).toInt(), int(PC::User));

        c.save();
        QVERIFY(!c.modified());

        const QStringList shared = readOverride(QStringLiteral("org.test.Toggle")).value(QStringLiteral("Context"), QStringLiteral("shared")).split(QLatin1Char(';'), Qt::SkipEmptyParts);
        QVERIFY(shared.contains(QStringLiteral("!network")));
        QVERIFY(!shared.contains(QStringLiteral("!ipc"))); // unchanged options are not written
    }

    void filesystemsSplitByMode()
    {
        makeApp(QStringLiteral("org.test.Fs"), QStringLiteral("[Context]\nfilesystems=home;~/Docs:ro;xdg-download\n"));
        PC c;
        c.setAppId(QStringLiteral("org.test.Fs"));

        // "home" is a preset toggle, on.
        const int home = findToggle(c, QStringLiteral("filesystems-presets"), QStringLiteral("home"));
        QVERIFY(home >= 0);
        QCOMPARE(c.data(c.index(home), PC::ValueRole).toBool(), true);

        // The non-preset paths become entries split into path + access mode.
        QMap<QString, QString> paths;
        for (int i = 0; i < c.rowCount(); ++i) {
            const QModelIndex idx = c.index(i);
            if (c.data(idx, PC::RowTypeRole).toInt() == PC::PathEntry && c.data(idx, PC::CategoryIdRole).toString() == QStringLiteral("filesystems-other")) {
                paths.insert(c.data(idx, PC::ValueRole).toString(), c.data(idx, PC::SecondaryRole).toString());
            }
        }
        QCOMPARE(paths.value(QStringLiteral("~/Docs")), QStringLiteral("ro"));
        QCOMPARE(paths.value(QStringLiteral("xdg-download")), QStringLiteral("rw"));
    }

    void unknownKeysPreserved()
    {
        makeApp(QStringLiteral("org.test.Unknown"), QStringLiteral("[Context]\nshared=network\n"));
        writeOverride(QStringLiteral("org.test.Unknown"),
                      QStringLiteral("[Context]\nshared=!network\nunknownkey=foo\n\n[Weird Group]\na=b\n"));

        PC c;
        c.setAppId(QStringLiteral("org.test.Unknown"));
        // Loading an existing file must be idempotent (no spurious "modified").
        QVERIFY(!c.modified());

        const int net = findToggle(c, QStringLiteral("share"), QStringLiteral("network"));
        QVERIFY(net >= 0);
        QCOMPARE(c.data(c.index(net), PC::ValueRole).toBool(), false); // overridden off

        c.setToggleValue(net, true); // back to the metadata default -> override removed
        QVERIFY(c.modified());
        c.save();

        const KeyFile kf = readOverride(QStringLiteral("org.test.Unknown"));
        // Keys we do not model must survive the round-trip.
        QCOMPARE(kf.value(QStringLiteral("Context"), QStringLiteral("unknownkey")), QStringLiteral("foo"));
        QCOMPARE(kf.value(QStringLiteral("Weird Group"), QStringLiteral("a")), QStringLiteral("b"));
    }
};

QTEST_GUILESS_MAIN(ResolutionTest)
#include "resolutiontest.moc"
