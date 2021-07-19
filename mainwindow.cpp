 /**********************************************************************
 *  MainWindow.cpp
 **********************************************************************
 * Copyright (C) 2017-2021 MX Authors
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

#include <QDebug>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QTextEdit>

#include <unistd.h>

#include "ui_mainwindow.h"
#include "about.h"
#include "flatbutton.h"
#include "mainwindow.h"
#include "version.h"

MainWindow::MainWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MainWindow)
{
    qDebug().noquote() << QCoreApplication::applicationName() << "version:" << VERSION;
    ui->setupUi(this);
    setWindowFlags(Qt::Window); // for the close, min and max buttons
    shell = new Cmd;
    local_dir = QFile::exists(QDir::homePath() + "/.local/share/applications")
              ? QDir::homePath() + "/.local/share/applications " : " ";
    setup();

    file_location = "/etc/custom-toolbox";

    QStringList args = qApp->arguments();
    if (args.size() > 1)
        file_name =  QFile(args.at(1)).exists() ? args.at(1) : getFileName();
    else
        file_name = getFileName();
    readFile(file_name);
    setGui();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// find icon file by name
QString MainWindow::findIcon(QString icon_name)
{
    if (icon_name.isEmpty())
        return QString();
    if (QFileInfo::exists("/" + icon_name))
        return icon_name;

    QString search_term = icon_name;
    if (!icon_name.endsWith(".png") && !icon_name.endsWith(".svg") && !icon_name.endsWith(".xpm"))
        search_term = icon_name + ".*";

    icon_name.remove(QRegularExpression("\\.png$|\\.svg$|\\.xpm$"));

    // Try to find in most obvious places
    QStringList search_paths { QDir::homePath() + "/.local/share/icons/",
                               "/usr/share/icons/" + QIcon::themeName() + "/48x48/apps/",
                               "/usr/share/icons/" + QIcon::themeName() + "/48x48/",
                               "/usr/share/icons/" + QIcon::themeName(),
                               "/usr/share/pixmaps/",
                               "/usr/local/share/icons/",
                               "/usr/share/icons/hicolor/48x48/apps/"};
    for (const QString &path : search_paths) {
        if (!QFileInfo::exists(path)) {
            search_paths.removeOne(path);
            continue;
        }
        for (const QString &ext : {".png", ".svg", ".xpm"} ) {
            QString file = path + icon_name + ext;
            if (QFileInfo::exists(file))
                return file;
        }
    }

    // Search recursive
    search_paths.append("/usr/share/icons/hicolor/48x48/");
    search_paths.append("/usr/share/icons/hicolor/");
    search_paths.append("/usr/share/icons/");
    QString out = shell->getCmdOut("find " + search_paths.join(" ") + " -iname \"" + search_term
                                   + "\" -print -quit 2>/dev/null", true);
    return (!out.isEmpty()) ? out : QString();
}

// fix varios exec= items to make sure they run correctly
QString MainWindow::fixExecItem(QString item)
{
    item.remove(QLatin1String(" %f"));  // remove %f if exec expects a file name since it's called without a name
    item.remove(QLatin1String(" %F"));
    item.remove(QLatin1String(" %U"));
    return item;
}

// fix name of the item
QString MainWindow::fixNameItem(QString item)
{
   if (item == "System Profiler and Benchmark")
       item = "System Information";
   return item;
}

// setup versious items and load configurations first time program runs
void MainWindow::setup()
{
    this->setWindowTitle(tr("Custom Toolbox"));
    this->adjustSize();

    QSettings settings("/etc/custom-toolbox/custom-toolbox.conf", QSettings::IniFormat);
    hideGUI = settings.value("hideGUI", "false").toBool();
    min_height = settings.value("min_height").toInt();
    min_width = settings.value("min_width").toInt();
    gui_editor = settings.value("gui_editor").toString();
    max_col = settings.value("max_col", 3).toInt();
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

    int width = this->width();
    int height = this->height();

    QSettings settings(qApp->organizationName(), qApp->applicationName() + "_" + QFileInfo(file_name).baseName());
    restoreGeometry(settings.value("geometry").toByteArray());

    if (this->isMaximized()) {  // if started maximized give option to resize to normal window size
        this->resize(width, height);
        QRect screenGeometry = QApplication::desktop()->screenGeometry();
        int x = (screenGeometry.width()-this->width()) / 2;
        int y = (screenGeometry.height()-this->height()) / 2;
        this->move(x, y);
    }

    // check if .desktop file is in autostart
    QString file_name = QDir::homePath() + "/.config/autostart/" + base_name + ".desktop"; // same base_name as .list file
    if (QFile::exists(file_name))
        ui->checkBoxStartup->setChecked(true);
    ui->lineSearch->setFocus();

    this->show();
}

// execute command when button is clicked
void MainWindow::btn_clicked()
{
    QString cmd = sender()->property("cmd").toString();

    if (hideGUI) {
        this->hide();
        system(cmd.toUtf8());
        this->show();
    } else {
        system(cmd.toUtf8() + "&");
    }
}

void MainWindow::closeEvent(QCloseEvent *)
{
    QSettings settings(qApp->organizationName(), qApp->applicationName() + "_" + QFileInfo(file_name).baseName());
    settings.setValue("geometry", saveGeometry());
}



// select .list file to open
QString MainWindow::getFileName()
{
   QString file_name = QFileDialog::getOpenFileName(this, tr("Open List File"), file_location, tr("List Files (*.list)"));
   if (file_name.isEmpty()) exit(-1);
   if (!QFile::exists(file_name)) {
       int ans = QMessageBox::critical(this, tr("File Open Error"),
                                       tr("Could not open file, do you want to try again?"),
                                       QMessageBox::Yes, QMessageBox::No);
       if (ans == QMessageBox::No)
           exit(-1);
       else
           return getFileName();
   }
   return file_name;
}

// find the .desktop file for the app name
QString MainWindow::getDesktopFileName(QString app_name)
{
    QString name = shell->getCmdOut("find " + local_dir + "/usr/share/applications -name " + app_name + ".desktop | grep . -m1", true);
    if (name.isEmpty() && system("command -v \"" + app_name.toUtf8() +"\">/dev/null") == 0)
        name = app_name; // if not a desktop file, but the command exits
    return name;
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

    QRegularExpression re;
    re.setPatternOptions(QRegularExpression::MultilineOption);

    // if a command not a .desktop file
    if (!file_name.endsWith(".desktop")) {
        app_info << file_name << comment << file_name << file_name << "true";
        return app_info;
    }

    QFile file(file_name);
    if (!file.open(QFile::Text | QFile::ReadOnly))
       return QStringList();

    QString text = file.readAll();
    file.close();

    if (lang != QLatin1String("en")) {
        re.setPattern(QLatin1String("^Name\\[") + lang + QLatin1String("\\]=(.*)$"));
        name = re.match(text).captured(1);
        re.setPattern(QLatin1String("^Comment\\[") + lang + QLatin1String("\\]=(.*)$"));
        comment = re.match(text).captured(1);
    }
    if (lang == QLatin1String("pt") && name.isEmpty()) { // Brazilian if Portuguese and name empty
        re.setPattern(QLatin1String("^Name\\[pt_BR\\]=(.*)$"));
        name = re.match(text).captured(1);
    }
    if (lang == QLatin1String("pt") && comment.isEmpty()) { // Brazilian if Portuguese and comment empty
        re.setPattern(QLatin1String("^Comment\\[pt_BR\\]=(.*)$"));
        comment = re.match(text).captured(1);
    }
    if (name.isEmpty()) { // backup if Name is not translated
        re.setPattern(QLatin1String("^Name=(.*)$"));
        name = re.match(text).captured(1);
        name = name.remove(QRegularExpression(QLatin1String("^MX "))); // remove MX from begining of the program name (most of the MX Linux apps)
    }
    if (comment.isEmpty()) { // backup if Comment is not translated
        re.setPattern(QLatin1String("^Comment=(.*)$"));
        comment = re.match(text).captured(1);
    }
    re.setPattern(QLatin1String("^Exec=(.*)$"));
    exec = re.match(text).captured(1);
    re.setPattern(QLatin1String("^Icon=(.*)$"));
    icon_name = re.match(text).captured(1);
    re.setPattern(QLatin1String("^Terminal=(.*)$"));
    terminal = re.match(text).captured(1);

    app_info << name << comment << icon_name << exec << terminal.toLower();
    return app_info;
}

// add the buttoms to the window
void MainWindow::addButtons(QMultiMap<QString, QStringList> map)
{
    int col = 0;
    int row = 0;

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
                name = fixNameItem(item.at(0));
                comment = item.at(1);
                icon_name = item.at(2);
                exec = fixExecItem(item.at(3));
                terminal = item.at(4);
                root = item.at(5);

                btn = new FlatButton(name);
                btn->setIconSize(40, 40);
                btn->setToolTip(comment);
                btn->setAutoDefault(false);
                icon_name = findIcon(icon_name);
                QIcon icon = !icon_name.isEmpty() ? QIcon(icon_name) : QIcon::fromTheme("utilities-terminal");
                btn->setIcon(icon);
                ui->gridLayout_btn->addWidget(btn, row, col);
                 ui->gridLayout_btn->setRowStretch(row, 0);
                col += 1;
                if (col >= max_col) {
                    col = 0;
                    row += 1;
                }

                if (terminal == "true")
                    exec = "x-terminal-emulator -e "  + exec;
                if (root == "true")
                    exec = "su-to-root -X -c '" + exec + "'";
                btn->setProperty("cmd", exec);
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
    if (line.startsWith("#") || line.isEmpty()) // filter out comment and empty lines
        return;

    QStringList line_list = line.split("=");
    QString key = line_list.at(0).trimmed();
    QString value = line_list.size() > 1 ? line_list[1].remove(QLatin1Char('"')).trimmed() : QString();
    if (key.toLower() == QLatin1String("name")) {
        this->setWindowTitle(value);
    } else if (key.toLower() == QLatin1String("comment")) {
        ui->commentLabel->setText(value);
    } else if (key.toLower() == QLatin1String("category")) {
        categories.append(value);
    } else { // assume it's the name of the app and potentially a "root" flag
        QStringList list = key.split(" ");
        QString desktop_file = getDesktopFileName(list.at(0));
        if (!desktop_file.isEmpty()) {
            QStringList info = getDesktopFileInfo(desktop_file);
            if (list.size() > 1) // check if root flag present
               info << ((list.at(1).toLower() == QLatin1String("root")) ? QLatin1String("true") : QLatin1String("false"));
            else
                info << QLatin1String("false");
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
    if (QFileInfo::exists(file_name)) {
        base_name = QFileInfo(file_name).baseName();
        file_location= QFileInfo(file_name).path();
        if (!file.open(QFile::ReadOnly | QFile::Text)) {
            QMessageBox::critical(this, tr("File Open Error"), tr("Could not open file: ") + file_name + "\n" +
                                  tr("Application will close."));
            exit(-1);
        }
        QTextStream in(&file);
        QString line;
        while(!in.atEnd())
            processLine(in.readLine());
        file.close();
    } else {
        exit(-1);
    }
}

// About button clicked
void MainWindow::on_buttonAbout_clicked()
{
    this->hide();
    displayAboutMsgBox(tr("About %1").arg(this->windowTitle()), "<p align=\"center\"><b><h2>" + this->windowTitle()
                       + "</h2></b></p><p align=\"center\">" +
                       tr("Version: ") + VERSION + "</p><p align=\"center\"><h3>" +
                       tr("Custom Toolbox is a tool used for creating a custom launcher") +
                       "</h3></p><p align=\"center\"><a href=\"http://mxlinux.org\">http://mxlinux.org</a><br /></p>"
                       "<p align=\"center\">" + tr("Copyright (c) MX Linux") + "<br /><br /></p>",
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
            QString name = item.at(0);
            QString comment = item.at(1);
            if (name.contains(arg1, Qt::CaseInsensitive)
                    or comment.contains(arg1, Qt::CaseInsensitive)
                    or category.contains(arg1, Qt::CaseInsensitive)) {
                new_map.insert(category, item);
            }
        }
    }
    if (!new_map.empty())
        arg1.isEmpty() ? addButtons(category_map) : addButtons(new_map);
}

// add a .desktop file to the ~/.config/autostart
void MainWindow::on_checkBoxStartup_clicked(bool checked)
{
    QString file_name = QDir::homePath() + "/.config/autostart/" + base_name + ".desktop"; // same base_name as .list file
    if (checked) {
        QFile file(file_name);
        if (!file.open(QFile::WriteOnly | QFile::Text))
            QMessageBox::critical(this, tr("File Open Error"), tr("Could not write file: ") + file_name);
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
    if (!QFile::exists(gui_editor)) {  // if specified editor doesn't exist get the default one
        QString desktop_file = getDesktopFileName(shell->getCmdOut("xdg-mime query default text/plain").remove(".desktop"));
        QString editor = shell->getCmdOut("grep -m1 ^Exec " + desktop_file + " |cut -d= -f2 |cut -d\" \" -f1", true);
        if (editor.isEmpty() || system("command -v " + editor.toUtf8()) != 0) // if default one doesn't exit use nano as backup editor
            editor = "x-terminal-emulator -e nano";
        else if (getuid() == 0 && (editor == "kate" || editor == "kwrite")) // need to run these as normal user
            editor = "runuser -u $(logname) " + editor;
        gui_editor = editor;
    }
    this->hide();
    QString cmd = "su-to-root -X -c '" + gui_editor + " " + file_name + "'";
    system(cmd.toUtf8());
    readFile(file_name);
    setGui();
}
