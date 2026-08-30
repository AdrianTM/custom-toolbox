#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

#include "launchermodel.h"

class TestLauncherModel : public QObject
{
    Q_OBJECT

private slots:
    void filtersAndExposesLauncherRoles();
};

void TestLauncherModel::filtersAndExposesLauncherRoles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    qputenv("HOME", directory.path().toUtf8());
    qputenv("XDG_DATA_HOME", (directory.path() + QStringLiteral("/share")).toUtf8());

    const QString applications = directory.path() + QStringLiteral("/share/applications");
    QVERIFY(QDir().mkpath(applications));
    QFile desktopFile(applications + QStringLiteral("/sample.desktop"));
    QVERIFY(desktopFile.open(QFile::WriteOnly | QFile::Text));
    QTextStream desktop(&desktopFile);
    desktop << "[Desktop Entry]\nType=Application\nName=Sample Tool\nComment=A test launcher\n"
               "Icon=applications-utilities\nExec=/bin/true\nTerminal=false\n";
    desktopFile.close();

    const QString listPath = directory.path() + QStringLiteral("/model.list");
    QFile listFile(listPath);
    QVERIFY(listFile.open(QFile::WriteOnly | QFile::Text));
    QTextStream list(&listFile);
    list << "Name=Model Test\nComment=Model description\nCategory=First\n"
            "sample alias 'First Alias'\nCategory=Second\nsample alias 'Second Alias'\n";
    listFile.close();

    QCommandLineParser parser;
    parser.addOption({QStringLiteral("remove-checkbox"), QStringLiteral("test option")});
    LauncherIconProvider iconProvider;
    LauncherModel model(parser, listPath, &iconProvider);

    QCOMPARE(model.title(), QStringLiteral("Model Test"));
    QCOMPARE(model.description(), QStringLiteral("Model description"));
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.categories(), QStringList({QStringLiteral("All launchers"), QStringLiteral("First"),
                                               QStringLiteral("Second")}));
    QCOMPARE(model.data(model.index(0), LauncherModel::NameRole).toString(), QStringLiteral("First Alias"));
    QVERIFY(model.data(model.index(0), LauncherModel::IconSourceRole).toString().startsWith(
        QStringLiteral("image://launchericons/")));

    model.setSearch(QStringLiteral("second"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), LauncherModel::CategoryRole).toString(), QStringLiteral("Second"));

    model.setSearch({});
    model.setSelectedCategory(QStringLiteral("First"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), LauncherModel::NameRole).toString(), QStringLiteral("First Alias"));
}

QTEST_MAIN(TestLauncherModel)
#include "test_launchermodel.moc"
