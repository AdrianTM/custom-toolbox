/**********************************************************************
 *  MainWindow.cpp
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


#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "flatbutton.h"

#include <QFileDialog>
#include <QScrollBar>
#include <QTimer>

#include <QDebug>

MainWindow::MainWindow(QString arg, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setup();

    file_location = QDir::homePath() + "/.config/custom-toolbar";
    file_name =  QFile(arg).exists() ? arg : getFileName();
    readFile(file_name);
    qDebug() << "Categories: " << categories;
    qDebug() <<  "Cat_map: " << category_map;
}

MainWindow::~MainWindow()
{
    delete ui;
}

// setup versious items first time program runs
void MainWindow::setup()
{
    version = getVersion("custom-toolbox");
    this->setWindowTitle(tr("Custom Toolbox"));
    this->adjustSize();

}


// Util function
QString MainWindow::getCmdOut(QString cmd) {
    proc = new QProcess(this);
    proc->start("/bin/bash", QStringList() << "-c" << cmd);
    proc->setReadChannel(QProcess::StandardOutput);
    proc->setReadChannelMode(QProcess::MergedChannels);
    proc->waitForFinished(-1);
    return proc->readAllStandardOutput().trimmed();
}

// select .list file to open
QString MainWindow::getFileName()
{
   QString file_name = QFileDialog::getOpenFileName(this, tr("Open List File"), file_location, tr("List Files (*.list)"));
   if (!QFile(file_name).exists()) {
       int ans = QMessageBox::critical(0, tr("File Open Error"), tr("Could not open file, do you want to try again?"), QMessageBox::Yes, QMessageBox::No);
       if (ans == QMessageBox::No) {
           //QTimer::singleShot(0, this, SLOT(close()));
           exit(-1);
       } else {
           return getFileName();
       }
   }
   return file_name;
}

// process read line
void MainWindow::processLine(QString line)
{
    if (line.startsWith("#") || line == "") { // filter out comment and empty lines
        return;
    }
    QStringList line_list= line.split("=");
    QString name = line_list[0].toLower().trimmed();
    QString content = line_list[1].remove("\"").trimmed();
    if (name == "name") {
        this->setWindowTitle(content);
    } else if (name == "comment") {
        ui->commentLabel->setText(content);
    } else if (name == "category") {
        categories.append(content);
    } else if (name == "item") {
        QStringList item_list = content.split(" ", QString::SkipEmptyParts);
        category_map.insert(categories.last(), item_list);
    }
}

// open the .list file and process it
void MainWindow::readFile(QString file_name)
{
    QFile file(file_name);
    if(!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::critical(0, tr("File Open Error"), tr("Could not open file: ") + file_name + "\n" + tr("Application will close."));
        QTimer::singleShot(0, this, SLOT(close()));
    }
    QTextStream in(&file);
    QString line;
    while(!in.atEnd()) {
        processLine(in.readLine());
    }
    file.close();
}


// Get version of the program
QString MainWindow::getVersion(QString name)
{
    QString cmd_str = QString("dpkg -l %1 | awk 'NR==6 {print $3}'").arg(name);
    return getCmdOut(cmd_str);
}

// About button clicked
void MainWindow::on_buttonAbout_clicked()
{
    this->hide();
    QMessageBox msgBox(QMessageBox::NoIcon,
                       tr("About Custom Toolbox"), "<p align=\"center\"><b><h2>" +
                       tr("Custom Toolbox") + "</h2></b></p><p align=\"center\">" + tr("Version: ") + version + "</p><p align=\"center\"><h3>" +
                       tr("Custom Toolbox is a tool used for creating collections of icons") +
                       "</h3></p><p align=\"center\"><a href=\"http://mxlinux.org\">http://mxlinux.org</a><br /></p><p align=\"center\">" +
                       tr("Copyright (c) MX Linux") + "<br /><br /></p>", 0, this);
    msgBox.addButton(tr("Cancel"), QMessageBox::AcceptRole); // because we want to display the buttons in reverse order we use counter-intuitive roles.
    msgBox.addButton(tr("License"), QMessageBox::RejectRole);
    if (msgBox.exec() == QMessageBox::RejectRole) {
        system("mx-viewer file:///usr/share/doc/custom-toolbox/license.html '" + tr("Custom Toolbox").toUtf8() + " " + tr("License").toUtf8() + "'");
    }
    this->show();
}

// Help button clicked
void MainWindow::on_buttonHelp_clicked()
{
    this->hide();
    QString cmd = QString("mx-viewer https://mxlinux.org/user_manual_mx15/mxum.html#test '%1'").arg(tr("Custom Toolbox"));
    system(cmd.toUtf8());
    this->show();
}
