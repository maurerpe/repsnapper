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

#include "settings.h"
#include <gtkmm.h>

#include "set_dlg.h"

class PrefsDlg {
  Gtk::Dialog *m_preferences_dlg;
  Model *m_model;
  SetDlg m_set;

  void handle_response(Gtk::Dialog *dialog);
  void handle_response_int(int, Gtk::Dialog *dialog);

  std::vector<Settings *> m_settings;
  bool load_settings();

 public:
  PrefsDlg(Glib::RefPtr<Gtk::Builder> &builder, Model *model);
  ~PrefsDlg();
  void show(Gtk::Window &trans);
  void set_icon_from_file(const string path) {m_preferences_dlg->set_icon_from_file(path);}
};
