/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2010 Michael Meeks
    Copyright (C) 2013  martin.dieringer@gmx.de

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

#include <string>
#include <gtkmm.h>

class Settings;

class Settings_ui {
 private:
  Glib::RefPtr<Gtk::Builder> m_builder;
  Settings *m_settings;
  bool inhibit_callback;
  
  void set_up_combobox(Gtk::ComboBoxText *combo, vector<string> values);
  void set_to_gui              (const string &group, const string &key);
  void get_colour_from_gui     (const string &glade_name);
  void mat_changed(Gtk::ComboBoxText *combo, string ext);

 public:
  Settings_ui(Glib::RefPtr<Gtk::Builder> &builder, Settings *settings);
  
  // sync changed settings with the GUI eg. used post load
  void set_to_gui(void);
  void get_from_gui(const string &glade_name);
  
  // connect settings to relevant GUI widgets
  void connect_to_ui();
  
  void printer_changed(void);
  void qualmat_changed(void);
  
  void ps_to_gui(const ps_value_t *set);
  void SetTargetTemps();
};
