/**********************************************************************
 *  main.cpp
 **********************************************************************
 * Copyright (C) 2017 MX Authors
 *
 * Authors: Adrian
 *          MX Linux <http://mxlinux.org>
 *
 * This file is part of custom-toolbox.
 *
 * custom-toolbox is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * custom-toolbox is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with custom-toolbox.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QProcess>
#include <QTranslator>

#include "mainwindow.h"
#include <unistd.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon::fromTheme(app.applicationName()));
    app.setOrganizationName(QStringLiteral("MX-Linux"));

    QTranslator qtTran;
    if (qtTran.load(QLocale::system(), QStringLiteral("qt"), QStringLiteral("_"), QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTran);

    QTranslator qtBaseTran;
    if (qtBaseTran.load("qtbase_" + QLocale::system().name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtBaseTran);

    QTranslator appTran;
    if (appTran.load(app.applicationName() + "_" + QLocale::system().name(), "/usr/share/" + app.applicationName() + "/locale"))
        app.installTranslator(&appTran);

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("This app can be used to create custom launchers: box of buttons/icons"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({{"r", "remove-checkbox"}, QObject::tr("Don't show 'show this dialog at startup' checkbox")});
    parser.addPositionalArgument(QStringLiteral("file"), QObject::tr("Full path and name of the .list file you want to load to set up the application"));
    parser.process(app);

    // Root guard
    if (QProcess::execute(QStringLiteral("/bin/bash"), {"-c", "logname |grep -q ^root$"}) == 0) {
        QMessageBox::critical(nullptr, QObject::tr("Error"),
                              QObject::tr("You seem to be logged in as root, please log out and log in as normal user to use this program."));
        exit(EXIT_FAILURE);
    }
    if (getuid() == 0) {
        qputenv("XDG_RUNTIME_DIR", "/run/user/0");
        qputenv("HOME", "/root");
    }
    MainWindow w(parser);
    w.show();
    return app.exec();

}
