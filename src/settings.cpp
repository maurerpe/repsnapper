/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2010 Michael Meeks
    Copyright (C) 2013 martin.dieringer@gmx.de
    Copyright (C) 2018 Paul Maurer

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
#include <glibmm.h>
#include <glib/gi18n.h>

#include "ps_helper.h"
#include "settings.h"

#ifdef WIN32
#  define DEFAULT_COM_PORT "COM0"
#else
#  define DEFAULT_COM_PORT "/dev/ttyUSB0"
#endif

vector<string> serialspeeds = {"9600", "19200", "38400", "57600", "115200", "230400", "250000"};

vector<string> Settings::get_serial_speeds(void) {
  return serialspeeds;
}

string Settings::GetExtruderText() {
  return _("Extruder");
}

string Settings::GetConfigPath(string filename) {
  vector<string> path(3);
  path[0] = Glib::get_user_config_dir();
  path[1] = "repsnapper";
  path[2] = filename;
  
  return Glib::build_filename(path);
}

/////////////////////////////////////////////////////////////////

Settings::Settings() {
  set_defaults();
  m_user_changed = false;
  inhibit_callback = false;
}

Settings::~Settings() {
}

// always merge when loading settings
bool Settings::load_from_file(string file) {
  Psf in(file.c_str(), "r");
  
  ps_value_t *val = PS_ParseJsonFile(in());
  if (val == NULL)
    return false;
  
  if (settings.IsNull())
    settings.Take(val);
  else {
    PS_MergeSettings(settings(), val);
    PS_FreeValue(val);
  }
  
  return true;
}

string Settings::get_string(const string &group, const string &name) const {
  const char *str = PS_GetString(settings.Get(group.c_str(), name.c_str()));
  if (str == NULL)
    return "";
  
  return str;
}

double Settings::get_double(const string &group, const string &name) const {
  return PS_AsFloat(settings.Get(group.c_str(), name.c_str()));
}

int Settings::get_integer(const string &group, const string &name) const {
  return PS_AsInteger(settings.Get(group.c_str(), name.c_str()));
}

bool Settings::get_boolean(const string &group, const string &name) const {
  return PS_AsBoolean(settings.Get(group.c_str(), name.c_str()));
}

Vector4f Settings::get_colour(const string &group, const string &name) const {
  vector<double> s = get_double_list(group, name);
  return Vector4f(s[0],s[1],s[2],s[3]);
}

vector<double> Settings::get_double_list(const string &group, const string &name) const {
  vector<double> vect;
  
  Psvi vi(settings.Get(group.c_str(), name.c_str()));
  while (vi.Next())
    vect.push_back(PS_AsFloat(vi.Data()));
  
  return vect;
}

vector<string> Settings::get_string_list(const string &group, const string &name) const {
  vector<string> vect;
  const ps_value_t *v = settings.Get(group.c_str(), name.c_str());
  if (PS_GetType(v) == t_list)
    return vect;

  for (size_t count = 0; count < PS_ItemCount(v); count++) {
    const char *str = PS_GetString(PS_GetItem(v, count));
    
    if (str == NULL)
      continue;
    
    vect.push_back(str);
  }
  
  return vect;
}

void Settings::set_string(const string &group, const string &name, const string &value) {
  settings.Set(group.c_str(), name.c_str(), value);
}

void Settings::set_double(const string &group, const string &name, double value) {
  settings.Set(group.c_str(), name.c_str(), value);
}

void Settings::set_integer(const string &group, const string &name, int value) {
  settings.Set(group.c_str(), name.c_str(), value);
}

void Settings::set_boolean(const string &group, const string &name, bool value) {
  settings.Set(group.c_str(), name.c_str(), value);
}

void Settings::set_colour(const string &group, const string &name,
			  const Vector4f &value) {
  set_double_list(group, name, value);
}

void Settings::set_double_list(const string &group, const string &name, const vector<double> &values) {
  Psv val(PS_NewList());

  for (size_t count = 0; count < values.size(); count++)
    PS_AppendToList(val(), PS_NewFloat(values[count]));
  
  settings.Set(group.c_str(), name.c_str(), val());
}

void Settings::set_double_list(const string &group, const string &name, const Vector4f &values) {
  vector<double> vect;

  vect.push_back(values[0]);
  vect.push_back(values[1]);
  vect.push_back(values[2]);
  vect.push_back(values[3]);
  
  set_double_list(group, name, values);
}

void Settings::set_string_list(const string &group, const string &name, const vector<string> &values) {
  Psv val(PS_NewList());

  for (size_t count = 0; count < values.size(); count++)
    PS_AppendToList(val(), PS_NewString(values[count].c_str()));
  
  settings.Set(group.c_str(), name.c_str(), val());
}

bool Settings::has_group(const string &group) const {
  return settings.Get(group.c_str()) != NULL;
}

vector<string> Settings::get_groups(void) const {
  return Psv::GetNames(settings());
}

vector<string> Settings::get_keys(const string &group) const {
  return Psv::GetNames(settings.Get(group.c_str()));
}

void Settings::set_defaults() {
  filename = "";

  settings.Take(PS_NewObject());
  set_string("Global","Version",VERSION);
}

void Settings::load_settings(const string &file) {
  {
    Inhibitor inhibit(&inhibit_callback);
    
    filename = file;
  
    if (!load_from_file(filename)) {
      cout << _("Failed to load settings from file '") << filename << "\n";
      return;
    }
    
    cerr << _("Parsing config from '") << filename << "\n";
    
    vector<string> CustomButtonLabels;
    vector<string> CustomButtonGCodes;
    if (has_group("UserButtons")) {
      CustomButtonLabels = get_string_list("UserButtons","Labels");
      CustomButtonGCodes = get_string_list("UserButtons","GCodes");
    }
  }
  
  m_user_changed = false;
  m_signal_visual_settings_changed.emit();
  m_signal_update_settings_gui.emit();
}

void Settings::save_settings(const string &file) {
  Inhibitor inhibit(&inhibit_callback);
  
  set_string("Global","Version",VERSION);

  Psf out(file.c_str(), "w");
  Pso os(PS_NewFileOStream(out()));
  PS_WriteValuePretty(os(), settings());

  // all changes safely saved
  m_user_changed = false;
}

static bool StringsEqual(vector<string> s1, vector<string> s2) {
  if (s1.size() != s2.size())
    return false;
  
  for (size_t count = 0; count < s1.size(); count++) {
    if (s1[count] != s2[count])
      return false;
  }
  
  return true;
}

void Settings::SetPrinter(const ps_value_t *v) {
  vector<string> prev_ext;
  try {
    prev_ext = Psv::GetNames(PS_GetMember(printer(), "overrides", NULL));
  } catch (exception &e) {
  }
  
  bool ext_changed = false;
  
  printer.Take(PS_CopyValue(v));
  vector<string> ext = Psv::GetNames(printer.Get("overrides"));
  
  if (!StringsEqual(prev_ext, ext)) {
    ext_changed = true;
    ps.Take(load_printer(Psv::GetNames(printer.Get("overrides"))));
  }
  
  dflt.Take(PS_GetDefaults(ps()));
  PS_MergeSettings(dflt(), PS_GetMember(printer(), "overrides", NULL));

  if (ext_changed)
    m_extruders_changed.emit();

  m_printer_changed.emit();
}

void Settings::SetQualMat(const ps_value_t *v) {
  qualmat.Take(PS_CopyValue(v));

  m_qualmat_changed.emit();
}

string Settings::GetPrinterName(void) {
  const char *str = PS_GetString(printer.Get("name"));
  if (str == NULL)
    return "<Unknown>";

  return str;
}

void Settings::WriteTempPrinter(FILE *file, vector<string> ext) {
  fprintf(file, "{\"name\":\"%s\",\"version\":2,\"inherits\":\"fdmprinter\",\"metadata\":{\"machine_extruder_trains\":{", GetPrinterName().c_str());

  bool first = true;
  for (size_t count = 0; count < ext.size(); count++) {
    if (ext[count] == "#global")
      continue;
    
    fprintf(file, "%s\"%s\": \"fdmextruder\"", first ? "" : ",", ext[count].c_str());
    first = false;
  }
  fprintf(file, "}},\"overrides\":{}}\n");
}

ps_value_t *Settings::load_printer(string filename) {
  Psv search(PS_NewList());
  PS_AppendToList(search(), PS_NewString("/usr/share/cura/resources/definitions"));
  PS_AppendToList(search(), PS_NewString("/usr/share/cura/resources/extruders"));
  return PS_New(filename.c_str(), search());
}

ps_value_t *Settings::load_printer(vector<string> ext) {
  Pstemp temp(".def.json");
  WriteTempPrinter(temp(), ext);
  temp.Close();
  return load_printer(temp.Name().c_str());
}

void Settings::load_printer_settings(void) {
  Psf qualmat_file(GetConfigPath("qualmat.json").c_str(), "r");
  SetQualMat(PS_ParseJsonFile(qualmat_file()));
  qualmat_file.close();
  
  Psf printer_file(GetConfigPath("printer.json").c_str(), "r");
  SetPrinter(PS_ParseJsonFile(printer_file()));
  printer_file.close();  
}

// extrusion ratio for round-edge lines
double Settings::RoundedLinewidthCorrection(double extr_width,
					    double layerheight) {
  double factor = 1 + (M_PI/4.-1) * layerheight/extr_width;
  // assume 2 half circles at edges
  //    /-------------------\     //
  //   |                     |    //
  //    \-------------------/     //
  //cerr << "round factor " << factor << endl;
  return factor;
}

uint Settings::getNumExtruders() const {
  return PS_ItemCount(dflt()) - 1;
}

int Settings::getSerialSpeed() const {
  const char *str = PS_GetString(printer.Get("repsnapper", "#global", "serial_speed"));

  if (str == NULL)
    str = serialspeeds[0].c_str();
  
  return atoi(str);
}

Vector3d Settings::getPrintVolume() const {
  return Vector3d(PS_AsFloat(dflt.Get("#global", "machine_width")),
		  PS_AsFloat(dflt.Get("#global", "machine_depth")),
		  PS_AsFloat(dflt.Get("#global", "machine_height")));
}

vmml::vec3d Settings::getPrintMargin() const {
  return Vector3d(0, 0, 0);
}

bool Settings::set_user_button(const string &name, const string &gcode) {
  vector<string> buttonlabels = get_string_list("UserButtons","Labels");
  vector<string> buttongcodes = get_string_list("UserButtons","GCodes");
  for (uint i = 0; i < buttonlabels.size(); i++){
    if (buttonlabels[i] == name) {
      // change button
      buttongcodes[i] = gcode;
      set_string_list("UserButtons","GCodes",buttongcodes);
    } else {
	// add button
      buttonlabels.push_back(name);
      buttongcodes.push_back(gcode);
      set_string_list("UserButtons","Labels",buttonlabels);
      set_string_list("UserButtons","GCodes",buttongcodes);
    }
  }
  return true;
}

string Settings::get_user_gcode(const string &name) {
  vector<string> buttonlabels = get_string_list("UserButtons","Labels");
  vector<string> buttongcodes = get_string_list("UserButtons","GCodes");
  
  for (uint i = 0; i < buttonlabels.size(); i++){
    if (buttonlabels[i] == name)
      return buttongcodes[i];
  }
  
  return "";
}

bool Settings::del_user_button(const string &name) {
  vector<string> buttonlabels = get_string_list("UserButtons","Labels");
  vector<string> buttongcodes = get_string_list("UserButtons","GCodes");
  for (uint i = 0; i < buttonlabels.size(); i++){
    if (buttonlabels[i] == name) {
      buttonlabels.erase(buttonlabels.begin()+i);
      buttongcodes.erase(buttongcodes.begin()+i);
      return true;
    }
  }
  
  return false;
}

int Settings::GetENo(string name, int model_specific) const {
  const char *extc = PS_GetString(printer.Get("repsnapper", "#global", name.c_str()));
  if (extc == NULL)
    return -1;
  string ext(extc);
  
  if (string(ext) == "Model Specific")
    return model_specific;

  size_t len = Settings::GetExtruderText().size();

  if (ext.size() < len)
    return -1;
  
  return stoi(ext.substr(len));
}

static string EStr(int e_no) {
  return to_string(e_no);
}

ps_value_t *Settings::FullSettings(int model_specific) {
  string qualname = get_string("Slicing", "Quality");
  const ps_value_t *qual = qualmat.Get("quality", qualname.c_str());
  
  double wh = PS_AsFloat(PS_GetMember(qual, "width/height", NULL));
  double speed = PS_AsFloat(PS_GetMember(qual, "speed", NULL));
  double ratio = PS_AsFloat(PS_GetMember(qual, "wall-speed-ratio", NULL));

  double h = get_double("Slicing", "LayerHeight");
  bool support = get_boolean("Slicing", "Support");
  double infill = get_double("Slicing", "InfillPercent");
  int shells = get_integer("Slicing", "ShellCount");
  int skins = get_integer("Slicing", "Skins");
  string adhesion = get_string("Slicing", "Adhesion");
  bool spiralize = get_boolean("Slicing","Spiralize");
  
  int num_e = getNumExtruders();
  vector<double> espeed(num_e);
  for (int e_no = 0; e_no < num_e; e_no++) {
    const char *matname = PS_GetString(printer.Get("repsnapper", EStr(e_no).c_str(), "material"));
    const ps_value_t *mat = qualmat.Get("materials", matname);
    if (mat == NULL)
      cout << "Unknown material: " << matname << endl;

    double dia = PS_AsFloat(dflt.Get(EStr(e_no).c_str(), "material_diameter"));
    double efeed = PS_AsFloat(PS_GetItem(PS_GetItem(PS_GetMember(mat, "nozzle-feedrate", NULL), 0), 1));
    const ps_value_t *matwh = PS_GetMember(mat, "width/height", NULL);
    
    if (matwh && PS_AsFloat(matwh) > 0)
      wh = PS_AsFloat(matwh);
    
    double matspeed = efeed * dia * dia / (h * h * wh);
    if (speed < matspeed)
      matspeed = speed;
    espeed[e_no] = matspeed;
  }
  
  int eshell   = GetENo("shell_extruder",   model_specific);
  int eskin    = GetENo("skin_extruder",    model_specific);
  int einfill  = GetENo("infill_extruder",  model_specific);
  int esupport = GetENo("support_extruder", model_specific);
  
  Psv set(PS_BlankSettings(ps()));
  set.Set("#global", "extruders_enabled_count", (int) getNumExtruders());
  
  set.Set("#global", "speed_print",     speed);
  set.Set("#global", "speed_wall",      espeed[eshell  ] * ratio);
  set.Set("#global", "speed_topbottom", espeed[eskin   ] * ratio);
  set.Set("#global", "speed_infill",    espeed[einfill ]);
  set.Set("#global", "speed_support",   espeed[esupport]);
  
  PS_MergeSettings(set(), PS_GetMember(qual, "settings", NULL));
  for (int e_no = num_e - 1; e_no >= 0; e_no--) {
    const char *matname = PS_GetString(printer.Get("repsnapper", EStr(e_no).c_str(), "material"));
    const ps_value_t *mat = qualmat.Get("materials", matname);

    string ext = to_string(e_no);
    set.MergeActive(ext.c_str(), PS_GetMember(mat, "settings", NULL));
    
    set.Set(ext.c_str(), "extruder_nr", e_no);
  }
  
  set.Set("#global", "wall_extruder_nr",       EStr(eshell));
  set.Set("#global", "top_bottom_extruder_nr", EStr(eskin));
  set.Set("#global", "infill_extruder_nr",     EStr(einfill));
  set.Set("#global", "support_extruder_nr",    EStr(esupport));
  
  set.Set("#global", "layer_height", h);
  set.Set("#global", "line_width", h * wh);
  set.Set("#global", "wall_line_count", spiralize ? 1 : shells);
  set.Set("#global", "top_layers", skins);
  set.Set("#global", "bottom_layers", skins);
  set.Set("#global", "infill_sparse_density", infill);
  set.Set("#global", "adhesion_type", adhesion);
  set.Set("#global", "support_enable", support);
  set.Set("#global", "magic_spiralize", spiralize);
  
  set.Set("#global", "machine_extruder_count", (int) getNumExtruders());
  
  Psv eval(PS_EvalAllDflt(ps(), set(), dflt()));
  Psv all(PS_CopyValue(dflt()));
  PS_MergeSettings(all(), eval());
  
  ps_value_t *allv = all();
  all.Disown();
  
  return allv;
}
