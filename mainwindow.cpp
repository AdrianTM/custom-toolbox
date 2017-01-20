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

#include <QDebug>

MainWindow::MainWindow(QString arg, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setup();

    file_location = QDir::homePath() + "/.config/custom-toolbox";
    file_name =  QFile(arg).exists() ? arg : getFileName();
    if (QFile(file_name).exists()){
        readFile(file_name);        
    }
    addButtons(category_map);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// find icon file by name
QIcon MainWindow::findIcon(QString icon_name)
{
    // return icon if fully specified
    if (QFile("/" + icon_name).exists()) { // make sure it looks for icon in root, not in home
        return QIcon(icon_name);
    } else {
        icon_name = icon_name.remove(".png");
        icon_name = icon_name.remove(".svg");
        icon_name = icon_name.remove(".xpm");
        // return the icon from the theme if it exists
        if (QIcon::fromTheme(icon_name).name() != "") {
            return QIcon::fromTheme(icon_name);
        // return png, svg, xpm icons from /usr/share/pixmaps
        } else if (QFile("/usr/share/pixmaps/" + icon_name + ".png").exists()) {
            return QIcon("/usr/share/pixmaps/" + icon_name + ".png");
        } else if (QFile("/usr/share/pixmaps/" + icon_name + ".svg").exists()) {
            return QIcon("/usr/share/pixmaps/" + icon_name + ".svg");
        } else if (QFile("/usr/share/pixmaps/" + icon_name + ".xpm").exists()) {
            return QIcon("/usr/share/pixmaps/" + icon_name + ".xpm");
        } else if (QFile("/usr/share/pixmaps/" + icon_name).exists()) {
            return QIcon("/usr/share/pixmaps/" + icon_name);
        } else {
            return QIcon();
        }
    }
}

// setup versious items first time program runs
void MainWindow::setup()
{
    version = getVersion("custom-toolbox");
    this->setWindowTitle(tr("Custom Toolbox"));
    this->adjustSize();

}

void MainWindow::btn_clicked()
{
    this->hide();
    //qDebug() << sender()->objectName();
    system(sender()->objectName().toUtf8());
    this->show();
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
           exit(-1);
       } else {
           return getFileName();
       }
   }
   return file_name;
}

// find the .desktop file for the app name
QString MainWindow::getDesktopFileName(QString app_name)
{
    QString home = QDir::homePath();
    return getCmdOut("find /usr/share/applications " + home + "/.local/share/applications -name " + app_name + ".desktop | tail -1");
}

// return the app info needed for the button
QStringList MainWindow::getDesktopFileInfo(QString file_name, QString category)
{          
    QStringList app_info;

    QString name;
    QString comment;
    QString exec;
    QString icon_name;
    QString terminal;
    QLocale locale;
    QString lang = locale.bcp47Name();

    name = "";
    comment = "";
    if (lang != "en") {
        name = getCmdOut("grep -i ^'Name\\￼Close[" + lang + "\\]=' " + file_name + " | cut -f2 -d=");
        comment = getCmdOut("grep -i ^'Comment\\[" + lang + "\\]=' " + file_name + " | cut -f2 -d=");
    }
    if (lang == "pt" && name == "") { // Brazilian if Portuguese and name empty
        name = getCmdOut("grep -i ^'Name\\[pt_BR]=' " + file_name + " | cut -f2 -d=");
    }
    if (lang == "pt" && comment == "") { // Brazilian if Portuguese and comment empty
        comment = getCmdOut("grep -i ^'Comment\\[pt_BR]=' " + file_name + " | cut -f2 -d=");
    }
    if (name == "") { // backup if Name is not translated
        name = getCmdOut("grep -i ^Name= " + file_name + " | cut -f2 -d=");
        name = name.remove("MX ");
    }
    if (comment == "") { // backup if Comment is not translated
        comment = getCmdOut("grep ^Comment= " + file_name + " | cut -f2 -d=");
    }
    exec = getCmdOut("grep ^Exec= " + file_name + " | cut -f2 -d=");
    icon_name = getCmdOut("grep ^Icon= " + file_name + " | cut -f2 -d=");
    terminal = getCmdOut("grep ^Terminal= " + file_name + " | cut -f2 -d=");

    app_info << name << comment << icon_name << exec << category << terminal.toLower();
    return app_info;
}

// add the buttoms to the window
void MainWindow::addButtons(QMultiMap<QString, QStringList> map)
{
    int col = 0;
    int row = 0;
    int max = 3; // no. max of col
    QString name;
    QString comment;
    QString exec;
    QString icon_name;
    QString terminal;

    foreach (QString category, map.uniqueKeys()) {
        if (!category_map.values(category).isEmpty()) {
            QLabel *label = new QLabel(this);
            QFont font;
            font.setBold(true);
            font.setUnderline(true);
            label->setFont(font);
            label->setText(category);
            col = 0;
            row += 1;
            ui->gridLayout_btn->addWidget(label, row, col);
            ui->gridLayout_btn->setRowStretch(row, 0);
            row += 1;
            foreach (QStringList item, map.values(category)) {
                name = item[0];
                comment = item[1];
                icon_name = item[2];
                exec = item[3];
                terminal = item[4];

                btn = new FlatButton(name);
                btn->setToolTip(comment);
                btn->setAutoDefault(false);
                btn->setIcon(findIcon(icon_name));
                ui->gridLayout_btn->addWidget(btn, row, col);
                 ui->gridLayout_btn->setRowStretch(row, 0);
                col += 1;
                if (col >= max) {
                    col = 0;
                    row += 1;
                }
                //add "x-termial-emulator -e " if terminal = true
                QString cmd = "x-terminal-emulator -e ";
                if (terminal == "true") {
                    btn->setObjectName(cmd + exec); // add the command to be executed to the object name
                } else {
                    btn->setObjectName(exec); // add the command to be executed to the object name
                }
                QObject::connect(btn, SIGNAL(clicked()), this, SLOT(btn_clicked()));
            }
        }
        // add empty row if it's not the last key
        if (category != map.lastKey()) {
            col = 0;
            row += 1;
            QFrame *line = new QFrame();
            line->setFrameShape(QFrame::HLine);
            line->setFrameShadow(QFrame::Sunken);
            ui->gridLayout_btn->addWidget(line, row, col, 1, -1);
            ui->gridLayout_btn->setRowStretch(row, 0);
        }
    }
    ui->gridLayout_btn->setRowStretch(row, 1);
    this->adjustSize();
    this->resize(ui->gridLayout_btn->sizeHint().width() + 90, this->height());
    //qDebug() << "width window" << this->width();
    //qDebug() << "width btn layout area" << ui->gridLayout_btn->sizeHint().width();

}



// process read line
void MainWindow::processLine(QString line)
{
    if (line.startsWith("#") || line == "") { // filter out comment and empty lines
        return;
    }
    QStringList line_list = line.split("=");
    QString key = line_list[0].toLower().trimmed();
    QString value = line_list.size() > 1 ? line_list[1].remove("\"").trimmed() : QString();
    if (key == "name") {
        this->setWindowTitle(value);
    } else if (key == "comment") {
        ui->commentLabel->setText(value);
    } else if (key == "category") {
        categories.append(value);
    } else { // assume it's the name of the app
        QString desktop_file = getDesktopFileName(key);
        if (desktop_file != "") {
            category_map.insert(categories.last(), getDesktopFileInfo(desktop_file, categories.last()));
        }
    }
}

// open the .list file and process it
void MainWindow::readFile(QString file_name)
{
    QFile file(file_name);
    if(!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::critical(0, tr("File Open Error"), tr("Could not open file: ") + file_name + "\n" + tr("Application will close."));
        exit(-1);
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
                       tr("Custom Toolbox is a tool used for creating a custom launcher") +
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

// search
void MainWindow::on_lineSearch_textChanged(const QString &arg1)
{
    // remove all items from the layout
    QLayoutItem *child;
    while ((child = ui->gridLayout_btn->takeAt(0)) != 0) {
        delete child->widget();
        delete child;
    }

    QMultiMap<QString, QStringList> new_map;

    // create a new_map with items that match the search argument
    foreach (QString category, categories) {
        foreach (QStringList item, category_map.values(category)) {
            QString name = item[0];
            QString comment = item[1];
            QString category = item[4];
            if (name.contains(arg1, Qt::CaseInsensitive) || comment.contains(arg1, Qt::CaseInsensitive)
                    || category.contains(arg1, Qt::CaseInsensitive)) {
                new_map.insert(category, item);
            }
        }
    }
    if (!new_map.empty()) {
        arg1 == "" ? addButtons(category_map) : addButtons(new_map);
    }
}
