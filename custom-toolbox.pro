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

QT       += core gui widgets
CONFIG   += c++1z

TARGET = custom-toolbox
TEMPLATE = app

# The following define makes your compiler warn you if you use any
# feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += main.cpp\
    mainwindow.cpp \
    flatbutton.cpp \
    about.cpp

HEADERS  += \
    mainwindow.h \
    flatbutton.h \
    about.h

FORMS    += \
    mainwindow.ui

TRANSLATIONS += translations/custom-toolbox_af.ts \
                translations/custom-toolbox_am.ts \
                translations/custom-toolbox_ar.ts \
                translations/custom-toolbox_ast.ts \
                translations/custom-toolbox_be.ts \
                translations/custom-toolbox_bg.ts \
                translations/custom-toolbox_bn.ts \
                translations/custom-toolbox_bs_BA.ts \
                translations/custom-toolbox_bs.ts \
                translations/custom-toolbox_ca.ts \
                translations/custom-toolbox_ceb.ts \
                translations/custom-toolbox_co.ts \
                translations/custom-toolbox_cs.ts \
                translations/custom-toolbox_cy.ts \
                translations/custom-toolbox_da.ts \
                translations/custom-toolbox_de.ts \
                translations/custom-toolbox_el.ts \
                translations/custom-toolbox_en_GB.ts \
                translations/custom-toolbox_en.ts \
                translations/custom-toolbox_en_US.ts \
                translations/custom-toolbox_eo.ts \
                translations/custom-toolbox_es_ES.ts \
                translations/custom-toolbox_es.ts \
                translations/custom-toolbox_et.ts \
                translations/custom-toolbox_eu.ts \
                translations/custom-toolbox_fa.ts \
                translations/custom-toolbox_fi_FI.ts \
                translations/custom-toolbox_fil_PH.ts \
                translations/custom-toolbox_fil.ts \
                translations/custom-toolbox_fi.ts \
                translations/custom-toolbox_fr_BE.ts \
                translations/custom-toolbox_fr.ts \
                translations/custom-toolbox_fy.ts \
                translations/custom-toolbox_ga.ts \
                translations/custom-toolbox_gd.ts \
                translations/custom-toolbox_gl_ES.ts \
                translations/custom-toolbox_gl.ts \
                translations/custom-toolbox_gu.ts \
                translations/custom-toolbox_ha.ts \
                translations/custom-toolbox_haw.ts \
                translations/custom-toolbox_he_IL.ts \
                translations/custom-toolbox_he.ts \
                translations/custom-toolbox_hi.ts \
                translations/custom-toolbox_hr.ts \
                translations/custom-toolbox_ht.ts \
                translations/custom-toolbox_hu.ts \
                translations/custom-toolbox_hy_AM.ts \
                translations/custom-toolbox_hye.ts \
                translations/custom-toolbox_hy.ts \
                translations/custom-toolbox_id.ts \
                translations/custom-toolbox_ie.ts \
                translations/custom-toolbox_is.ts \
                translations/custom-toolbox_it.ts \
                translations/custom-toolbox_ja.ts \
                translations/custom-toolbox_jv.ts \
                translations/custom-toolbox_kab.ts \
                translations/custom-toolbox_ka.ts \
                translations/custom-toolbox_kk.ts \
                translations/custom-toolbox_km.ts \
                translations/custom-toolbox_kn.ts \
                translations/custom-toolbox_ko.ts \
                translations/custom-toolbox_ku.ts \
                translations/custom-toolbox_ky.ts \
                translations/custom-toolbox_lb.ts \
                translations/custom-toolbox_lo.ts \
                translations/custom-toolbox_lt.ts \
                translations/custom-toolbox_lv.ts \
                translations/custom-toolbox_mg.ts \
                translations/custom-toolbox_mi.ts \
                translations/custom-toolbox_mk.ts \
                translations/custom-toolbox_ml.ts \
                translations/custom-toolbox_mn.ts \
                translations/custom-toolbox_mr.ts \
                translations/custom-toolbox_ms.ts \
                translations/custom-toolbox_mt.ts \
                translations/custom-toolbox_my.ts \
                translations/custom-toolbox_nb_NO.ts \
                translations/custom-toolbox_nb.ts \
                translations/custom-toolbox_ne.ts \
                translations/custom-toolbox_nl_BE.ts \
                translations/custom-toolbox_nl.ts \
                translations/custom-toolbox_nn.ts \
                translations/custom-toolbox_ny.ts \
                translations/custom-toolbox_oc.ts \
                translations/custom-toolbox_or.ts \
                translations/custom-toolbox_pa.ts \
                translations/custom-toolbox_pl.ts \
                translations/custom-toolbox_ps.ts \
                translations/custom-toolbox_pt_BR.ts \
                translations/custom-toolbox_pt.ts \
                translations/custom-toolbox_ro.ts \
                translations/custom-toolbox_rue.ts \
                translations/custom-toolbox_ru_RU.ts \
                translations/custom-toolbox_ru.ts \
                translations/custom-toolbox_rw.ts \
                translations/custom-toolbox_sd.ts \
                translations/custom-toolbox_si.ts \
                translations/custom-toolbox_sk.ts \
                translations/custom-toolbox_sl.ts \
                translations/custom-toolbox_sm.ts \
                translations/custom-toolbox_sn.ts \
                translations/custom-toolbox_so.ts \
                translations/custom-toolbox_sq.ts \
                translations/custom-toolbox_sr.ts \
                translations/custom-toolbox_st.ts \
                translations/custom-toolbox_su.ts \
                translations/custom-toolbox_sv.ts \
                translations/custom-toolbox_sw.ts \
                translations/custom-toolbox_ta.ts \
                translations/custom-toolbox_te.ts \
                translations/custom-toolbox_tg.ts \
                translations/custom-toolbox_th.ts \
                translations/custom-toolbox_tk.ts \
                translations/custom-toolbox_tr.ts \
                translations/custom-toolbox_tt.ts \
                translations/custom-toolbox_ug.ts \
                translations/custom-toolbox_uk.ts \
                translations/custom-toolbox_ur.ts \
                translations/custom-toolbox_uz.ts \
                translations/custom-toolbox_vi.ts \
                translations/custom-toolbox_xh.ts \
                translations/custom-toolbox_yi.ts \
                translations/custom-toolbox_yo.ts \
                translations/custom-toolbox_yue_CN.ts \
                translations/custom-toolbox_zh_CN.ts \
                translations/custom-toolbox_zh_HK.ts \
                translations/custom-toolbox_zh_TW.ts

RESOURCES += \
    images.qrc
