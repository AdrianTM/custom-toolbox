/**********************************************************************
 *  customtoolbox.h
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <flatbutton.h>
#include <QDialog>
#include <QProcess>
#include <QMessageBox>
#include <QMultiMap>
#include <QIcon>

#include "cmd.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QDialog
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    QIcon findIcon(QString icon_name);
    QString fixExecItem(QString item);
    QString fixNameItem(QString item);
    QString getVersion(QString name);
    QString getFileName();
    QString getDesktopFileName(QString app_name);
    QStringList getDesktopFileInfo(QString file_name);
    void addButtons(QMultiMap<QString, QStringList> map);
    void processLine(QString line);
    void readFile(QString file_name);
    void setup();
    void setGui();

public slots:

private slots:
    void btn_clicked();
    void on_buttonAbout_clicked();
    void on_buttonHelp_clicked();
    void on_checkBoxStartup_clicked(bool checked);
    void on_lineSearch_textChanged(const QString &arg1);

    void on_buttonEdit_clicked();

private:
    bool hideGUI;
    int min_height;
    int min_width;
    QString gui_editor;

    Cmd *shell;
    FlatButton *btn;
    QList<QString> categories;
    QMultiMap<QString, QStringList> category_map;
    QString file_location;
    QString file_name;
    QString base_name;
    QString version;

    Ui::MainWindow *ui;
};


#endif // MAINWINDOW_H
