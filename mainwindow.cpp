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
#include <QFileDialog>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollBar>
#include <QSettings>
#include <QTextEdit>

#include <unistd.h>

#include "about.h"
#include "flatbutton.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "version.h"

MainWindow::MainWindow(const QCommandLineParser &arg_parser, QWidget *parent)
    : QDialog(parent),
      col_count { 0 },
      ui(new Ui::MainWindow)
{
    qDebug().noquote() << QCoreApplication::applicationName() << "version:" << VERSION;
    ui->setupUi(this);
    setConnections();
    if (arg_parser.isSet(QStringLiteral("remove-checkbox")))
        ui->checkBoxStartup->hide();

    setWindowFlags(Qt::Window); // for the close, min and max buttons
    local_dir = QFile::exists(QDir::homePath() + "/.local/share/applications")
              ? QDir::homePath() + "/.local/share/applications" : QLatin1String("");
    setup();

    file_location = QStringLiteral("/etc/custom-toolbox");

    const QStringList arg_list = arg_parser.positionalArguments();
    if (!arg_list.empty())
        file_name = QFile(arg_list.first()).exists() ? arg_list.first() : getFileName();
    else
        file_name = getFileName();
    readFile(file_name);
    setGui();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// Find icon file by name
QIcon MainWindow::findIcon(QString icon_name)
{
    if (icon_name.isEmpty())
        return QIcon();
    if (QFileInfo::exists("/" + icon_name))
        return QIcon(icon_name);

    QString search_term = icon_name;
    if (!icon_name.endsWith(QLatin1String(".png")) && !icon_name.endsWith(QLatin1String(".svg")) && !icon_name.endsWith(QLatin1String(".xpm")))
        search_term = icon_name + ".*";

    icon_name.remove(QRegularExpression(QStringLiteral("\\.png$|\\.svg$|\\.xpm$")));

    if (!icon_theme.isEmpty())
        QIcon::setThemeName(icon_theme);

    // return the icon from the theme if it exists
    if (QIcon::hasThemeIcon(icon_name))
        return QIcon::fromTheme(icon_name);

    // Try to find in most obvious places
    QStringList search_paths { QDir::homePath() + "/.local/share/icons/",
                "/usr/share/pixmaps/",
                "/usr/local/share/icons/",
                "/usr/share/icons/hicolor/48x48/apps/" };
    for (const QString &path : search_paths) {
        if (!QFileInfo::exists(path)) {
            search_paths.removeOne(path);
            continue;
        }
        for (const QString &ext : { ".png", ".svg", ".xpm" }) {
            QString file = path + icon_name + ext;
            if (QFileInfo::exists(file))
                return QIcon(file);
        }
    }

    // Search recursively
    search_paths.append(QStringLiteral("/usr/share/icons/hicolor/48x48/"));
    search_paths.append(QStringLiteral("/usr/share/icons/hicolor/"));
    search_paths.append(QStringLiteral("/usr/share/icons/"));
    proc.start(QStringLiteral("find"), QStringList{search_paths << QStringLiteral("-iname") << search_term << QStringLiteral("-print") << QStringLiteral("-quit")});
    proc.waitForFinished();
    const QString out = proc.readAllStandardOutput().trimmed();
    if (out.isEmpty())
        return QIcon();
    return QIcon(out);
}

// Fix varios exec= items to make sure they run correctly; remove %f if exec expects a file name since it's called without a name
QString MainWindow::fixExecItem(QString item)
{
    return item.remove(QRegularExpression(QStringLiteral(" %f| %F| %U")));
}

// Fix name of the item
QString MainWindow::fixNameItem(QString item)
{
    if (item == QLatin1String("System Profiler and Benchmark"))
        item = QStringLiteral("System Information");
    return item;
}

// Setup versious items and load configurations first time program runs
void MainWindow::setup()
{
    this->setWindowTitle(tr("Custom Toolbox"));
    this->adjustSize();

    QSettings settings(QStringLiteral("/etc/custom-toolbox/custom-toolbox.conf"), QSettings::NativeFormat);
    hideGUI = settings.value(QStringLiteral("hideGUI"), "false").toBool();
    min_height = settings.value(QStringLiteral("min_height")).toInt();
    min_width = settings.value(QStringLiteral("min_width")).toInt();
    gui_editor = settings.value(QStringLiteral("gui_editor")).toString();
    fixed_number_col = settings.value(QStringLiteral("fixed_number_columns"), 0).toInt();
    int size = settings.value(QStringLiteral("icon_size"), 40).toInt();
    icon_size = { size, size };
}

// Add buttons and resize GUI
void MainWindow::setGui()
{
    // Remove all items from the layout
    QLayoutItem *child {nullptr};
    while ((child = ui->gridLayout_btn->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    addButtons(category_map);
    this->adjustSize();
    this->setMinimumSize(min_width, min_height);

    QSettings settings(QApplication::organizationName(), QApplication::applicationName() + "_" + QFileInfo(file_name).baseName());
    restoreGeometry(settings.value(QStringLiteral("geometry")).toByteArray());

    const QSize size = this->size();
    if (this->isMaximized()) { // if started maximized give option to resize to normal window size
        this->resize(size);
        const QRect screenGeometry = QApplication::primaryScreen()->geometry();
        const int x = (screenGeometry.width() - this->width()) / 2;
        const int y = (screenGeometry.height() - this->height()) / 2;
        this->move(x, y);
    }

    // Check if .desktop file is in autostart
    const QString file_name = QDir::homePath() + "/.config/autostart/" + base_name + ".desktop"; // same base_name as .list file
    if (QFile::exists(file_name)) {
        ui->checkBoxStartup->show();
        ui->checkBoxStartup->setChecked(true);
    }
    ui->textSearch->setFocus();
}

// Execute command when button is clicked
void MainWindow::btn_clicked()
{
    const QString cmd = sender()->property("cmd").toString();

    if (hideGUI) {
        this->hide();
        system(cmd.toUtf8());
        this->show();
    } else {
        system(cmd.toUtf8() + "&");
    }
}

void MainWindow::closeEvent(QCloseEvent* /*unused*/)
{
    QSettings settings(QApplication::organizationName(), QApplication::applicationName() + "_" + QFileInfo(file_name).baseName());
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    if (event->oldSize().width() == event->size().width())
        return;
    if (fixed_number_col != 0) // 0 is default value, if nothing set in .conf file
        return;
    if (int new_count = this->width() / 200; new_count != col_count) {
        if (new_count > max_elements && col_count == max_elements)
            return;
        col_count = 0;
        if (ui->textSearch->text().isEmpty()) {
            QLayoutItem *child {nullptr};
            while ((child = ui->gridLayout_btn->takeAt(0)) != nullptr) {
                delete child->widget();
                delete child;
            }
            addButtons(category_map);
        } else {
            textSearch_textChanged(ui->textSearch->text());
        }
    }
}

// Select .list file to open
QString MainWindow::getFileName()
{
    QString file_name = QFileDialog::getOpenFileName(this, tr("Open List File"), file_location, tr("List Files (*.list)"));
    if (file_name.isEmpty())
        exit(EXIT_FAILURE);
    if (!QFile::exists(file_name)) {
        if (QMessageBox::No == QMessageBox::critical(this, tr("File Open Error"), tr("Could not open file, do you want to try again?"),
                                                     QMessageBox::Yes, QMessageBox::No))
            exit(EXIT_FAILURE);
        else
            return getFileName();
    }
    return file_name;
}

// Find the .desktop file for the app name
QString MainWindow::getDesktopFileName(const QString &app_name)
{
    proc.start(QStringLiteral("find"), QStringList{local_dir, "/usr/share/applications", "-name", app_name + ".desktop"});
    proc.waitForFinished();
    QString name = proc.readAllStandardOutput();
    name = name.section(QStringLiteral("\n"), 0, 0); // get first if more items
    if (name.isEmpty() && system("command -v \"" + app_name.toUtf8() + "\">/dev/null") == 0)
        name = app_name; // if not a desktop file, but the command exits
    return name;
}

// Return the app info needed for the button
QStringList MainWindow::getDesktopFileInfo(const QString &file_name)
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

    // If a command not a .desktop file
    if (!file_name.endsWith(QLatin1String(".desktop"))) {
        app_info << file_name << comment << file_name << file_name << QStringLiteral("true");
        return app_info;
    }

    QFile file(file_name);
    if (!file.open(QFile::Text | QFile::ReadOnly))
        return QStringList();

    const QString text = file.readAll();
    file.close();

    if (lang != QLatin1String("en")) {
        re.setPattern(QStringLiteral("^Name\\[") + lang + QStringLiteral("\\]=(.*)$"));
        name = re.match(text).captured(1);
        re.setPattern(QStringLiteral("^Comment\\[") + lang + QStringLiteral("\\]=(.*)$"));
        comment = re.match(text).captured(1);
    }
    if (lang == QLatin1String("pt") && name.isEmpty()) { // Brazilian if Portuguese and name empty
        re.setPattern(QStringLiteral("^Name\\[pt_BR\\]=(.*)$"));
        name = re.match(text).captured(1);
    }
    if (lang == QLatin1String("pt") && comment.isEmpty()) { // Brazilian if Portuguese and comment empty
        re.setPattern(QStringLiteral("^Comment\\[pt_BR\\]=(.*)$"));
        comment = re.match(text).captured(1);
    }
    if (name.isEmpty()) { // backup if Name is not translated
        re.setPattern(QStringLiteral("^Name=(.*)$"));
        name = re.match(text).captured(1);
        name = name.remove(QRegularExpression(QStringLiteral("^MX "))); // remove MX from begining of the program name (most of the MX Linux apps)
    }
    if (comment.isEmpty()) { // backup if Comment is not translated
        re.setPattern(QStringLiteral("^Comment=(.*)$"));
        comment = re.match(text).captured(1);
    }
    re.setPattern(QStringLiteral("^Exec=(.*)$"));
    exec = re.match(text).captured(1);
    re.setPattern(QStringLiteral("^Icon=(.*)$"));
    icon_name = re.match(text).captured(1);
    re.setPattern(QStringLiteral("^Terminal=(.*)$"));
    terminal = re.match(text).captured(1);

    app_info << name << comment << icon_name << exec << terminal.toLower();
    return app_info;
}

void MainWindow::addButtons(const QMultiMap<QString, QStringList> &map)
{
    int col = 0;
    int row = 0;
    int max = this->width() / 200;

    if (fixed_number_col != 0) // default value is 0
        max = fixed_number_col;

    max_elements = 0;
    for (const QString &category : map.uniqueKeys())
        if (map.count(category) > max_elements)
            max_elements = map.count(category);

    QString name;
    QString comment;
    QString exec;
    QString icon_name;
    QString root;
    QString terminal;

    for (const QString &category : map.uniqueKeys()) {
        if (!category_map.values(category).isEmpty()) {
            auto *label = new QLabel(this);
            QFont font;
            font.setBold(true);
            font.setUnderline(true);
            label->setFont(font);
            label->setText(category);
            col = 0;
            ++row;
            ui->gridLayout_btn->addWidget(label, row, col);
            ui->gridLayout_btn->setRowStretch(row, 0);
            ++row;
            for (const QStringList& item : map.values(category)) {
                if (col >= col_count)
                    col_count = col + 1;
                name = fixNameItem(item.at(0));
                comment = item.at(1);
                icon_name = item.at(2);
                exec = fixExecItem(item.at(3));
                terminal = item.at(4);
                root = item.at(5);

                btn = new FlatButton(name);
                btn->setIconSize(icon_size);
                btn->setToolTip(comment);
                btn->setAutoDefault(false);
                QIcon icon = findIcon(icon_name);
                if (icon.isNull())
                    icon = QIcon::fromTheme(QStringLiteral("utilities-terminal"));
                btn->setIcon(icon);
                ui->gridLayout_btn->addWidget(btn, row, col);
                ui->gridLayout_btn->setRowStretch(row, 0);
                ++col;
                if (col >= max) {
                    col = 0;
                    ++row;
                }
                if (terminal == QLatin1String("true"))
                    exec = "x-terminal-emulator -e " + exec;
                if (root == QLatin1String("true"))
                    exec = "su-to-root -X -c '" + exec + "'";
                btn->setProperty("cmd", exec);
                QObject::connect(btn, &QPushButton::clicked, this, &MainWindow::btn_clicked);
            }
        }
        // Add empty row if it's not the last key
        if (category != map.lastKey()) {
            col = 0;
            ++row;
            auto *line = new QFrame();
            line->setFrameShape(QFrame::HLine);
            line->setFrameShadow(QFrame::Sunken);
            ui->gridLayout_btn->addWidget(line, row, col, 1, -1);
            ui->gridLayout_btn->setRowStretch(row, 0);
        }
    }
    ui->gridLayout_btn->setRowStretch(row, 1);
}

void MainWindow::processLine(const QString &line)
{
    if (line.startsWith(QLatin1String("#")) || line.isEmpty()) // filter out comment and empty lines
        return;

    QStringList line_list = line.split(QStringLiteral("="));
    QString key = line_list.first().trimmed();
    QString value = line_list.size() > 1 ? line_list[1].remove(QLatin1Char('"')).trimmed() : QString();
    if (key.toLower() == QLatin1String("name")) {
        this->setWindowTitle(value);
    } else if (key.toLower() == QLatin1String("comment")) {
        ui->commentLabel->setText(value);
    } else if (key.toLower() == QLatin1String("category")) {
        categories.append(value);
    } else if (key.toLower() == QLatin1String("theme")) {
        icon_theme = value;
    } else { // assume it's the name of the app and potentially a "root" flag
        QStringList list = key.split(QStringLiteral(" "));
        QString desktop_file = getDesktopFileName(list.first());
        if (!desktop_file.isEmpty()) {
            QStringList info = getDesktopFileInfo(desktop_file);
            if (list.size() > 1) { // check if root or alias flag present
                info << ((list.contains(QLatin1String("root"))) ? QStringLiteral("true") : QStringLiteral("false"));
                if (list.contains(QLatin1String("alias"))) {
                    QString str;
                    for (auto it = list.lastIndexOf(QStringLiteral("alias")); it + 1 < list.size(); ++it)
                        str.append(list.at(it + 1) + " ");
                    info[0] = str.trimmed().remove(QStringLiteral("'")).remove(QStringLiteral("\""));
                }
            } else {
                info << QStringLiteral("false");
            }

            category_map.insert(categories.constLast(), info);
        }
    }
}

// Open the .list file and process it
void MainWindow::readFile(const QString &file_name)
{
    // Reset categories, category_map
    categories.clear();
    category_map.clear();

    QFile file(file_name);
    if (!QFileInfo::exists(file_name))
        exit(EXIT_FAILURE);
    base_name = QFileInfo(file_name).baseName();
    file_location = QFileInfo(file_name).path();
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::critical(this, tr("File Open Error"), tr("Could not open file: ") + file_name + "\n" + tr("Application will close."));
        exit(EXIT_FAILURE);
    }

    for (QTextStream in(&file); !in.atEnd();)
        processLine(in.readLine());
    file.close();

    // Reverse map
    QMultiMap<QString, QStringList> map;
    for (auto i = category_map.constBegin(); i != category_map.constEnd(); ++i)
        map.insert(i.key(), i.value());
    category_map = map;
}

void MainWindow::setConnections()
{
    connect(ui->pushAbout, &QPushButton::clicked, this, &MainWindow::pushAbout_clicked);
    connect(ui->pushEdit, &QPushButton::clicked, this, &MainWindow::pushEdit_clicked);
    connect(ui->pushHelp, &QPushButton::clicked, this, &MainWindow::pushHelp_clicked);
    connect(ui->checkBoxStartup, &QPushButton::clicked, this, &MainWindow::checkBoxStartup_clicked);
    connect(ui->textSearch, &QLineEdit::textChanged, this, &MainWindow::textSearch_textChanged);
}

void MainWindow::pushAbout_clicked()
{
    this->hide();
    displayAboutMsgBox(tr("About %1").arg(this->windowTitle()),
                       "<p align=\"center\"><b><h2>" + this->windowTitle() + "</h2></b></p><p align=\"center\">" +
                       tr("Version: ") + VERSION + "</p><p align=\"center\"><h3>" +
                       tr("Custom Toolbox is a tool used for creating a custom launcher") +
                       "</h3></p><p align=\"center\"><a href=\"http://mxlinux.org\">http://mxlinux.org</a><br /></p>"
                       "<p align=\"center\">" + tr("Copyright (c) MX Linux") + "<br /><br /></p>",
                       QStringLiteral("/usr/share/doc/custom-toolbox/license.html"), tr("%1 License").arg(this->windowTitle()));
    this->show();
}

void MainWindow::pushHelp_clicked()
{
    const QString url = QStringLiteral("/usr/share/doc/custom-toolbox/help.html");
    displayDoc(url, tr("%1 Help").arg(this->windowTitle()));
}

void MainWindow::textSearch_textChanged(const QString& arg1)
{
    // remove all items from the layout
    QLayoutItem *child;
    while ((child = ui->gridLayout_btn->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    // create a new_map with items that match the search argument
    QMultiMap<QString, QStringList> new_map;
    for (auto i = category_map.constEnd() - 1; i != category_map.constBegin() - 1; --i) {
        QString category = i.key();
        QString name = i.value().first();
        QString comment = i.value().first();
        if (name.contains(arg1, Qt::CaseInsensitive)
            || comment.contains(arg1, Qt::CaseInsensitive)
            || category.contains(arg1, Qt::CaseInsensitive)) {
            new_map.insert(i.key(), i.value());
        }
    }
    addButtons(new_map.empty() ? category_map : new_map);
}

// Add a .desktop file to the ~/.config/autostart
void MainWindow::checkBoxStartup_clicked(bool checked)
{
    const QString &file_name = QDir::homePath() + "/.config/autostart/" + base_name + ".desktop"; // same base_name as .list file
    if (checked) {
        QFile file(file_name);
        if (!file.open(QFile::WriteOnly | QFile::Text))
            QMessageBox::critical(this, tr("File Open Error"), tr("Could not write file: ") + file_name);
        QTextStream out(&file);
        out << "[Desktop Entry]"
            << "\n";
        out << "Name=" << this->windowTitle() << "\n";
        out << "Comment=" << ui->commentLabel->text() << "\n";
        out << "Exec="
            << "custom-toolbox " + file_location + "/" + base_name + ".list"
            << "\n";
        out << "Terminal=false"
            << "\n";
        out << "Type=Application"
            << "\n";
        out << "Icon=custom-toolbox"
            << "\n";
        out << "Categories=XFCE;System"
            << "\n";
        out << "StartupNotify=false";
        file.close();
    } else {
        QDir().remove(file_name);
    }
}

void MainWindow::pushEdit_clicked()
{
    QString editor = gui_editor;
    QString desktop_file;
    if (editor.isEmpty() || system("command -v " + editor.toUtf8()) != 0) { // if specified editor doesn't exist get the default one
        proc.start(QStringLiteral("xdg-mime"), QStringList{"query", "default", "text/plain"}, QIODevice::ReadOnly);
        proc.waitForFinished();
        QString default_editor = proc.readAllStandardOutput().trimmed();

        // find first app with .desktop name that matches default_editors
        QString local = QFile::exists(QDir::homePath() + "/.local/share/applications") ? QDir::homePath() + "/.local/share/applications " : QLatin1String("");
        proc.start(QStringLiteral("find"), QStringList{local, "/usr/share/applications", "-name", default_editor, "-print", "-quit"}, QIODevice::ReadOnly);
        proc.waitForFinished();
        desktop_file = proc.readAllStandardOutput().trimmed();

        QFile file(desktop_file);
        if (file.open(QIODevice::ReadOnly)) {
            QString line;
            while (!file.atEnd()) {
                line = file.readLine();
                if (line.contains(QRegularExpression(QStringLiteral("^Exec="))))
                    break;
            }
            file.close();
            editor = line.remove(QRegularExpression(QStringLiteral("^Exec=|%u|%U|%f|%F|%c|%C"))).trimmed();
        }
        if (editor.isEmpty()) // use nano as backup editor
            editor = QStringLiteral("x-terminal-emulator -e nano");
    }
    // Editors that need to run as normal user
    if (getuid() == 0 && (editor.endsWith(QLatin1String("kate")) || editor.endsWith(QLatin1String("kwrite")) || desktop_file.endsWith(QLatin1String("atom.desktop"))))
        editor = "runuser -u $(logname) " + editor;
    QString cmd = editor + " " + file_name;
    // If we need to run as root (with the exception of the listed editors)
    if (!QFileInfo(file_name).isWritable() && !editor.endsWith(QLatin1String("kate")) && !editor.endsWith(QLatin1String("kwrite")) && !desktop_file.endsWith(QLatin1String("atom.desktop")))
        cmd = "su-to-root -X -c '" + cmd + "'";

    this->hide(); // unfortunatelly Atom is non-blocking
    system(cmd.toUtf8());
    readFile(file_name);
    setGui();
    this->show();
}
