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
#include <giomm/file.h>
#include <glibmm/keyfile.h>

#include "stdafx.h"
#include "ps_helper.h"

using namespace std;

// Allow passing as a pointer to something to
// avoid including glibmm in every header.
typedef Glib::RefPtr<Gtk::Builder> Builder;

class Inhibitor {
 private:
  bool m_init;
  bool *m_ptr;

 public:
  Inhibitor(bool *ptr) {m_ptr = ptr; m_init = *m_ptr; *m_ptr = true;};
  ~Inhibitor() {*m_ptr = m_init;};
};

class Settings : public Glib::KeyFile {
  Glib::ustring filename; // where it's loaded from
  
  bool m_user_changed;
  bool inhibit_callback; // don't update settings from gui while setting to gui

  Psv printer;
  Psv ps;
  Psv dflt;
  Psv qualmat;
  
 public:
  const Psv *GetPrinter() {return &printer;};
  const Psv *GetPs() {return &ps;};
  const Psv *GetDflt() {return &dflt;};
  const Psv *GetQualMat() {return &qualmat;};

  void SetPrinter(const ps_value_t *v);
  void SetQualMat(const ps_value_t *v);
  
  string GetConfigPath(string filename);
  
  void copyGroup(const string &from, const string &to);

  Vector4f get_colour(const string &group, const string &name) const;
  void set_colour(const string &group, const string &name, const Vector4f &value);

  vmml::vec3d getPrintVolume() const;
  vmml::vec3d getPrintMargin() const;

  static double RoundedLinewidthCorrection(double extr_width,
					   double layerheight);
  uint getNumExtruders() const;

  // Paths we loaded / saved things to last time
  string STLPath;
  string RFOPath;
  string GCodePath;
  string SettingsPath;

 private:
  void set_to_gui              (Builder &builder,
				const string &group, const string &key);
  void get_colour_from_gui     (Builder &builder, const string &glade_name);
  void set_defaults();
  int  GetENo(string name, int model_specific = 1) const;
  
 public:
  Settings();
  ~Settings();
  
  string GetExtruderText() const;
  
  bool has_user_changed() const { return m_user_changed; }
  void assign_from(Settings *pSettings);

  bool set_user_button(const string &name, const string &gcode);
  bool del_user_button(const string &name);
  string get_user_gcode(const string &name);

  // sync changed settings with the GUI eg. used post load
  void set_to_gui(Builder &builder, const string filter="");
  void get_from_gui(Builder &builder, const string &glade_name);

  // connect settings to relevant GUI widgets
  void connect_to_ui(Builder &builder);

  void merge(const Glib::KeyFile &keyfile);
  bool load_from_file(string file);
  bool load_from_data(string data);

  void load_settings(Glib::RefPtr<Gio::File> file);
  void save_settings(Glib::RefPtr<Gio::File> file);

  void WriteTempPrinter(FILE *file, vector<string> ext);
  ps_value_t *load_printer(string filename);
  ps_value_t *load_printer(vector<string> ext);
  void load_printer_settings(void);
  
  void ps_to_gui(Builder &builder, const ps_value_t *set);
  ps_value_t *FullSettings(int model_specific = 0);
  void SetTargetTemps(Builder &builder);
  
  sigc::signal< void > m_signal_visual_settings_changed;
  sigc::signal< void > m_signal_update_settings_gui;

  sigc::signal< void > m_extruders_changed;
  sigc::signal< void > m_printer_changed;
};
