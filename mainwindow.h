/**********************************************************************
 *  customtoolbox.h
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QCommandLineParser>
#include <QDialog>
#include <QLocale>
#include <QMessageBox>
#include <QMultiMap>
#include <QProcess>

#include "flatbutton.h"

class QIcon;

namespace Ui
{
class MainWindow;
}

class MainWindow : public QDialog
{
    Q_OBJECT

public:
    explicit MainWindow(const QCommandLineParser &arg_parser, QWidget *parent = nullptr);
    ~MainWindow() override;

    QIcon findIcon(const QString &icon_name);
    static void fixExecItem(QString *item);
    static void fixNameItem(QString *item);
    QString getDesktopFileName(const QString &app_name) const;
    QString getFileName();
    QStringList getDesktopFileInfo(const QString &file_name);
    void addButtons(const QMultiMap<QString, QStringList> &map);
    void processLine(const QString &line);
    void readFile(const QString &file_name);
    void setConnections();
    void setGui();
    void setup();

private slots:
    void closeEvent(QCloseEvent * /*unused*/) override;
    void resizeEvent(QResizeEvent *event) override;
    void btn_clicked();
    void pushAbout_clicked();
    void pushEdit_clicked();
    void pushHelp_clicked();
    void checkBoxStartup_clicked(bool checked);
    void textSearch_textChanged(const QString &arg1);

private:
    FlatButton *btn {};
    QMultiMap<QString, QStringList> category_map;
    QProcess proc;
    QSize icon_size;
    QString base_name;
    QString file_location;
    QString file_name;
    QString gui_editor;
    QString icon_theme;
    QLocale locale;
    QString lang = locale.name();
    QString local_dir;
    QString version;
    QStringList categories;
    bool firstRun {true};
    bool hideGUI {};
    const QStringList defaultPath {qEnvironmentVariable("PATH").split(":") << "/usr/sbin"};
    int col_count;
    int fixed_number_col {};
    int max_elements {};
    int min_height {};
    int min_width {};

    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
