# **********************************************************************
# * Copyright (C) 2017 MX Authors
# *
# * Authors: Adrian
# *          MX Linux <http://mxlinux.org>
# *
# * This file is part of custom-toolbox.
# *
# * custom-toolbox is free software: you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation, either version 3 of the License, or
# * (at your option) any later version.
# *
# * custom-toolbox is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with custom-toolbox.  If not, see <http://www.gnu.org/licenses/>.
# **********************************************************************/

QT       += core gui
CONFIG   += c++11

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = custom-toolbox
TEMPLATE = app


SOURCES += main.cpp\
    mainwindow.cpp \
    flatbutton.cpp

HEADERS  += \
    mainwindow.h \
    flatbutton.h

FORMS    += \
    mainwindow.ui

TRANSLATIONS += translations/custom-toolbox_ca.ts \
                translations/custom-toolbox_de.ts \
                translations/custom-toolbox_el.ts \
                translations/custom-toolbox_es.ts \
                translations/custom-toolbox_fr.ts \
                translations/custom-toolbox_it.ts \
                translations/custom-toolbox_ja.ts \
                translations/custom-toolbox_nl.ts \
                translations/custom-toolbox_ro.ts \
                translations/custom-toolbox_sv.ts

RESOURCES += \
    images.qrc

unix:!macx: LIBS += -lcmd
