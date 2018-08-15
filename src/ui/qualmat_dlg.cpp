/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2010 Michael Meeks

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <cstdlib>
#include <gtkmm.h>

#include "qualmat_dlg.h"

QualMatDlg::QualMatDlg(Glib::RefPtr<Gtk::Builder> builder, Settings *settings, SetDlg *set) : m_cust(set, settings->GetPs()) {
  m_settings = settings;
  m_set = set;
  
  builder->get_widget("qualmat_dlg", m_dlg);
  m_cust.SetWindow(m_dlg);
  m_cust.SetValue("#global", PS_GetMember(PS_GetMember(m_settings->GetQualMat()->Get("quality", "Normal"), "settings", NULL), "#global", NULL));
  
  Gtk::Box *box;
  builder->get_widget("qual_cust", box);
  box->add(m_cust);
}

void QualMatDlg::show(Gtk::Window &trans) {
  m_dlg->set_transient_for(trans);
  m_dlg->show();
  m_dlg->raise();
}
