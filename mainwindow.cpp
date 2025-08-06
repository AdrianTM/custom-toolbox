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

#include <algorithm>
#include <QDebug>
#include <QDirIterator>
#include <QFileDialog>
#include <QHash>
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

    setWindowFlags(Qt::Window); // Enable close, minimize, and maximize buttons
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

QIcon MainWindow::findIcon(const QString &iconName)
{
    static QHash<QString, QIcon> iconCache;
    static QIcon defaultIcon;
    static bool defaultIconLoaded = false;

    if (iconName.isEmpty()) {
        if (!defaultIconLoaded) {
            defaultIcon = findIcon(QStringLiteral("utilities-terminal"));
            defaultIconLoaded = true;
        }
        return defaultIcon;
    }

    // Check cache first
    auto cacheIt = iconCache.find(iconName);
    if (cacheIt != iconCache.end()) {
        return *cacheIt;
    }

    // Check if the icon name is an absolute path and exists
    if (QFileInfo(iconName).isAbsolute() && QFile::exists(iconName)) {
        QIcon icon(iconName);
        iconCache.insert(iconName, icon);
        return icon;
    }

    // Prepare regular expression to strip extension
    static const QRegularExpression re(R"(\.(png|svg|xpm)$)");
    QString nameNoExt = iconName;
    nameNoExt.remove(re);

    // Set theme if specified
    if (!icon_theme.isEmpty()) {
        QIcon::setThemeName(icon_theme);
    }

    // Return the themed icon if available
    if (QIcon::hasThemeIcon(nameNoExt)) {
        QIcon icon = QIcon::fromTheme(nameNoExt);
        iconCache.insert(iconName, icon);
        return icon;
    }

    // Define common search paths for icons
    QStringList searchPaths {QDir::homePath() + QStringLiteral("/.local/share/icons/"),
                             QStringLiteral("/usr/share/pixmaps/"),
                             QStringLiteral("/usr/local/share/icons/"),
                             QStringLiteral("/usr/share/icons/"),
                             QStringLiteral("/usr/share/icons/hicolor/scalable/apps/"),
                             QStringLiteral("/usr/share/icons/hicolor/48x48/apps/"),
                             QStringLiteral("/usr/share/icons/Adwaita/48x48/legacy/")};

    // Optimization: search first for the full iconName with the specified extension
    auto it = std::find_if(searchPaths.cbegin(), searchPaths.cend(),
                           [&](const QString &path) { return QFile::exists(path + iconName); });
    if (it != searchPaths.cend()) {
        QIcon icon(*it + iconName);
        iconCache.insert(iconName, icon);
        return icon;
    }

    // Search for the icon without extension in the specified paths
    for (const QString &path : searchPaths) {
        if (!QFile::exists(path)) {
            continue;
        }
        for (const auto &ext : {QStringLiteral(".png"), QStringLiteral(".svg"), QStringLiteral(".xpm")}) {
            const QString file = path + nameNoExt + ext;
            if (QFile::exists(file)) {
                QIcon icon(file);
                iconCache.insert(iconName, icon);
                return icon;
            }
        }
    }

    // If the icon is "utilities-terminal" and not found, return the default icon if it's already loaded
    if (iconName == QStringLiteral("utilities-terminal")) {
        if (!defaultIconLoaded) {
            defaultIcon = QIcon();
            defaultIconLoaded = true;
        }
        iconCache.insert(iconName, defaultIcon);
        return defaultIcon;
    }

    // If the icon is not "utilities-terminal", try to load the default icon
    QIcon fallbackIcon = findIcon(QStringLiteral("utilities-terminal"));
    iconCache.insert(iconName, fallbackIcon);
    return fallbackIcon;
}

// Strip %f, %F, %U, etc. if exec expects a file name since it's called without an argument from this launcher.
void MainWindow::fixExecItem(QString *item)
{
    static const QRegularExpression execPattern(R"( %[a-zA-Z])");
    item->remove(execPattern);
}

void MainWindow::fixNameItem(QString *item)
{
    static const QString oldName = "System Profiler and Benchmark";
    static const QString newName = "System Information";

    if (*item == oldName) {
        *item = newName;
    }
}

void MainWindow::setup()
{
    setWindowTitle(tr("Custom Toolbox"));
    adjustSize();

    const int default_icon_size = 40;

    QSettings settings("/etc/custom-toolbox/custom-toolbox.conf", QSettings::NativeFormat);
    hideGUI = settings.value("hideGUI", false).toBool();
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
        int exitCode = system(cmd.toUtf8());
        if (exitCode != 0) {
            QMessageBox::warning(this, tr("Execution Error"), tr("Failed to execute command: %1").arg(cmd));
        }
        show();
    } else {
        QStringList arguments = QProcess::splitCommand(cmd);
        QString program = arguments.takeFirst();
        if (!QProcess::startDetached(program, arguments)) {
            QMessageBox::warning(this, tr("Execution Error"), tr("Failed to start program: %1").arg(program));
        }
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
    // Static cache to avoid repeated filesystem searches
    static QHash<QString, QString> desktopFileCache;
    static QHash<QString, bool> executableCache; // Cache executable existence
    
    // Check cache first
    if (desktopFileCache.contains(appName)) {
        return desktopFileCache[appName];
    }
    
    QString result;
    
    // Search for .desktop files in standard applications locations
    static const QStringList searchPaths = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);

    // Search for .desktop file in each path
    const QString desktopFileName = appName + ".desktop";
    for (const QString &searchPath : searchPaths) {
        const QString fullPath = QDir(searchPath).absoluteFilePath(desktopFileName);
        if (QFile::exists(fullPath)) {
            result = fullPath;
            break;
        }
    }
    
    // If .desktop file not found, check executable cache first
    if (result.isEmpty()) {
        if (executableCache.contains(appName)) {
            // Use cached result
            result = executableCache[appName] ? appName : QString();
        } else {
            // Optimized executable search - check common paths directly first
            static const QStringList commonPaths = {"/usr/bin/", "/bin/", "/usr/sbin/", "/sbin/"};
            bool found = false;
            
            for (const QString &path : commonPaths) {
                if (QFile::exists(path + appName)) {
                    found = true;
                    break;
                }
            }
            
            // Fallback to slower QStandardPaths::findExecutable only if not found
            if (!found) {
                const QString executablePath = QStandardPaths::findExecutable(appName, defaultPath);
                found = !executablePath.isEmpty();
            }
            
            // Cache the executable existence result
            executableCache[appName] = found;
            result = found ? appName : QString();
        }
    }
    
    // Cache the final result (even if empty to avoid repeated failed lookups)
    desktopFileCache[appName] = result;
    return result;
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

    // Function to attempt matching localized fields first, then fall back to non-localized
    auto matchLocalizedField = [&](const QString &field) -> QString {
        QString value = matchPattern("^" + field + "\\[" + lang + "\\]=(.*)$");
        if (value.isEmpty()) {
            value = matchPattern("^" + field + "\\[" + lang.section('_', 0, 0) + "\\]=(.*)$");
        }
        if (value.isEmpty()) {
            value = matchPattern("^" + field + "=(.*)$");
        }
        return value;
    };

    static const QRegularExpression mxPrefixPattern("^MX ");
    item.name = matchLocalizedField("Name").remove(mxPrefixPattern);
    item.comment = matchLocalizedField("Comment");
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
    const int maxCols = fixed_number_col != 0 ? fixed_number_col : width() / 200;

    for (const auto &category : map.uniqueKeys()) {
        if (!category_map.values(category).isEmpty()) {
            addCategoryLabel(category, row, col);

            for (const auto &item : map.values(category)) {
                addItemButton(item, row, col, maxCols);
            }
        }
        addEmptyRowIfNeeded(category, map, row, col);
    }
    ui->gridLayout_btn->setRowStretch(row, 1);
}

void MainWindow::addCategoryLabel(const QString &category, int &row, int &col)
{
    auto *label = new QLabel(category, this);
    QFont font;
    font.setBold(true);
    font.setUnderline(true);
    label->setFont(font);
    ui->gridLayout_btn->addWidget(label, ++row, col);
    ui->gridLayout_btn->setRowStretch(++row, 0);
}

void MainWindow::addItemButton(const ItemInfo &item, int &row, int &col, int maxCols)
{
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

    prepareCommand(item, cmd);
    btn->setProperty("cmd", cmd);
    connect(btn, &QPushButton::clicked, this, &MainWindow::btn_clicked);

    if (++col >= maxCols) {
        col = 0;
        ++row;
    }
}

void MainWindow::prepareCommand(const ItemInfo &item, QString &cmd)
{
    if (item.terminal) {
        cmd.prepend("x-terminal-emulator -e ");
    }
    if (item.root && getuid() != 0) {
        cmd.prepend("pkexec env DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY ");
    } else if (item.user && getuid() == 0) {
        cmd = QString("pkexec --user $(logname) env DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY ") + cmd;
    }
}

void MainWindow::addEmptyRowIfNeeded(const QString &category, const QMultiMap<QString, ItemInfo> &map, int &row,
                                     int &col)
{
    if (category != map.lastKey()) {
        col = 0;
        auto *line = new QFrame();
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        ui->gridLayout_btn->addWidget(line, ++row, col, 1, -1);
        ui->gridLayout_btn->setRowStretch(row, 0);
    }
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
    if (line.isEmpty()) {
        return;
    }
    const int splitPos = line.indexOf('=');
    const QString key = (splitPos > 0) ? line.left(splitPos).trimmed() : line.trimmed();
    if (key.isEmpty()) {
        return;
    }
    const QString value = (splitPos > 0) ? QString(line.mid(splitPos + 1)).trimmed().remove('"') : QString();
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
                const bool hasRoot = keyTokens.contains("root");
                const bool hasUser = keyTokens.contains("user");
                const bool hasAlias = keyTokens.contains("alias");

                info.root = hasRoot;
                info.user = hasUser;

                if (hasAlias) {
                    const int aliasIndex = keyTokens.indexOf("alias");
                    if (aliasIndex >= 0 && aliasIndex + 1 < keyTokens.size()) {
                        info.name = keyTokens.mid(aliasIndex + 1).join(' ').trimmed().remove('\'').remove('"');
                    } else {
                        qWarning() << "Alias keyword found but no valid alias name provided.";
                    }
                }
            }

            if (!categories.isEmpty()) {
                info.category = categories.last();
                category_map.insert(info.category, info);
            }
        }
    }
}

// Open the .list file and process it
void MainWindow::readFile(const QString &file_name)
{
    categories.clear();
    category_map.clear();

    QFile file(file_name);
    if (!file.exists()) {
        QMessageBox::critical(this, tr("File Not Found"), tr("The file %1 does not exist.").arg(file_name));
        return;
    }

    custom_name = QFileInfo(file_name).baseName();
    file_location = QFileInfo(file_name).path();

    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::critical(this, tr("File Open Error"), tr("Could not open file: ") + file_name);
        return;
    }

    const QString text = file.readAll();
    file.close();

    QString name = extractPattern(text, "Name");
    QString comment = extractPattern(text, "Comment");

    setWindowTitle(name);
    ui->commentLabel->setText(comment);

    const auto lines = text.split('\n');
    static const QRegularExpression skipPattern("^(Name|Comment|#|$).*");
    for (const QString &line : lines) {
        if (!skipPattern.match(line).hasMatch()) {
            processLine(line);
        }
    }
}

QString MainWindow::extractPattern(const QString &text, const QString &key)
{
    static QHash<QString, QRegularExpression> regexCache;
    
    QString pattern = QString("^%1\\[").arg(key) + lang + "]=(.*)$";
    QString fallbackPattern = QString("^%1=(.*)$").arg(key);
    
    // Check cache for this pattern
    QRegularExpression re;
    if (regexCache.contains(pattern)) {
        re = regexCache[pattern];
    } else {
        re = QRegularExpression(pattern, QRegularExpression::MultilineOption);
        regexCache[pattern] = re;
    }
    QRegularExpressionMatch match = re.match(text);

    if (match.hasMatch()) {
        return match.captured(1);
    }

    pattern = QString("^%1\\[").arg(key) + lang.section('_', 0, 0) + "]=(.*)$";
    if (regexCache.contains(pattern)) {
        re = regexCache[pattern];
    } else {
        re = QRegularExpression(pattern, QRegularExpression::MultilineOption);
        regexCache[pattern] = re;
    }
    match = re.match(text);

    if (match.hasMatch()) {
        return match.captured(1);
    }

    if (regexCache.contains(fallbackPattern)) {
        re = regexCache[fallbackPattern];
    } else {
        re = QRegularExpression(fallbackPattern, QRegularExpression::MultilineOption);
        regexCache[fallbackPattern] = re;
    }
    match = re.match(text);

    return match.hasMatch() ? match.captured(1) : QString();
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
    QString aboutText
        = QString("<p align=\"center\"><b><h2>%1</h2></b></p>"
                  "<p align=\"center\">%2 %3</p>"
                  "<p align=\"center\"><h3>%4</h3></p>"
                  "<p align=\"center\"><a href=\"http://mxlinux.org\">http://mxlinux.org</a><br /></p>"
                  "<p align=\"center\">%5<br /><br /></p>")
              .arg(windowTitle(), tr("Version:"), VERSION,
                   tr("Custom Toolbox is a tool used for creating a custom launcher"), tr("Copyright (c) MX Linux"));

    displayAboutMsgBox(tr("About %1").arg(windowTitle()), aboutText, "/usr/share/doc/custom-toolbox/license.html",
                       tr("%1 License").arg(windowTitle()));
    show();
}

void MainWindow::pushHelp_clicked()
{
    const QString url = "/usr/share/doc/custom-toolbox/help.html";
    displayDoc(url, tr("%1 Help").arg(windowTitle()));
}

void MainWindow::textSearch_textChanged(const QString &searchText)
{
    // Create a lambda function to check if an item matches the search text
    auto matchesSearchText = [&searchText](const ItemInfo &item) {
        return item.name.contains(searchText, Qt::CaseInsensitive)
               || item.comment.contains(searchText, Qt::CaseInsensitive)
               || item.category.contains(searchText, Qt::CaseInsensitive);
    };

    // Filter category_map to only include items that match the search text
    QMultiMap<QString, ItemInfo> filtered_map;
    for (const auto &item : category_map) {
        if (matchesSearchText(item)) {
            filtered_map.insert(item.category, item);
        }
    }

    // Update the buttons with the filtered map or the original map if no matches found
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
    QStringList cmdList = buildEditorCommand(editor) << editor << file_name;
    QProcess::startDetached(cmdList.join(' '));
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
    if (desktop_file.isEmpty() || !file.open(QIODevice::ReadOnly)) {
        return "nano"; // Fallback to nano
    }

    QTextStream in(&file);
    QString line;
    while (in.readLineInto(&line)) {
        if (line.startsWith("Exec=")) {
            // Extract the command from the Exec line and remove common desktop file parameters
            static const QRegularExpression execPrefixPattern("^Exec=");
            static const QRegularExpression paramPattern("(%[a-zA-Z]|%[a-zA-Z]{1,2}|-{1,2}[a-zA-Z-]+)\\b");
            return line.remove(execPrefixPattern)
                .remove(paramPattern)
                .trimmed();
        }
    }

    return "nano"; // Fallback to nano
}

QStringList MainWindow::buildEditorCommand(const QString &editor)
{
    const bool isRoot = getuid() == 0;
    static const QRegularExpression elevatesPattern(R"((kate|kwrite|featherpad|code|codium)$)");
    static const QRegularExpression cliPattern(R"(nano|vi|vim|nvim|micro|emacs)");
    const bool isEditorThatElevates = elevatesPattern.match(editor).hasMatch();
    const bool isCliEditor = cliPattern.match(editor).hasMatch();

    QStringList editorCommands;
    if (isRoot && isEditorThatElevates) {
        editorCommands << "pkexec --user $(logname)";
    } else if (!QFileInfo(file_name).isWritable() && !isEditorThatElevates) {
        editorCommands << "pkexec";
    }

    editorCommands << "env DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY";

    if (isCliEditor) {
        editorCommands << "x-terminal-emulator -e";
    }

    return editorCommands;
}
