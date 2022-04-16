/**********************************************************************
 *  about.cpp
 **********************************************************************
 * Copyright (C) 2021 MX Authors
 *
 * Authors: MX Linux <http://mxlinux.org>
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package. If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#include "about.h"
#include "version.h"
#include <unistd.h>

#include <QApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

// display doc as nomal user when run as root
void displayDoc(QString url, QString title)
{
    if (system("command -v mx-viewer >/dev/null") == 0) {
        system("mx-viewer " + url.toUtf8() + " \"" + title.toUtf8() + "\"&");
    } else {
        if (getuid() != 0)
            system("xdg-open " + url.toUtf8());
        else
            system("runuser $(logname) -c \"env XDG_RUNTIME_DIR=/run/user/$(id -u $(logname)) xdg-open " + url.toUtf8() + "\"&");
    }
}

void displayAboutMsgBox(QString title, QString message, QString licence_url, QString license_title)
{
    QMessageBox msgBox(QMessageBox::NoIcon, title, message);
    QPushButton *btnLicense = msgBox.addButton(QApplication::tr("License"), QMessageBox::HelpRole);
    QPushButton *btnChangelog = msgBox.addButton(QApplication::tr("Changelog"), QMessageBox::HelpRole);
    QPushButton *btnCancel = msgBox.addButton(QApplication::tr("Cancel"), QMessageBox::NoRole);
    btnCancel->setIcon(QIcon::fromTheme("window-close"));

    msgBox.exec();

    if (msgBox.clickedButton() == btnLicense) {
        displayDoc(licence_url, license_title);
    } else if (msgBox.clickedButton() == btnChangelog) {
        QDialog *changelog = new QDialog();
        changelog->setWindowTitle(QApplication::tr("Changelog"));
        changelog->resize(600, 500);

        QTextEdit *text = new QTextEdit;
        text->setReadOnly(true);
        QProcess proc;
        proc.start("zless", QStringList{"/usr/share/doc/" + QFileInfo(QCoreApplication::applicationFilePath()).fileName()  + "/changelog.gz"});
        proc.waitForFinished();
        if (proc.exitCode() != 0)
            return;
        text->setText(proc.readAllStandardOutput());

        QPushButton *btnClose = new QPushButton(QApplication::tr("&Close"));
        btnClose->setIcon(QIcon::fromTheme("window-close"));
        QApplication::connect(btnClose, &QPushButton::clicked, changelog, &QDialog::close);

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(text);
        layout->addWidget(btnClose);
        changelog->setLayout(layout);
        changelog->exec();
        delete changelog;
    }
}
