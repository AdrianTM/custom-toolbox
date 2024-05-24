/**********************************************************************
 *  MainWindow.cpp
 **********************************************************************
 * Copyright (C) 2017-2024 MX Authors
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
#include <QProcess>
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
      file_location {"/etc/custom-toolbox"}
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
    static QIcon defaultIcon;
    static bool defaultIconLoaded = false;

    if (icon_name.isEmpty()) {
        if (!defaultIconLoaded) {
            defaultIcon = findIcon("utilities-terminal");
            defaultIconLoaded = true;
        }
        return defaultIcon;
    }

    // Check if the icon name is an absolute path and exists
    if (QFileInfo(icon_name).isAbsolute() && QFile::exists(icon_name)) {
        return QIcon(icon_name);
    }

    // Prepare regular expression to strip extension
    static const QRegularExpression re(R"(\.(png|svg|xpm)$)");
    QString name_noext = icon_name;
    name_noext.remove(re);

    // Set icon theme if specified
    if (!icon_theme.isEmpty()) {
        QIcon::setThemeName(icon_theme);
    }

    // Return the themed icon if available
    if (QIcon::hasThemeIcon(name_noext)) {
        return QIcon::fromTheme(name_noext);
    }

    // Define common search paths for icons
    QStringList search_paths {QDir::homePath() + "/.local/share/icons/",
                              "/usr/share/pixmaps/",
                              "/usr/local/share/icons/",
                              "/usr/share/icons/",
                              "/usr/share/icons/hicolor/48x48/apps/",
                              "/usr/share/icons/Adwaita/48x48/legacy/"};

    // Optimization: search first for the full icon_name with the specified extension
    auto it = std::find_if(search_paths.cbegin(), search_paths.cend(),
                           [&](const QString &path) { return QFile::exists(path + icon_name); });
    if (it != search_paths.cend()) {
        return QIcon(*it + icon_name);
    }

    // Search for the icon without extension in the specified paths
    for (const QString &path : search_paths) {
        if (!QFile::exists(path)) {
            continue;
        }
        for (const char *ext : {".png", ".svg", ".xpm"}) {
            QString file = path + name_noext + ext;
            if (QFile::exists(file)) {
                return QIcon(file);
            }
        }
    }

    // If the icon is "utilities-terminal" and not found, return the default icon if it's already loaded
    if (icon_name == "utilities-terminal") {
        if (!defaultIconLoaded) {
            defaultIcon = QIcon();
            defaultIconLoaded = true;
        }
        return defaultIcon;
    }

    // If the icon is not "utilities-terminal", try to load the default icon
    return findIcon("utilities-terminal");
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
    clearGridLayout();
    adjustSize();
    setMinimumSize(min_width, min_height);

    QSettings settings(QApplication::organizationName(),
                       QApplication::applicationName() + '_' + QFileInfo(file_name).baseName());
    const auto size = this->size();
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
        if (isMaximized()) { // Add option to resize if maximized
            resize(size);
            centerWindow();
        }
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
        QStringList arguments = QProcess::splitCommand(cmd);
        QString program = arguments.takeFirst();
        QProcess::startDetached(program, arguments);
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
    if (firstRun || event->oldSize().width() == event->size().width() || fixed_number_col != 0) {
        firstRun = false;
        return;
    }

    const int item_size = 200; // Determined through trial and error
    const int new_count = width() / item_size;

    if (new_count == col_count || (new_count >= max_elements && col_count == max_elements)) {
        return;
    }

    col_count = new_count;

    if (ui->textSearch->text().isEmpty()) {
        addButtons(category_map);
    } else {
        textSearch_textChanged(ui->textSearch->text());
    }
}

// Select .list file to open
QString MainWindow::getFileName()
{
    while (true) {
        QString fileName
            = QFileDialog::getOpenFileName(this, tr("Open List File"), file_location, tr("List Files (*.list)"));
        if (fileName.isEmpty()) {
            QMessageBox::critical(this, tr("File Selection Error"), tr("No file selected. Application will now exit."));
            exit(EXIT_FAILURE);
        }
        if (QFile::exists(fileName)) {
            return fileName;
        } else {
            auto userChoice = QMessageBox::critical(this, tr("File Open Error"),
                                                    tr("Could not open file. Do you want to try again?"),
                                                    QMessageBox::Yes | QMessageBox::No);
            if (userChoice == QMessageBox::No) {
                exit(EXIT_FAILURE);
            }
        }
    }
}

// Find the .desktop file for the given app name
QString MainWindow::getDesktopFileName(const QString &appName) const
{
    // Search for .desktop files in standard applications locations
    QStringList searchPaths = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    searchPaths.append(defaultPath); // Include default path for executable search

    for (const QString &searchPath : searchPaths) {
        QDir searchDir(searchPath);
        QStringList desktopFiles = searchDir.entryList({appName + ".desktop"}, QDir::Files, QDir::NoSort);
        if (!desktopFiles.isEmpty()) {
            return searchDir.absoluteFilePath(desktopFiles.first());
        }
    }
    // If .desktop file not found, fallback to finding the executable
    QString executablePath = QStandardPaths::findExecutable(appName, {defaultPath});
    if (!executablePath.isEmpty()) {
        return QFileInfo(executablePath).fileName(); // Return just the executable name
    }
    return {};
}

// Return the app info needed for the button
MainWindow::ItemInfo MainWindow::getDesktopFileInfo(const QString &fileName)
{
    ItemInfo item;

    // If not a .desktop file
    if (!fileName.endsWith(".desktop")) {
        item.name = item.icon_name = item.exec = fileName;
        item.root = item.user = false;
        item.terminal = true;
        return item;
    }

    QFile file(fileName);
    if (!file.open(QFile::Text | QFile::ReadOnly)) {
        return {};
    }
    const QString text = file.readAll();
    file.close();

    // Helper lambda to search for a pattern and extract the first capture group
    auto matchPattern = [&text](const QString &pattern) -> QString {
        QRegularExpression re(pattern, QRegularExpression::MultilineOption);
        auto match = re.match(text);
        return match.hasMatch() ? match.captured(1) : QString();
    };

    // Attempt to match localized fields first, then fall back to non-localized
    QString langPrefix = lang.section('_', 0, 0);
    item.name = matchPattern("^Name\\[" + lang + "\\]=(.*)$");
    if (item.name.isEmpty()) {
        item.name = matchPattern("^Name\\[" + langPrefix + "\\]=(.*)$");
    }
    if (item.name.isEmpty()) {
        item.name = matchPattern("^Name=(.*)$").remove(QRegularExpression("^MX "));
    }

    item.comment = matchPattern("^Comment\\[" + lang + "\\]=(.*)$");
    if (item.comment.isEmpty()) {
        item.comment = matchPattern("^Comment\\[" + langPrefix + "\\]=(.*)$");
    }
    if (item.comment.isEmpty()) {
        item.comment = matchPattern("^Comment=(.*)$");
    }

    item.exec = matchPattern("^Exec=(.*)$");
    item.icon_name = matchPattern("^Icon=(.*)$");
    item.terminal = matchPattern("^Terminal=(.*)$").toLower() == "true";

    return item;
}

void MainWindow::addButtons(const QMultiMap<QString, ItemInfo> &map)
{
    clearGridLayout();
    int col = 0;
    int row = 0;
    const int max = fixed_number_col != 0 ? fixed_number_col : width() / 200;

    for (const auto &category : map.uniqueKeys()) {
        if (!category_map.values(category).isEmpty()) {
            auto *label = new QLabel(category, this);
            label->setFont(QFont("", -1, QFont::Bold, true)); // Set font bold and underlined
            ui->gridLayout_btn->addWidget(label, ++row, col);
            ui->gridLayout_btn->setRowStretch(++row, 0);

            for (const auto &item : map.values(category)) {
                col_count = std::max(col_count, col + 1);
                QString name = item.name;
                QString cmd = item.exec;
                fixNameItem(&name);
                fixExecItem(&cmd);

                auto *btn = new FlatButton(name);
                btn->setIconSize(icon_size);
                btn->setToolTip(item.comment);
                auto icon = findIcon(item.icon_name);
                btn->setIcon(icon.isNull() ? QIcon::fromTheme("utilities-terminal") : icon);
                ui->gridLayout_btn->addWidget(btn, row, col);
                ui->gridLayout_btn->setRowStretch(row, 0);

                if (item.terminal) {
                    cmd.prepend("x-terminal-emulator -e ");
                }
                if (item.root && getuid() != 0) {
                    cmd.prepend("pkexec env DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY ");
                } else if (item.user && getuid() == 0) {
                    cmd = QString("pkexec --user $(logname) env DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY ") + cmd;
                }
                btn->setProperty("cmd", cmd);
                connect(btn, &QPushButton::clicked, this, &MainWindow::btn_clicked);

                if (++col >= max) {
                    col = 0;
                    ++row;
                }
            }
        }
        // Add empty row if it's not the last key
        if (category != map.lastKey()) {
            col = 0;
            auto *line = new QFrame();
            line->setFrameShape(QFrame::HLine);
            line->setFrameShadow(QFrame::Sunken);
            ui->gridLayout_btn->addWidget(line, ++row, col, 1, -1);
            ui->gridLayout_btn->setRowStretch(row, 0);
        }
    }
    ui->gridLayout_btn->setRowStretch(row, 1);
}

void MainWindow::centerWindow()
{
    const auto screenGeometry = QApplication::primaryScreen()->geometry();
    const auto x = (screenGeometry.width() - width()) / 2;
    const auto y = (screenGeometry.height() - height()) / 2;
    move(x, y);
}

// Remove all items from the grid layout
void MainWindow::clearGridLayout()
{
    QLayoutItem *childItem {};
    while ((childItem = ui->gridLayout_btn->takeAt(0)) != nullptr) {
        // Delete the widget and the layout item
        delete childItem->widget();
        delete childItem;
    }
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
                info.root = keyTokens.contains("root");
                info.user = keyTokens.contains("user");
                if (keyTokens.contains("alias")) {
                    const int aliasIndex = keyTokens.indexOf("alias");
                    info.name = keyTokens.mid(aliasIndex + 1).join(' ').trimmed().remove('\'').remove('"');
                }
            }
            info.category = categories.last();
            category_map.insert(info.category, info);
        }
    }
}

// Open the .list file and process it
void MainWindow::readFile(const QString &file_name)
{
    // Reset categories and category_map when reloading the file
    categories.clear();
    category_map.clear();

    QFile file(file_name);
    if (!file.exists()) {
        exit(EXIT_FAILURE);
    }
    custom_name = QFileInfo(file_name).baseName();
    file_location = QFileInfo(file_name).path();
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::critical(this, tr("File Open Error"),
                              tr("Could not open file: ") + file_name + '\n' + tr("Application will close."));
        exit(EXIT_FAILURE);
    }
    const QString text = file.readAll();
    file.close();

    QString name;
    QString comment;
    QRegularExpression re;
    re.setPatternOptions(QRegularExpression::MultilineOption);
    if (lang.section('_', 0, 0) != "en") {
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
            re.setPattern("^Comment\\[" + lang.section('_', 0, 0) + "\\]=(.*)$");
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

    const auto lines = text.splitRef('\n');
    QRegularExpression skipPattern("^(Name|Comment|#|$).*");
    for (const QStringRef &line : lines) {
        if (skipPattern.match(line).hasMatch()) {
            continue;
        }
        processLine(line.toString());
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

void MainWindow::textSearch_textChanged(const QString &searchText)
{
    // Filter category_map to only include items that match the search text
    auto filtered_map = std::accumulate(
        category_map.cbegin(), category_map.cend(), QMultiMap<QString, ItemInfo> {},
        [&searchText](auto &map, const auto &item) {
            const auto &[category, name, comment, icon_name, exec, terminal, root, user] = item;
            if (name.contains(searchText, Qt::CaseInsensitive) || comment.contains(searchText, Qt::CaseInsensitive)
                || category.contains(searchText, Qt::CaseInsensitive)) {
                map.insert(category, {category, name, comment, icon_name, exec, terminal, root, user});
            }
            return map;
        });
    addButtons(filtered_map.isEmpty() ? category_map : filtered_map);
}

// Add a .desktop file to the ~/.config/autostart
void MainWindow::checkBoxStartup_clicked(bool checked)
{
    const QString fileName
        = QDir::homePath() + "/.config/autostart/" + custom_name + ".desktop"; // Same name as .list file
    if (checked) {
        if (QFile file(fileName); file.open(QFile::WriteOnly | QFile::Text)) {
            QTextStream out(&file);
            out << "[Desktop Entry]\n"
                << "Name=" << windowTitle() << '\n'
                << "Comment=" << ui->commentLabel->text() << '\n'
                << "Exec=custom-toolbox " << file_location << '/' << custom_name << ".list\n"
                << "Terminal=false\n"
                << "Type=Application\n"
                << "Icon=custom-toolbox\n"
                << "Categories=XFCE;System\n"
                << "StartupNotify=false";
        } else {
            QMessageBox::critical(this, tr("File Open Error"), tr("Could not write file: %1").arg(fileName));
        }
    } else {
        if (!QFile::remove(fileName)) {
            QMessageBox::warning(this, tr("File Removal Error"), tr("Could not remove file: %1").arg(fileName));
        }
    }
}

void MainWindow::pushEdit_clicked()
{
    QString editor = gui_editor.isEmpty() || QStandardPaths::findExecutable(gui_editor, {defaultPath}).isEmpty()
                         ? getDefaultEditor()
                         : gui_editor;

    QStringList editorCommands = buildEditorCommand(editor);

    QString cmd = editorCommands.join(' ') + ' ' + editor + ' ' + file_name;
    system(cmd.toUtf8());
    readFile(file_name);
    setGui();
}

QString MainWindow::getDefaultEditor()
{
    QProcess proc;
    proc.start("xdg-mime", {"query", "default", "text/plain"});
    proc.waitForFinished();
    QString default_editor = proc.readAllStandardOutput().trimmed();
    QString desktop_file
        = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, default_editor, QStandardPaths::LocateFile);

    QFile file(desktop_file);
    if (file.open(QIODevice::ReadOnly)) {
        QTextStream in(&file);
        QString line;
        while (in.readLineInto(&line)) {
            if (line.startsWith("Exec=")) {
                break;
            }
        }
        file.close();
        return line.remove(QRegularExpression("^Exec=|%u|%U|%f|%F|%c|%C|-b")).trimmed();
    }
    return "nano"; // Use nano as backup editor
}

QStringList MainWindow::buildEditorCommand(const QString &editor)
{
    bool isRoot = getuid() == 0;
    bool isEditorThatElevates = QRegularExpression("(kate|kwrite|featherpad)$").match(editor).hasMatch();
    bool isElectronBased = QRegularExpression("(atom\\.desktop|code\\.desktop)$").match(editor).hasMatch();
    bool isCliEditor = QRegularExpression("nano|vi|vim|nvim|micro|emacs").match(editor).hasMatch();

    QStringList editorCommands;
    if (!QFileInfo(file_name).isWritable()) {
        if (!isEditorThatElevates && !isElectronBased) {
            editorCommands << "pkexec";
        } else if (isRoot) {
            editorCommands << "pkexec --user $(logname)";
        }
    }

    editorCommands << "env DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY";

    if (isCliEditor) {
        editorCommands << "x-terminal-emulator -e";
    }

    return editorCommands;
}
