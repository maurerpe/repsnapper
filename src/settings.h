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
#include <sigc++/sigc++.h>

#include "stdafx.h"
#include "ps_helper.h"

using namespace std;

class Inhibitor {
 private:
  bool m_init;
  bool *m_ptr;

 public:
  Inhibitor(bool *ptr) {m_ptr = ptr; m_init = *m_ptr; *m_ptr = true;};
  ~Inhibitor() {*m_ptr = m_init;};
};

class Settings {
  string filename; // where it's loaded from
  
  bool m_user_changed;
  bool inhibit_callback; // don't update settings from gui while setting to gui

  Psv settings;
  Psv printer;
  Psv ps;
  Psv dflt;
  Psv qualmat;
  
 public:
  static vector<string> get_serial_speeds();
  
  const Psv *GetPrinter() {return &printer;};
  const Psv *GetPs() {return &ps;};
  const Psv *GetDflt() {return &dflt;};
  const Psv *GetQualMat() {return &qualmat;};
  
  void SetPrinter(const ps_value_t *v);
  void SetQualMat(const ps_value_t *v);
  
  string GetPrinterName(void);
  
  string GetConfigPath(string filename);
  
  string get_string(const string &group, const string &name) const;
  double get_double(const string &group, const string &name) const;
  int    get_integer(const string &group, const string &name) const;
  bool   get_boolean(const string &group, const string &name) const;
  Vector4f get_colour(const string &group, const string &name) const;
  vector<double> get_double_list(const string &group, const string &name) const;
  vector<string> get_string_list(const string &group, const string &name) const;
  
  void set_string(const string &group, const string &name, const string &value);
  void set_double(const string &group, const string &name, double value);
  void set_integer(const string &group, const string &name, int value);
  void set_boolean(const string &group, const string &name, bool value);
  void set_colour(const string &group, const string &name, const Vector4f &value);
  void set_double_list(const string &group, const string &name, const vector<double> &values);
  void set_double_list(const string &group, const string &name, const Vector4f &values);
  void set_string_list(const string &group, const string &name, const vector<string> &values);

  bool has_group(const string &group) const;
  vector<string> get_groups(void) const;
  vector<string> get_keys(const string &group) const;

  vmml::vec3d getPrintVolume() const;
  vmml::vec3d getPrintMargin() const;

  static double RoundedLinewidthCorrection(double extr_width,
					   double layerheight);
  uint getNumExtruders() const;
  int getSerialSpeed() const;

  // Paths we loaded / saved things to last time
  string STLPath;
  string RFOPath;
  string GCodePath;
  string SettingsPath;

 private:
  void set_defaults();
  int  GetENo(string name, int model_specific = 0) const;
  
 public:
  Settings();
  ~Settings();
  
  static string GetExtruderText();
  
  bool has_user_changed() const { return m_user_changed; }

  bool set_user_button(const string &name, const string &gcode);
  bool del_user_button(const string &name);
  string get_user_gcode(const string &name);

  bool load_from_file(string file);

  void load_settings(const string &file);
  void save_settings(const string &file);

  void WriteTempPrinter(FILE *file, vector<string> ext);
  ps_value_t *load_printer(string filename);
  ps_value_t *load_printer(vector<string> ext);
  void load_printer_settings(void);
  
  ps_value_t *FullSettings(int model_specific = 0);
  
  sigc::signal< void > m_signal_visual_settings_changed;
  sigc::signal< void > m_signal_update_settings_gui;

  sigc::signal< void > m_extruders_changed;
  sigc::signal< void > m_printer_changed;
  sigc::signal< void > m_qualmat_changed;
};
