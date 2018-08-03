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

class Settings : public Glib::KeyFile {
  Glib::ustring filename; // where it's loaded from
  
  bool m_user_changed;
  bool inhibit_callback; // don't update settings from gui while setting to gui
  
  Psv ps;
  Psv dflt;
  Psv config;
  
 public:
  const Psv *GetPs() {return &ps;};
  const Psv *GetDflt() {return &dflt;};
  const Psv *GetConfig() {return &config;};
  
  void copyGroup(const string &from, const string &to);

  Vector4f get_colour(const string &group, const string &name) const;
  void set_colour(const string &group, const string &name, const Vector4f &value);

  string numberedExtruder(const string &group, int num=-1) const;

  vmml::vec3d getPrintVolume() const;
  vmml::vec3d getPrintMargin() const;

  static double RoundedLinewidthCorrection(double extr_width,
					   double layerheight);
  double GetExtrudedMaterialWidth(const double layerheight) const;
  double GetExtrusionPerMM(double layerheight) const;
  vector<char> get_extruder_letters() const;
  Vector3d get_extruder_offset(uint num) const;
  uint GetSupportExtruder() const;
  void CopyExtruder(uint num);
  void RemoveExtruder(uint num);
  void SelectExtruder(uint num, Builder *builder=NULL);
  uint selectedExtruder;
  uint getNumExtruders() const;

  // Paths we loaded / saved things to last time
  string STLPath;
  string RFOPath;
  string GCodePath;
  string SettingsPath;

 private:
  void set_to_gui              (Builder &builder, int i);
  void set_to_gui              (Builder &builder,
				const string &group, const string &key);
  void get_from_gui_old        (Builder &builder, int i);
  void get_from_gui            (Builder &builder, const string &glade_name);
  bool get_group_and_key       (int i, Glib::ustring &group, Glib::ustring &key);
  void get_colour_from_gui     (Builder &builder, const string &glade_name);
  void set_defaults();

 public:
  Settings();
  ~Settings();

  bool has_user_changed() const { return m_user_changed; }
  void assign_from(Settings *pSettings);

  bool set_user_button(const string &name, const string &gcode);
  bool del_user_button(const string &name);
  string get_user_gcode(const string &name);

  Matrix4d getBasicTransformation(Matrix4d T) const;

  // return real mm depending on hardware extrusion width setting
  double GetInfillDistance(double layerthickness, float percent) const;

  // sync changed settings with the GUI eg. used post load
  void set_to_gui(Builder &builder, const string filter="");

  // connect settings to relevant GUI widgets
  void connect_to_ui(Builder &builder);

  void merge(const Glib::KeyFile &keyfile);
  bool load_from_file(string file);
  bool load_from_data(string data);

  void load_settings(Glib::RefPtr<Gio::File> file);
  void save_settings(Glib::RefPtr<Gio::File> file);

  void load_printer_settings(void);
  
  string get_image_path();

  void ps_to_gui(Builder &builder, ps_value_t *set);
  ps_value_t *FullSettings();
  void SetTargetTemps(Builder &builder);
  
  sigc::signal< void > m_signal_visual_settings_changed;
  sigc::signal< void > m_signal_update_settings_gui;
  sigc::signal< void > m_signal_core_settings_changed;
};
