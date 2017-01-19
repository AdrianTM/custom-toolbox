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

namespace Ui {
class MainWindow;
}

class MainWindow : public QDialog
{
    Q_OBJECT

public:
    explicit MainWindow(QString arg, QWidget *parent = 0);
    ~MainWindow();

    QString getVersion(QString name);    
    QString getCmdOut(QString cmd);
    QString getFileName();
    void processLine(QString line);
    void readFile(QString file_name);
    void setup();


public slots:

private slots:
    void on_buttonAbout_clicked();
    void on_buttonHelp_clicked();

private:    
    QList<QString> categories;
    QMultiMap<QString, QStringList> category_map;
    QProcess *proc;
    QString file_location;
    QString file_name;
    QString version;


    Ui::MainWindow *ui;

};


#endif // MAINWINDOW_H

