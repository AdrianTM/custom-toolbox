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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QDirIterator>
#include <QFileDialog>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollBar>
#include <QSettings>
#include <QStandardPaths>
#include <QTextEdit>

#include "about.h"
#include "flatbutton.h"
#include "version.h"
#include <unistd.h>

MainWindow::MainWindow(const QCommandLineParser &arg_parser, QWidget *parent)
    : QDialog(parent)
    , col_count {0}
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setConnections();
    if (arg_parser.isSet(QStringLiteral("remove-checkbox")))
        ui->checkBoxStartup->hide();

    setWindowFlags(Qt::Window); // for the close, min and max buttons
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

MainWindow::~MainWindow() { delete ui; }

QIcon MainWindow::findIcon(const QString &icon_name)
{
    if (icon_name.isEmpty())
        return QIcon();
    // Use slash to avoid matching file/folder names when is meant to match full path
    if (QFileInfo::exists("/" + icon_name))
        return QIcon(icon_name);

    const QRegularExpression re {R"(\.png$|\.svg$|\.xpm$")"};
    QString name_noext = icon_name;
    name_noext.remove(re);

    if (!icon_theme.isEmpty())
        QIcon::setThemeName(icon_theme);

    // Return the icon from the theme if it exists
    if (QIcon::hasThemeIcon(name_noext))
        return QIcon::fromTheme(name_noext);

    // Try to find in most obvious places
    QStringList search_paths {QDir::homePath() + "/.local/share/icons/", "/usr/share/pixmaps/",
                              "/usr/local/share/icons/", "/usr/share/icons/", "/usr/share/icons/hicolor/48x48/apps/"};

    // Optimization: search first for the full icon_name with the specified extension
    auto it = std::find_if(search_paths.cbegin(), search_paths.cend(),
                           [&](const QString &path) { return QFileInfo::exists(path + icon_name); });

    if (it != search_paths.cend()) {
        QString foundPath = *it;
        return QIcon(foundPath + icon_name);
    }

    // run loop again if icon not found
    for (const QString &path : search_paths) {
        if (!QFileInfo::exists(path)) {
            search_paths.removeOne(path);
            continue;
        }
        for (const QString &ext : {".png", ".svg", ".xpm"}) {
            QString file = path + name_noext + ext;
            if (QFileInfo::exists(file))
                return QIcon(file);
        }
    }

    // Backup search: search all hicolor icons and return the first one found
    proc.start(QStringLiteral("find"),
               QStringList {search_paths << QStringLiteral("-iname") << name_noext + ".*" << QStringLiteral("-print")
                                         << QStringLiteral("-quit")});
    proc.waitForFinished();
    const QString out = proc.readAllStandardOutput().trimmed();
    if (out.isEmpty())
        return QIcon();
    return QIcon(out);
}

// Strip %f, %F, %U, etc. if exec expects a file name since it's called without an argument from this launcher.
void MainWindow::fixExecItem(QString *item) { item->remove(QRegularExpression(QStringLiteral(R"( %[a-zA-Z])"))); }

void MainWindow::fixNameItem(QString *item)
{
    if (*item == QLatin1String("System Profiler and Benchmark"))
        *item = QStringLiteral("System Information");
}

void MainWindow::setup()
{
    this->setWindowTitle(tr("Custom Toolbox"));
    this->adjustSize();

    const int default_icon_size = 40;

    QSettings settings(QStringLiteral("/etc/custom-toolbox/custom-toolbox.conf"), QSettings::NativeFormat);
    hideGUI = settings.value(QStringLiteral("hideGUI"), "false").toBool();
    min_height = settings.value(QStringLiteral("min_height")).toInt();
    min_width = settings.value(QStringLiteral("min_width")).toInt();
    gui_editor = settings.value(QStringLiteral("gui_editor")).toString();
    fixed_number_col = settings.value(QStringLiteral("fixed_number_columns"), 0).toInt();
    int size = settings.value(QStringLiteral("icon_size"), default_icon_size).toInt();
    icon_size = {size, size};
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
    this->adjustSize();
    this->setMinimumSize(min_width, min_height);

    QSettings settings(QApplication::organizationName(),
                       QApplication::applicationName() + "_" + QFileInfo(file_name).baseName());
    restoreGeometry(settings.value(QStringLiteral("geometry")).toByteArray());

    const QSize size = this->size();
    if (this->isMaximized()) { // if started maximized give option to resize to normal window size
        this->resize(size);
        const QRect screenGeometry = QApplication::primaryScreen()->geometry();
        const int x = (screenGeometry.width() - this->width()) / 2;
        const int y = (screenGeometry.height() - this->height()) / 2;
        this->move(x, y);
    }

    addButtons(category_map);

    // Check if .desktop file is in autostart
    const QString file_name
        = QDir::homePath() + "/.config/autostart/" + base_name + ".desktop"; // same base_name as .list file
    if (QFile::exists(file_name)) {
        ui->checkBoxStartup->show();
        ui->checkBoxStartup->setChecked(true);
    }
    ui->textSearch->setFocus();
}

void MainWindow::btn_clicked()
{
    const QString cmd = sender()->property("cmd").toString();
    if (hideGUI) {
        this->hide();
        system(cmd.toUtf8());
        this->show();
    } else {
        system(cmd.toUtf8());
    }
}

void MainWindow::closeEvent(QCloseEvent * /*unused*/)
{
    QSettings settings(QApplication::organizationName(),
                       QApplication::applicationName() + "_" + QFileInfo(file_name).baseName());
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    if (firstRun) {
        firstRun = false;
        return;
    }
    if (event->oldSize().width() == event->size().width())
        return;
    if (fixed_number_col != 0) // 0 is default value, if nothing set in .conf file
        return;
    const auto item_size = 200; // this is a trial and error average value
    int new_count = this->width() / item_size;
    if (new_count == col_count)
        return;
    // when reaching the max no need to readd buttons, only if making the window smaller new_count < max_elements
    if (new_count >= max_elements && col_count == max_elements)
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

// Select .list file to open
QString MainWindow::getFileName()
{
    QString file_name
        = QFileDialog::getOpenFileName(this, tr("Open List File"), file_location, tr("List Files (*.list)"));
    if (file_name.isEmpty())
        exit(EXIT_FAILURE);
    if (!QFile::exists(file_name)) {
        if (QMessageBox::No
            == QMessageBox::critical(this, tr("File Open Error"), tr("Could not open file, do you want to try again?"),
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
    auto list = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    auto it = std::find_if(list.cbegin(), list.cend(), [&](const QString &path) {
        QDirIterator it(path, {app_name + ".desktop"}, QDir::Files, QDirIterator::Subdirectories);
        return it.hasNext();
    });

    if (it != list.cend())
        return *it + "/" + app_name + ".desktop";

    // if desktop file not found, but command exists
    return QStandardPaths::findExecutable(app_name, {path}).section("/", -1);
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
        name = name.remove(QRegularExpression(
            QStringLiteral("^MX "))); // remove MX from begining of the program name (most of the MX Linux apps)
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
    auto categories = map.uniqueKeys();
    for (const QString &category : qAsConst(categories))
        if (map.count(category) > max_elements)
            max_elements = map.count(category);

    QString name;
    QString comment;
    QString exec;
    QString icon_name;
    QString root;
    QString terminal;

    for (const QString &category : qAsConst(categories)) {
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
            for (const QStringList &item : map.values(category)) {
                if (col >= col_count)
                    col_count = col + 1;
                name = item.at(0);
                comment = item.at(1);
                icon_name = item.at(2);
                exec = item.at(3);
                terminal = item.at(4);
                root = item.at(5);
                fixNameItem(&name);
                fixExecItem(&exec);

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
                    exec.push_front("env DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY x-terminal-emulator -e ");
                if (root == QLatin1String("true") && getuid() != 0)
                    exec.push_front("pkexec ");
                btn->setProperty("cmd", exec);
                connect(btn, &QPushButton::clicked, this, &MainWindow::btn_clicked);
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

    const QStringList line_list = line.split(QStringLiteral("="));
    const QString key = line_list.first().trimmed();
    QString value;
    if (line_list.size() > 1) {
        value = line_list.at(1).trimmed();
        value.remove(QLatin1Char('"'));
    }

    if (key.toLower() == QLatin1String("name")) {
        this->setWindowTitle(value);
    } else if (key.toLower() == QLatin1String("comment")) {
        ui->commentLabel->setText(value);
    } else if (key.toLower() == QLatin1String("category")) {
        categories.append(value);
    } else if (key.toLower() == QLatin1String("theme")) {
        icon_theme = value;
    } else { // assume it's the name of the app and potentially a "root" flag
        const QStringList list = key.split(QStringLiteral(" "));
        const QString desktop_file = getDesktopFileName(list.first());
        if (!desktop_file.isEmpty()) {
            QStringList info = getDesktopFileInfo(desktop_file);
            if (list.size() > 1) { // check if root or alias flag present
                info << ((list.contains(QLatin1String("root"))) ? QStringLiteral("true") : QStringLiteral("false"));
                if (list.contains(QLatin1String("alias"))) {
                    QString str;
                    for (auto it = list.lastIndexOf(QStringLiteral("alias")); it + 1 < list.size(); ++it)
                        str.append(list.at(it + 1) + " ");
                    info.first() = str.trimmed().remove(QStringLiteral("'")).remove(QStringLiteral("\""));
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
        QMessageBox::critical(this, tr("File Open Error"),
                              tr("Could not open file: ") + file_name + "\n" + tr("Application will close."));
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
    connect(ui->checkBoxStartup, &QPushButton::clicked, this, &MainWindow::checkBoxStartup_clicked);
    connect(ui->pushAbout, &QPushButton::clicked, this, &MainWindow::pushAbout_clicked);
    connect(ui->pushCancel, &QPushButton::clicked, this, &MainWindow::close);
    connect(ui->pushEdit, &QPushButton::clicked, this, &MainWindow::pushEdit_clicked);
    connect(ui->pushHelp, &QPushButton::clicked, this, &MainWindow::pushHelp_clicked);
    connect(ui->textSearch, &QLineEdit::textChanged, this, &MainWindow::textSearch_textChanged);
}

void MainWindow::pushAbout_clicked()
{
    this->hide();
    displayAboutMsgBox(
        tr("About %1").arg(this->windowTitle()),
        "<p align=\"center\"><b><h2>" + this->windowTitle() + "</h2></b></p><p align=\"center\">" + tr("Version: ")
            + VERSION + "</p><p align=\"center\"><h3>"
            + tr("Custom Toolbox is a tool used for creating a custom launcher")
            + "</h3></p><p align=\"center\"><a href=\"http://mxlinux.org\">http://mxlinux.org</a><br /></p>"
              "<p align=\"center\">"
            + tr("Copyright (c) MX Linux") + "<br /><br /></p>",
        QStringLiteral("/usr/share/doc/custom-toolbox/license.html"), tr("%1 License").arg(this->windowTitle()));
    this->show();
}

void MainWindow::pushHelp_clicked()
{
    const QString url = QStringLiteral("/usr/share/doc/custom-toolbox/help.html");
    displayDoc(url, tr("%1 Help").arg(this->windowTitle()));
}

void MainWindow::textSearch_textChanged(const QString &arg1)
{
    // remove all items from the layout
    QLayoutItem *child = nullptr;
    while ((child = ui->gridLayout_btn->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    // create a new_map with items that match the search argument
    QMultiMap<QString, QStringList> new_map;
    for (auto i = category_map.cbegin(); i != category_map.cend(); ++i) {
        const QString &category = i.key();
        const QStringList &values = i.value();
        const QString &name = values.first();
        const QString &comment = values.last();
        if (name.contains(arg1, Qt::CaseInsensitive) || comment.contains(arg1, Qt::CaseInsensitive)
            || category.contains(arg1, Qt::CaseInsensitive)) {
            new_map.insert(category, values);
        }
    }
    addButtons(new_map.empty() ? category_map : new_map);
}

// Add a .desktop file to the ~/.config/autostart
void MainWindow::checkBoxStartup_clicked(bool checked)
{
    const QString &file_name
        = QDir::homePath() + "/.config/autostart/" + base_name + ".desktop"; // same base_name as .list file
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
        QFile::remove(file_name);
    }
}

void MainWindow::pushEdit_clicked()
{
    QString editor = gui_editor;
    QString desktop_file;
    // if specified editor doesn't exist get the default one
    if (editor.isEmpty() || QStandardPaths::findExecutable(editor, {path}).isEmpty()) {
        proc.start("xdg-mime", {"query", "default", "text/plain"});
        proc.waitForFinished();
        QString default_editor = proc.readAllStandardOutput().trimmed();

        // find first app with .desktop name that matches default_editors
        desktop_file
            = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, default_editor, QStandardPaths::LocateFile);
        QFile file(desktop_file);
        if (file.open(QIODevice::ReadOnly)) {
            QString line;
            while (!file.atEnd()) {
                line = file.readLine();
                if (line.contains(QRegularExpression(QStringLiteral("^Exec="))))
                    break;
            }
            file.close();
            editor = line.remove(QRegularExpression(QStringLiteral("^Exec=|%u|%U|%f|%F|%c|%C|-b"))).trimmed();
        }
        if (editor.isEmpty()) // use nano as backup editor
            editor = "nano";
    }
    // Editors that need to run as normal user
    if (getuid() == 0
        && (QRegularExpression("(kate|kwrite|featherpad)$").match(editor).hasMatch()
            || QRegularExpression("atom\\.desktop$").match(desktop_file).hasMatch()))
        editor.push_front("pkexec --user $(logname) env DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY ");

    QString cmd = editor + " " + file_name;

    // Handle CLI editors
    if (QRegularExpression("nano|vi|vim|nvim|micro|emacs").match(editor).hasMatch()) {
        QString term_env = "env DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY x-terminal-emulator -e ";
        cmd.push_front(term_env);
    }

    // If we need to run as root (with the exception of the listed editors)
    if (!QFileInfo(file_name).isWritable() && !QRegularExpression("(kate|kwrite|featherpad)$").match(editor).hasMatch()
        && !QRegularExpression("atom\\.desktop$").match(desktop_file).hasMatch())
        cmd.push_front("pkexec ");
    system(cmd.toUtf8());
    readFile(file_name);
    setGui();
}
