/**********************************************************************
 *  MainWindow.cpp
 **********************************************************************
 * Copyright (C) 2017-2023 MX Authors
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
    : QDialog(parent),
      ui(new Ui::MainWindow),
      file_location {"/etc/custom-toolbox"},
      col_count {0}
{

    ui->setupUi(this);
    setConnections();
    if (arg_parser.isSet("remove-checkbox")) {
        ui->checkBoxStartup->hide();
    }

    setWindowFlags(Qt::Window); // For the close, min and max buttons
    setup();

    const QStringList argList = arg_parser.positionalArguments();
    file_name = !argList.isEmpty() && QFile(argList.first()).exists() ? argList.first() : getFileName();

    readFile(file_name);
    setGui();
}

MainWindow::~MainWindow()
{
    delete ui;
}

QIcon MainWindow::findIcon(const QString &icon_name)
{
    if (icon_name.isEmpty()) {
        return {};
    }
    // Use slash to avoid matching file/folder names when is meant to match full path
    if (QFile::exists("/" + icon_name)) {
        return QIcon(icon_name);
    }

    const QRegularExpression re {R"(\.png$|\.svg$|\.xpm$")"};
    QString name_noext = icon_name;
    name_noext.remove(re);

    if (!icon_theme.isEmpty()) {
        QIcon::setThemeName(icon_theme);
    }

    // Return the icon from the theme if it exists
    if (QIcon::hasThemeIcon(name_noext)) {
        return QIcon::fromTheme(name_noext);
    }

    // Try to find in most obvious places
    QStringList search_paths {QDir::homePath() + "/.local/share/icons/", "/usr/share/pixmaps/",
                              "/usr/local/share/icons/", "/usr/share/icons/", "/usr/share/icons/hicolor/48x48/apps/"};

    // Optimization: search first for the full icon_name with the specified extension
    auto it = std::find_if(search_paths.cbegin(), search_paths.cend(),
                           [&](const QString &path) { return QFile::exists(path + icon_name); });

    if (it != search_paths.cend()) {
        const QString &foundPath = *it;
        return QIcon(foundPath + icon_name);
    }

    // Run loop again if icon not found
    for (const QString &path : search_paths) {
        if (!QFile::exists(path)) {
            search_paths.removeOne(path);
            continue;
        }
        for (const QString ext : {".png", ".svg", ".xpm"}) {
            QString file = path + name_noext + ext;
            if (QFile::exists(file)) {
                return QIcon(file);
            }
        }
    }

    // Backup search: search all hicolor icons and return the first one found
    proc.start("find", {search_paths << "-iname" << name_noext + ".*"
                                     << "-print"
                                     << "-quit"});
    proc.waitForFinished();
    const QString out = proc.readAllStandardOutput().trimmed();
    if (out.isEmpty()) {
        return {};
    }
    return QIcon(out);
}

// Strip %f, %F, %U, etc. if exec expects a file name since it's called without an argument from this launcher.
void MainWindow::fixExecItem(QString *item)
{
    item->remove(QRegularExpression(R"( %[a-zA-Z])"));
}

void MainWindow::fixNameItem(QString *item)
{
    if (*item == "System Profiler and Benchmark") {
        *item = "System Information";
    }
}

void MainWindow::setup()
{
    setWindowTitle(tr("Custom Toolbox"));
    adjustSize();

    const int default_icon_size = 40;

    QSettings settings("/etc/custom-toolbox/custom-toolbox.conf", QSettings::NativeFormat);
    hideGUI = settings.value("hideGUI", "false").toBool();
    min_height = settings.value("min_height").toInt();
    min_width = settings.value("min_width").toInt();
    gui_editor = settings.value("gui_editor").toString();
    fixed_number_col = settings.value("fixed_number_columns", 0).toInt();
    int size = settings.value("icon_size", default_icon_size).toInt();
    icon_size = {size, size};
}

// Add buttons and resize GUI
void MainWindow::setGui()
{
    // Remove all items from the grid layout
    QLayoutItem *childItem {nullptr};
    while ((childItem = ui->gridLayout_btn->takeAt(0)) != nullptr) {
        // Delete the widget and the layout item
        delete childItem->widget();
        delete childItem;
    }
    adjustSize();
    setMinimumSize(min_width, min_height);

    QSettings settings(QApplication::organizationName(),
                       QApplication::applicationName() + "_" + QFileInfo(file_name).baseName());
    restoreGeometry(settings.value("geometry").toByteArray());

    const auto size = this->size();
    if (isMaximized()) { // If started maximized give option to resize to normal window size
        resize(size);
        const QRect screenGeometry = QApplication::primaryScreen()->geometry();
        const int x = (screenGeometry.width() - width()) / 2;
        const int y = (screenGeometry.height() - height()) / 2;
        move(x, y);
    }

    addButtons(category_map);

    // Check if .desktop file is in autostart; same custom_name as .list file
    if (QFile::exists(QDir::homePath() + "/.config/autostart/" + custom_name + ".desktop")) {
        ui->checkBoxStartup->show();
        ui->checkBoxStartup->setChecked(true);
    }
    ui->textSearch->setFocus();
    ui->pushCancel->setDefault(true); // Otherwise some other button might be default
    ui->pushCancel->setDefault(false);
}

void MainWindow::btn_clicked()
{
    const QString cmd = sender()->property("cmd").toString();
    // pkexec cannot take &, it would block the GUI that's why we need to hide it
    if (hideGUI || cmd.startsWith("pkexec")) {
        hide();
        system(cmd.toUtf8());
        show();
    } else {
        system(cmd.toUtf8() + "&");
    }
}

void MainWindow::closeEvent(QCloseEvent * /*unused*/)
{
    QSettings settings(QApplication::organizationName(),
                       QApplication::applicationName() + "_" + QFileInfo(file_name).baseName());
    settings.setValue("geometry", saveGeometry());
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    if (firstRun) {
        firstRun = false;
        return;
    }
    if (event->oldSize().width() == event->size().width()) {
        return;
    }
    if (fixed_number_col != 0) { // 0 is default value, if nothing set in .conf file
        return;
    }
    const auto item_size = 200; // Trial and error average value
    int new_count = width() / item_size;
    if (new_count == col_count) {
        return;
    }
    // When reaching the max no need to re-add buttons, only if making the window smaller new_count < max_elements
    if (new_count >= max_elements && col_count == max_elements) {
        return;
    }
    col_count = 0;
    if (ui->textSearch->text().isEmpty()) {
        QLayoutItem *childItem {nullptr};
        while ((childItem = ui->gridLayout_btn->takeAt(0)) != nullptr) {
            delete childItem->widget();
            delete childItem;
        }
        addButtons(category_map);
    } else {
        textSearch_textChanged(ui->textSearch->text());
    }
}

// Select .list file to open
QString MainWindow::getFileName()
{
    QString fileName
        = QFileDialog::getOpenFileName(this, tr("Open List File"), file_location, tr("List Files (*.list)"));
    if (fileName.isEmpty()) {
        exit(EXIT_FAILURE);
    }
    if (!QFile::exists(fileName)) {
        if (QMessageBox::No
            == QMessageBox::critical(this, tr("File Open Error"), tr("Could not open file, do you want to try again?"),
                                     QMessageBox::Yes, QMessageBox::No)) {
            exit(EXIT_FAILURE);
        } else {
            return getFileName();
        }
    }
    return fileName;
}

// Find the .desktop file for the given app name
QString MainWindow::getDesktopFileName(const QString &appName) const
{
    // Search for .desktop files in standard applications locations
    auto appPaths = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    for (const auto &appPath : appPaths) {
        QDirIterator desktopFileIterator(appPath, {appName + ".desktop"}, QDir::Files, QDirIterator::Subdirectories);
        if (desktopFileIterator.hasNext()) {
            return desktopFileIterator.next();
        }
    }

    // If desktop file not found in standard locations, try finding the executable
    QString executablePath = QStandardPaths::findExecutable(appName, {defaultPath});

    //    if (executablePath.isEmpty()) {
    //        qWarning() << "Executable not found:" << appName;
    //    }

    return executablePath.section("/", -1);
}

// Return the app info needed for the button
MainWindow::ItemInfo MainWindow::getDesktopFileInfo(const QString &fileName)
{
    ItemInfo item;

    // If not a .desktop file
    if (!fileName.endsWith(".desktop")) {
        item.name = fileName;
        item.icon_name = fileName;
        item.exec = fileName;
        item.terminal = true;
        return item;
    }

    QFile file(fileName);
    if (!file.open(QFile::Text | QFile::ReadOnly)) {
        return {};
    }
    const QString text = file.readAll();
    file.close();

    QRegularExpression re;
    re.setPatternOptions(QRegularExpression::MultilineOption);
    if (lang.section("_", 0, 0) != "en") {
        re.setPattern("^Name\\[" + lang + "\\]=(.*)$");
        QRegularExpressionMatch nameMatch = re.match(text);
        if (nameMatch.hasMatch()) {
            item.name = nameMatch.captured(1);
        } else {
            re.setPattern("^Name\\[" + lang.section("_", 0, 0) + "\\]=(.*)$");
            nameMatch = re.match(text);
            if (nameMatch.hasMatch()) {
                item.name = nameMatch.captured(1);
            }
        }
        re.setPattern("^Comment\\[" + lang + "\\]=(.*)$");
        QRegularExpressionMatch commentMatch = re.match(text);
        if (commentMatch.hasMatch()) {
            item.comment = commentMatch.captured(1);
        } else {
            re.setPattern("^Comment\\[" + lang.section("_", 0, 0) + "\\]=(.*)$");
            commentMatch = re.match(text);
            if (commentMatch.hasMatch()) {
                item.comment = commentMatch.captured(1);
            }
        }
    }
    if (item.name.isEmpty()) { // Fallback if Name is not translated
        re.setPattern("^Name=(.*)$");
        QRegularExpressionMatch nameFallbackMatch = re.match(text);
        if (nameFallbackMatch.hasMatch()) {
            item.name = nameFallbackMatch.captured(1);
            // Remove MX from begining of the program name (most of the MX Linux apps)
            item.name = item.name.remove(QRegularExpression("^MX "));
        }
    }
    if (item.comment.isEmpty()) { // Fallback if Comment is not translated
        re.setPattern("^Comment=(.*)$");
        QRegularExpressionMatch commentFallbackMatch = re.match(text);
        if (commentFallbackMatch.hasMatch()) {
            item.comment = commentFallbackMatch.captured(1);
        }
    }
    re.setPattern("^Exec=(.*)$");
    item.exec = re.match(text).captured(1);
    re.setPattern("^Icon=(.*)$");
    item.icon_name = re.match(text).captured(1);
    re.setPattern("^Terminal=(.*)$");
    item.terminal = re.match(text).captured(1);
    return item;
}

void MainWindow::addButtons(const QMultiMap<QString, ItemInfo> &map)
{
    int col = 0;
    int row = 0;
    int max = width() / 200;

    if (fixed_number_col != 0) { // Default value is 0
        max = fixed_number_col;
    }

    max_elements = 0;
    const auto sortedCategories = map.uniqueKeys();
    for (const QString &category : sortedCategories) {
        if (map.count(category) > max_elements) {
            max_elements = map.count(category);
        }
    }
    for (const QString &category : sortedCategories) {
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
            for (const auto &item : map.values(category)) {
                if (col >= col_count) {
                    col_count = col + 1;
                }
                QString name = item.name;
                QString exec = item.exec;
                fixNameItem(&name);
                fixExecItem(&exec);

                btn = new FlatButton(name);
                btn->setIconSize(icon_size);
                btn->setToolTip(item.comment);
                btn->setAutoDefault(false);
                QIcon icon = findIcon(item.icon_name);
                if (icon.isNull()) {
                    icon = QIcon::fromTheme("utilities-terminal");
                }
                btn->setIcon(icon);
                ui->gridLayout_btn->addWidget(btn, row, col);
                ui->gridLayout_btn->setRowStretch(row, 0);
                ++col;
                if (col >= max) {
                    col = 0;
                    ++row;
                }
                if (item.terminal == "true") {
                    exec.push_front("x-terminal-emulator -e ");
                }
                if (item.root == "true" && getuid() != 0) {
                    exec.push_front("pkexec env DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY ");
                }
                if (item.root == "user" && getuid() == 0) {
                    exec.push_front("pkexec --user $(logname) env DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY ");
                }
                btn->setProperty("cmd", item.exec);
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
    const QStringList tokens = line.split('=');
    if (tokens.isEmpty()) {
        return;
    }
    const QString key = tokens.first().trimmed();
    if (key.isEmpty()) {
        return;
    }
    const QString value = (tokens.size() > 1) ? tokens.at(1).trimmed().remove('"') : QString();

    const QString lowerKey = key.toLower();

    if (lowerKey == "category") {
        categories.append(value);
    } else if (lowerKey == "theme") {
        icon_theme = value;
    } else {
        const QStringList keyTokens = key.split(' ');
        if (keyTokens.isEmpty()) {
            return;
        }

        const QString desktop_file = getDesktopFileName(keyTokens.first());
        if (!desktop_file.isEmpty()) {
            ItemInfo info = getDesktopFileInfo(desktop_file);

            if (keyTokens.size() > 1) {
                if (keyTokens.contains("root")) {
                    info.root = "true";
                } else if (keyTokens.contains("user")) {
                    info.root = "user";
                } else {
                    info.root = "false";
                }
                if (keyTokens.contains("alias")) {
                    const int aliasIndex = keyTokens.indexOf("alias");
                    info.name = keyTokens.mid(aliasIndex + 1).join(' ').trimmed().remove('\'').remove('"');
                }
            } else {
                info.root = "false";
            }

            info.category = categories.last();
            category_map.insert(info.category, info);
        }
    }
}

// Open the .list file and process it
void MainWindow::readFile(const QString &file_name)
{
    // Reset categories, category_map when relading the file
    categories.clear();
    category_map.clear();

    QFile file(file_name);
    if (!QFile::exists(file_name)) {
        exit(EXIT_FAILURE);
    }
    custom_name = QFileInfo(file_name).baseName();
    file_location = QFileInfo(file_name).path();
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::critical(this, tr("File Open Error"),
                              tr("Could not open file: ") + file_name + "\n" + tr("Application will close."));
        exit(EXIT_FAILURE);
    }
    const QString text = file.readAll();
    file.close();

    QString name;
    QString comment;
    QRegularExpression re;
    re.setPatternOptions(QRegularExpression::MultilineOption);
    if (lang.section("_", 0, 0) != QLatin1String("en")) {
        re.setPattern("^Name\\[" + lang + "\\]=(.*)$");
        QRegularExpressionMatch nameMatch = re.match(text);
        if (nameMatch.hasMatch()) {
            name = nameMatch.captured(1);
        } else {
            re.setPattern("^Name\\[" + lang.section("_", 0, 0) + "\\]=(.*)$");
            nameMatch = re.match(text);
            if (nameMatch.hasMatch()) {
                name = nameMatch.captured(1);
            }
        }
        re.setPattern("^Comment\\[" + lang + "\\]=(.*)$");
        QRegularExpressionMatch commentMatch = re.match(text);
        if (commentMatch.hasMatch()) {
            comment = commentMatch.captured(1);
        } else {
            re.setPattern("^Comment\\[" + lang.section("_", 0, 0) + "\\]=(.*)$");
            commentMatch = re.match(text);
            if (commentMatch.hasMatch()) {
                comment = commentMatch.captured(1);
            }
        }
    }
    if (name.isEmpty()) { // Fallback if Name is not translated
        re.setPattern("^Name=(.*)$");
        QRegularExpressionMatch nameFallbackMatch = re.match(text);
        if (nameFallbackMatch.hasMatch()) {
            name = nameFallbackMatch.captured(1);
        }
    }
    if (comment.isEmpty()) { // Fallback if Comment is not translated
        re.setPattern("^Comment=(.*)$");
        QRegularExpressionMatch commentFallbackMatch = re.match(text);
        if (commentFallbackMatch.hasMatch()) {
            comment = commentFallbackMatch.captured(1);
        }
    }
    setWindowTitle(name);
    ui->commentLabel->setText(comment);

    const auto lines = text.split("\n");
    for (const QString &line : lines) {
        if (line.startsWith("Name") || line.startsWith("Comment") || line.isEmpty() || line.startsWith("#")) {
            continue;
        }
        processLine(line);
    }
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
    hide();
    displayAboutMsgBox(
        tr("About %1").arg(windowTitle()),
        "<p align=\"center\"><b><h2>" + windowTitle() + "</h2></b></p><p align=\"center\">" + tr("Version: ") + VERSION
            + "</p><p align=\"center\"><h3>" + tr("Custom Toolbox is a tool used for creating a custom launcher")
            + "</h3></p><p align=\"center\"><a href=\"http://mxlinux.org\">http://mxlinux.org</a><br /></p>"
              "<p align=\"center\">"
            + tr("Copyright (c) MX Linux") + "<br /><br /></p>",
        "/usr/share/doc/custom-toolbox/license.html", tr("%1 License").arg(windowTitle()));
    show();
}

void MainWindow::pushHelp_clicked()
{
    const QString url = "/usr/share/doc/custom-toolbox/help.html";
    displayDoc(url, tr("%1 Help").arg(windowTitle()));
}

void MainWindow::textSearch_textChanged(const QString &arg1)
{
    // Remove all items from the layout
    QLayoutItem *childItem = nullptr;
    while ((childItem = ui->gridLayout_btn->takeAt(0)) != nullptr) {
        delete childItem->widget();
        delete childItem;
    }

    // Create a new_map with items that match the search argument
    QMultiMap<QString, ItemInfo> new_map;
    for (const auto &[category, name, comment, icon_name, exec, terminal, root] : category_map) {
        if (name.contains(arg1, Qt::CaseInsensitive) || comment.contains(arg1, Qt::CaseInsensitive)
            || category.contains(arg1, Qt::CaseInsensitive)) {
            new_map.insert(category, {category, name, comment, icon_name, exec, terminal, root});
        }
    }
    addButtons(new_map.empty() ? category_map : new_map);
}

// Add a .desktop file to the ~/.config/autostart
void MainWindow::checkBoxStartup_clicked(bool checked)
{
    const QString &fileName
        = QDir::homePath() + "/.config/autostart/" + custom_name + ".desktop"; // Same custom_name as .list file
    if (checked) {
        QFile file(fileName);
        if (!file.open(QFile::WriteOnly | QFile::Text)) {
            QMessageBox::critical(this, tr("File Open Error"), tr("Could not write file: ") + fileName);
        }
        QTextStream out(&file);
        out << "[Desktop Entry]"
            << "\n"
            << "Name=" << windowTitle() << "\n"
            << "Comment=" << ui->commentLabel->text() << "\n"
            << "Exec="
            << "custom-toolbox " + file_location + "/" + custom_name + ".list"
            << "\n"
            << "Terminal=false"
            << "\n"
            << "Type=Application"
            << "\n"
            << "Icon=custom-toolbox"
            << "\n"
            << "Categories=XFCE;System"
            << "\n"
            << "StartupNotify=false";
        file.close();
    } else {
        QFile::remove(fileName);
    }
}

void MainWindow::pushEdit_clicked()
{
    QString editor = gui_editor;
    // If specified editor doesn't exist get the default one
    if (editor.isEmpty() || QStandardPaths::findExecutable(editor, {defaultPath}).isEmpty()) {
        proc.start("xdg-mime", {"query", "default", "text/plain"});
        proc.waitForFinished();
        QString default_editor = proc.readAllStandardOutput().trimmed();

        // Find first app with .desktop name that matches default_editors
        QString desktop_file
            = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, default_editor, QStandardPaths::LocateFile);
        QFile file(desktop_file);
        if (file.open(QIODevice::ReadOnly)) {
            QString line;
            while (!file.atEnd()) {
                line = file.readLine();
                if (line.contains(QRegularExpression("^Exec="))) {
                    {
                        break;
                    }
                }
            }
            file.close();
            editor = line.remove(QRegularExpression("^Exec=|%u|%U|%f|%F|%c|%C|-b")).trimmed();
        }
        if (editor.isEmpty()) { // Use nano as backup editor
            editor = "nano";
        }
    }

    bool isRoot {getuid() == 0};
    bool isEditorThatElevates = QRegularExpression("(kate|kwrite|featherpad)$").match(editor).hasMatch();
    bool isElectronBased = QRegularExpression("(atom\\.desktop|code\\.desktop)$").match(editor).hasMatch();
    bool isCliEditor = QRegularExpression("nano|vi|vim|nvim|micro|emacs").match(editor).hasMatch();

    QStringList editorCommands;
    if (!QFileInfo(file_name).isWritable() && !isEditorThatElevates && !isElectronBased) {
        editorCommands << "pkexec";
    }

    if (isRoot && (isEditorThatElevates || isElectronBased)) {
        editorCommands << "pkexec --user $(logname)";
    }

    editorCommands << "env DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY";

    if (isCliEditor) {
        editorCommands << "x-terminal-emulator -e";
    }

    QString cmd = editorCommands.join(" ") + " " + editor.toUtf8() + " " + file_name;
    system(cmd.toUtf8());
    readFile(file_name);
    setGui();
}
