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

#include "about.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "flatbutton.h"
#include "version.h"
#include "unistd.h"

#include <QFileDialog>
#include <QScrollBar>
#include <QSettings>
#include <QTextEdit>

#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MainWindow)
{
    qDebug().noquote() << QCoreApplication::applicationName() << "version:" << VERSION;
    ui->setupUi(this);
    shell = new Cmd;
    setup();

    file_location = "/etc/custom-toolbox";

    QStringList args = qApp->arguments();
    if (args.size() > 1) {
        file_name =  QFile(args[1]).exists() ? args[1] : getFileName();
    } else {
        file_name = getFileName();
    }
    readFile(file_name);
    setGui();

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
        if (!QIcon::fromTheme(icon_name).name().isEmpty()) {
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

// fix varios exec= items to make sure they run correctly
QString MainWindow::fixExecItem(QString item)
{
    item.remove(" %f");  // remove %f if exec expects a file name since it's called without a name
    item.remove(" %F");
    item.remove(" %U");
    return item;
}

// fix name of the item
QString MainWindow::fixNameItem(QString item)
{
   if(item == "System Profiler and Benchmark") {
       item = "System Information";
   }
   return item;
}

// setup versious items and load configurations first time program runs
void MainWindow::setup()
{
    version = getVersion("custom-toolbox");
    this->setWindowTitle(tr("Custom Toolbox"));
    this->adjustSize();

    QSettings settings("/etc/custom-toolbox/custom-toolbox.conf", QSettings::IniFormat);
    hideGUI = settings.value("hideGUI", "false").toBool();
    min_height = settings.value("min_height").toInt();
    min_width = settings.value("min_width").toInt();
    gui_editor = settings.value("gui_editor").toString();
}

// add buttons and resize GUI
void MainWindow::setGui()
{
    // remove all items from the layout
    QLayoutItem *child;
    while ((child = ui->gridLayout_btn->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    addButtons(category_map);
    this->adjustSize();
    this->setMinimumSize(min_height, min_height);
    this->resize(ui->gridLayout_btn->sizeHint().width() + 70, this->height());
    //qDebug() << "width window" << this->width();
    //qDebug() << "width btn layout area" << ui->gridLayout_btn->sizeHint().width();

    // check if .desktop file is in autostart
    QString file_name = QDir::homePath() + "/.config/autostart/" + base_name + ".desktop"; // same base_name as .list file
    if (QFile(file_name).exists()) {
        ui->checkBoxStartup->setChecked(true);
    }
    ui->lineSearch->setFocus();

    this->show();
}

// execute command when button is clicked
void MainWindow::btn_clicked()
{
    QString cmd = sender()->objectName();

    if (hideGUI) {
        this->hide();
        system(cmd.toUtf8());
        this->show();
    } else {
        this->lower();
        shell->run(cmd);
        this->raise();
    }
}


// select .list file to open
QString MainWindow::getFileName()
{
   QString file_name = QFileDialog::getOpenFileName(this, tr("Open List File"), file_location, tr("List Files (*.list)"));
   if (!QFile(file_name).exists()) {
       int ans = QMessageBox::critical(nullptr, tr("File Open Error"), tr("Could not open file, do you want to try again?"), QMessageBox::Yes, QMessageBox::No);
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
    return shell->getCmdOut("find /usr/share/applications " + home + "/.local/share/applications -name " + app_name + ".desktop | tail -1");
}

// return the app info needed for the button
QStringList MainWindow::getDesktopFileInfo(QString file_name)
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
        name = shell->getCmdOut("grep -m1 -i ^'Name\\[" + lang + "\\]=' " + file_name + " | cut -f2 -d=");
        comment = shell->getCmdOut("grep -m1 -i ^'Comment\\[" + lang + "\\]=' " + file_name + " | cut -f2 -d=");
    }
    if (lang == "pt" && name.isEmpty()) { // Brazilian if Portuguese and name empty
        name = shell->getCmdOut("grep -m1 -i ^'Name\\[pt_BR]=' " + file_name + " | cut -f2 -d=");
    }
    if (lang == "pt" && comment.isEmpty()) { // Brazilian if Portuguese and comment empty
        comment = shell->getCmdOut("grep -m1 -i ^'Comment\\[pt_BR]=' " + file_name + " | cut -f2 -d=");
    }
    if (name.isEmpty()) { // backup if Name is not translated
        name = shell->getCmdOut("grep -m1 -i ^Name= " + file_name + " | cut -f2 -d=");
        name = name.remove(QRegExp("^MX ")); // remove MX from begining of the program name (most of the MX Linux apps)
    }
    if (comment.isEmpty()) { // backup if Comment is not translated
        comment = shell->getCmdOut("grep -m1 ^Comment= " + file_name + " | cut -f2 -d=");
    }
    exec = shell->getCmdOut("grep -m1 ^Exec= " + file_name + " | cut -f2 -d=");
    icon_name = shell->getCmdOut("grep -m1 ^Icon= " + file_name + " | cut -f2 -d=");
    terminal = shell->getCmdOut("grep -m1 ^Terminal= " + file_name + " | cut -f2 -d=");

    app_info << name << comment << icon_name << exec << terminal.toLower();
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
    QString root;
    QString terminal;


    for (const QString &category : map.uniqueKeys()) {
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
            for (const QStringList &item : map.values(category)) {
                name = fixNameItem(item[0]);
                comment = item[1];
                icon_name = item[2];
                exec = fixExecItem(item[3]);
                terminal = item[4];
                root = item[5];

                btn = new FlatButton(name);
                btn->setIconSize(40, 40);
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

                if (terminal == "true") {
                    exec = "x-terminal-emulator -e "  + exec;
                }
                if (root == "true") {
                    QString xdg_var = qgetenv("XDG_CURRENT_DESKTOP");
                    QString xdg_str;
                    if (xdg_var.isEmpty()) { // if not available use XFCE
                        xdg_str = "env XDG_CURRENT_DESKTOP=XFCE";
                    } else {
                        xdg_str = "env XDG_CURRENT_DESKTOP=" + xdg_var;
                    }
                    exec = "mx-pkexec '" + xdg_str + " " + exec + "'";
                }
                btn->setObjectName(exec);
                QObject::connect(btn, &QPushButton::clicked, this, &MainWindow::btn_clicked);
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
}



// process read line
void MainWindow::processLine(QString line)
{
    if (line.startsWith("#") || line.isEmpty()) { // filter out comment and empty lines
        return;
    }
    QStringList line_list = line.split("=");
    QString key = line_list[0].trimmed();
    QString value = line_list.size() > 1 ? line_list[1].remove("\"").trimmed() : QString();
    if (key.toLower() == "name") {
        this->setWindowTitle(value);
    } else if (key.toLower() == "comment") {
        ui->commentLabel->setText(value);
    } else if (key.toLower() == "category") {
        categories.append(value);
    } else { // assume it's the name of the app and potentially a "root" flag
        QStringList list = key.split(" ");
        QString desktop_file = getDesktopFileName(list[0]);
        if (!desktop_file.isEmpty()) {
            QStringList info = getDesktopFileInfo(desktop_file);
            if (list.size() > 1) { // check if root flag present
                if (list[1].toLower() == "root") {
                    info << "true";
                } else {
                    info << "false";
                }
            } else {
                info << "false";
            }
            category_map.insert(categories.last(), info);
        }
    }
}

// open the .list file and process it
void MainWindow::readFile(QString file_name)
{
    // reset categories, category_map
    categories.clear();
    category_map.clear();

    QFile file(file_name);
    if (file.exists()){
        base_name = QFileInfo(file_name).baseName();
        file_location= QFileInfo(file_name).path();
        if(!file.open(QFile::ReadOnly | QFile::Text)) {
            QMessageBox::critical(nullptr, tr("File Open Error"), tr("Could not open file: ") + file_name + "\n" + tr("Application will close."));
            exit(-1);
        }
        QTextStream in(&file);
        QString line;
        while(!in.atEnd()) {
            processLine(in.readLine());
        }
        file.close();
    } else {
        exit(-1);
    }
}


// Get version of the program
QString MainWindow::getVersion(QString name)
{
    Cmd cmd;
    return cmd.getCmdOut("dpkg-query -f '${Version}' -W " + name);
}

// About button clicked
void MainWindow::on_buttonAbout_clicked()
{
    this->hide();
    displayAboutMsgBox(tr("About %1").arg(this->windowTitle()), "<p align=\"center\"><b><h2>" + this->windowTitle() +"</h2></b></p><p align=\"center\">" +
                       tr("Version: ") + VERSION + "</p><p align=\"center\"><h3>" +
                       tr("Custom Toolbox is a tool used for creating a custom launcher") +
                       "</h3></p><p align=\"center\"><a href=\"http://mxlinux.org\">http://mxlinux.org</a><br /></p><p align=\"center\">" +
                       tr("Copyright (c) MX Linux") + "<br /><br /></p>",
                       "/usr/share/doc/custom-toolbox/license.html", tr("%1 License").arg(this->windowTitle()), (getuid() == 0));
    this->show();
}

// Help button clicked
void MainWindow::on_buttonHelp_clicked()
{
    QString url = "/usr/share/doc/custom-toolbox/help.html";
    displayDoc(url, tr("%1 Help").arg(this->windowTitle()), (getuid() == 0));
}

// search
void MainWindow::on_lineSearch_textChanged(const QString &arg1)
{
    // remove all items from the layout
    QLayoutItem *child;
    while ((child = ui->gridLayout_btn->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    QMultiMap<QString, QStringList> new_map;

    // create a new_map with items that match the search argument
    for (const QString &category : categories) {
        for (const QStringList &item : category_map.values(category)) {
            QString name = item[0];
            QString comment = item[1];
            if (name.contains(arg1, Qt::CaseInsensitive) || comment.contains(arg1, Qt::CaseInsensitive)
                    || category.contains(arg1, Qt::CaseInsensitive)) {
                new_map.insert(category, item);
            }
        }
    }
    if (!new_map.empty()) {
        arg1.isEmpty() ? addButtons(category_map) : addButtons(new_map);
    }
}

// add a .desktop file to the ~/.config/autostart
void MainWindow::on_checkBoxStartup_clicked(bool checked)
{
    QString file_name = QDir::homePath() + "/.config/autostart/" + base_name + ".desktop"; // same base_name as .list file
    if (checked) {
        QFile file(file_name);
        if(!file.open(QFile::WriteOnly | QFile::Text)) {
            QMessageBox::critical(nullptr, tr("File Open Error"), tr("Could not write file: ") + file_name);
        }
        QTextStream out(&file);
        out << "[Desktop Entry]" << "\n";
        out << "Name=" << this->windowTitle() << "\n";
        out << "Comment=" << ui->commentLabel->text() << "\n";
        out << "Exec=" <<  "custom-toolbox " + file_location + "/" + base_name + ".list" << "\n";
        out << "Terminal=false" << "\n";
        out << "Type=Application" << "\n";
        out << "Icon=custom-toolbox" << "\n";
        out << "Categories=XFCE;System" << "\n";
        out << "StartupNotify=false";
        file.close();
    } else {
        QDir().remove(file_name);
    }
}


// edit launcher .list file
void MainWindow::on_buttonEdit_clicked()
{
    if (!QFile(gui_editor).exists()) {  // if specified editor doesn't exist get the default one
        QString editor = shell->getCmdOut("grep Exec $(locate $(xdg-mime query default text/plain))|cut -d= -f2|cut -d\" \" -f1");
        if (editor.isEmpty() || system("command -v " + editor.toUtf8()) != 0) { // if default one doesn't exit use nano as backup editor
            editor = "x-terminal-emulator -e nano";
        }
        gui_editor = editor;
    }
    this->hide();
    QString xdg_var = qgetenv("XDG_CURRENT_DESKTOP");
    QString xdg_str;
    if (xdg_var.isEmpty()) { // if not available use XFCE
        xdg_str = "env XDG_CURRENT_DESKTOP=XFCE";
    } else {
        xdg_str = "env XDG_CURRENT_DESKTOP=" + xdg_var;
    }
    QString cmd = "mx-pkexec '"+ xdg_str + " " + gui_editor + " " + file_name + "'";
    system(cmd.toUtf8());
    readFile(file_name);
    setGui();
}
