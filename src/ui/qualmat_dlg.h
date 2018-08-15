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

#pragma once

#include <gtkmm.h>

#include "settings.h"
#include "set_dlg.h"
#include "cust_prop.h"
#include "ps_helper.h"

class QualMatDlg {
 private:
  Settings *m_settings;
  SetDlg *m_set;
  CustProp m_cust;

  Gtk::Dialog *m_dlg;
  
 public:
  QualMatDlg(Glib::RefPtr<Gtk::Builder> builder, Settings *settings, SetDlg *set);
  
  void show(Gtk::Window &trans);
};
