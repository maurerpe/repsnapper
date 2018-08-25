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

#include <cstdlib>
#include <gtkmm.h>

#include "ps_helper.h"
#include "settings.h"

/*
 * How settings are intended to work:
 *
 * Settings is a subclass of Glib::KeyFile.
 *
 * Glade Builder Widgets are named as <Group>.<Key>, so automatically
 * converted to KeyFile settings.  This works for most settings, but
 * there are exceptions...
 *
 * All default setting values have to be at least in the default .conf file
 *
 * A redraw is done on every change made by the GUI.
 */

#ifdef WIN32
#  define DEFAULT_COM_PORT "COM0"
#else
#  define DEFAULT_COM_PORT "/dev/ttyUSB0"
#endif

const string serialspeeds[] = { "9600", "19200", "38400", "57600", "115200", "230400", "250000" };

string Settings::GetExtruderText() const {
  return _("Extruder");
}

string Settings::GetConfigPath(string filename) {
  vector<string> path(3);
  path[0] = Glib::get_user_config_dir();
  path[1] = "repsnapper";
  path[2] = filename;
  
  return Glib::build_filename(path);
}

// convert GUI name to group/key
bool splitpoint(const string &glade_name, string &group, string &key) {
  int pos = glade_name.find(".");
  if (pos==(int)string::npos) return false;
  group = glade_name.substr(0,pos);
  key = glade_name.substr(pos+1);
  return true;
}

void set_up_combobox(Gtk::ComboBoxText *combo, vector<string> values) {
  combo->remove_all();
  
  for (uint i=0; i<values.size(); i++)
    combo->append(Glib::ustring(values[i].c_str()));
  
  if (!combo->get_has_entry())
    combo->set_active(0);
}

string combobox_get_active_value(Gtk::ComboBoxText *combo){
  if (combo->get_has_entry())
    return string(combo->get_entry_text());
  
  return string(combo->get_active_text());
}

bool combobox_set_to(Gtk::ComboBoxText *combo, string value) {
  if (combo->get_has_entry()) {
    Gtk::Entry *entry = combo->get_entry();
    if (entry == NULL)
      return false;
    
    entry->set_text(value);
  } else {
    combo->set_active_text(value.c_str());
  }
  
  return true;
}

/////////////////////////////////////////////////////////////////

Settings::Settings() {
  set_defaults();
  m_user_changed = false;
  inhibit_callback = false;
}

Settings::~Settings() {
}

// merge into current settings
void Settings::merge(const Glib::KeyFile &keyfile) {
  vector< Glib::ustring > groups = keyfile.get_groups();
  for (uint g = 0; g < groups.size(); g++) {
    vector< Glib::ustring > keys = keyfile.get_keys(groups[g]);
    for (uint k = 0; k < keys.size(); k++) {
      set_value(groups[g], keys[k], keyfile.get_value(groups[g], keys[k]));
    }
  }
}

// always merge when loading settings
bool Settings::load_from_file (string file) {
  Glib::KeyFile k;
  if (!k.load_from_file(file)) return false;
  merge(k);
  return true;
}

bool Settings::load_from_data (string data) {
  Glib::KeyFile k;
  if (!k.load_from_file(data)) return false;
  merge(k);
  return true;
}

Vector4f Settings::get_colour(const string &group, const string &name) const {
  vector<double> s = get_double_list(group, name);
  return Vector4f(s[0],s[1],s[2],s[3]);
}

void Settings::set_colour(const string &group, const string &name,
			  const Vector4f &value) {
  Glib::KeyFile::set_double_list(group, name, value);
}

void Settings::assign_from(Settings *pSettings) {
  this->load_from_data(pSettings->to_data());
  m_user_changed = false;
  m_signal_visual_settings_changed.emit();
  m_signal_update_settings_gui.emit();
}

void Settings::set_defaults() {
  filename = "";

  set_string("Global","SettingsName","Default Settings");
  set_string("Global","SettingsImage","");

  set_string("Global","Version",VERSION);
}

void Settings::load_settings(Glib::RefPtr<Gio::File> file) {
  {
    Inhibitor inhibit(&inhibit_callback);
    
    filename = file->get_path();
  
    try {
      if (!load_from_file(filename)) {
	cout << _("Failed to load settings from file '") << filename << "\n";
	return;
      }
    } catch (const Glib::KeyFileError &err) {
      cout << _("Exception ") << err.what() << _(" loading settings from file '") << filename << "\n";
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

void Settings::save_settings(Glib::RefPtr<Gio::File> file) {
  Inhibitor inhibit(&inhibit_callback);
  
  set_string("Global","Version",VERSION);

  Glib::ustring contents = to_data();
  // cerr << contents << endl;
  Glib::file_set_contents(file->get_path(), contents);

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
  vector<string> ext = Psv::GetNames(PS_GetMember(printer(), "overrides", NULL));
  
  if (!StringsEqual(prev_ext, ext)) {
    ext_changed = true;
    ps.Take(load_printer(Psv::GetNames(PS_GetMember(printer(), "overrides", NULL))));
  }
  
  dflt.Take(PS_GetDefaults(ps()));
  PS_MergeSettings(dflt(), PS_GetMember(printer(), "overrides", NULL));

  if (ext_changed)
    m_extruders_changed.emit();
  
  m_printer_changed.emit();
}

void Settings::WriteTempPrinter(FILE *file, vector<string> ext) {
  fprintf(file, "{\"name\":\"%s\",\"version\":2,\"inherits\":\"fdmprinter\",\"metadata\":{\"machine_extruder_trains\":{", PS_GetString(PS_GetMember(printer(), "name", NULL)));

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
  Psf printer_file(GetConfigPath("printer.json").c_str(), "r");
  SetPrinter(PS_ParseJsonFile(printer_file()));
  printer_file.close();
  
  Psf qualmat_file(GetConfigPath("qualmat.json").c_str(), "r");
  qualmat.Take(PS_ParseJsonFile(qualmat_file()));
  qualmat_file.close();
}

void Settings::set_to_gui(Builder &builder,
			  const string &group, const string &key) {
  Inhibitor inhibit(&inhibit_callback);

  Glib::ustring glade_name = group + "." + key;
  // inhibit warning for settings not defined in glade UI:
  if (!builder->get_object (glade_name)) {
    //cerr << glade_name << _(" not defined in GUI!")<< endl;
    return;
  }

  Gtk::Widget *w = NULL;
  builder->get_widget(glade_name, w);
  if (!w) {
    cerr << _("Missing user interface item ") << glade_name << "\n";
    return;
  }
  
  Gtk::CheckButton *check = dynamic_cast<Gtk::CheckButton *>(w);
  if (check) {
    check->set_active(get_boolean(group,key));
    return;
  }
  
  Gtk::Switch *swit = dynamic_cast<Gtk::Switch *>(w);
  if (swit) {
    swit->set_active(get_boolean(group,key));
    return;
  }
  
  Gtk::SpinButton *spin = dynamic_cast<Gtk::SpinButton *>(w);
  if (spin) {
    spin->set_value(get_double(group,key));
    return;
  }
  
  Gtk::Range *range = dynamic_cast<Gtk::Range *>(w);
  if (range) {
    range->set_value(get_double(group,key));
    return;
  }
  
  Gtk::ComboBoxText *combot = dynamic_cast<Gtk::ComboBoxText *>(w);
  if (combot) {
    combobox_set_to(combot, get_string(group,key));
    return;
  }
  
  Gtk::Entry *entry = dynamic_cast<Gtk::Entry *>(w);
  if (entry) {
    entry->set_text(get_string(group,key));
    return;
  }
  
  Gtk::Expander *exp = dynamic_cast<Gtk::Expander *>(w);
  if (exp) {
    exp->set_expanded(get_boolean(group,key));
    return;
  }
  
  Gtk::Paned *paned = dynamic_cast<Gtk::Paned *>(w);
  if (paned) {
    paned->set_position(get_integer(group,key));
    return;
  }
  
  Gtk::ColorButton *col = dynamic_cast<Gtk::ColorButton *>(w);
  if(col) {
    vector<double> c = get_double_list(group,key);
    Gdk::Color co; co.set_rgb_p(c[0],c[1],c[2]);
    col->set_use_alpha(true);
    col->set_color(co);
    col->set_alpha(c[3] * 65535.0);
    return;
  }
  
  Gtk::TextView *tv = dynamic_cast<Gtk::TextView *>(w);
  if (tv) {
    tv->get_buffer()->set_text(get_string(group,key));
    return;
  }

  cerr << "set_to_gui of "<< glade_name << " not done!" << endl;
}


void Settings::get_from_gui(Builder &builder, const string &glade_name) {
  if (inhibit_callback)
    return;
  
  if (!builder->get_object(glade_name)) {
    cerr << "no such object " << glade_name << endl;
    return;
  }
  
  Gtk::Widget *w = NULL;
  builder->get_widget(glade_name, w);
  string group, key;
  if (w == NULL || !splitpoint(glade_name, group, key))
    return;
  
  do { // for using break ...
    //cerr << "get " << group  << "." << key << " from gui"<< endl;
    m_user_changed = true;
    Gtk::CheckButton *check = dynamic_cast<Gtk::CheckButton *>(w);
    if (check) {
      set_boolean(group, key, check->get_active());
      break;
    }
    
    Gtk::Switch *swit = dynamic_cast<Gtk::Switch *>(w);
    if (swit) {
      set_boolean(group, key, swit->get_active());
      break;
    }
    
    Gtk::SpinButton *spin = dynamic_cast<Gtk::SpinButton *>(w);
    if (spin) {
      set_double(group, key, spin->get_value());
      break;
    }
    
    Gtk::Range *range = dynamic_cast<Gtk::Range *>(w);
    if (range) {
      set_double(group, key, range->get_value());
      break;
    }
    
    Gtk::ComboBoxText *combot = dynamic_cast<Gtk::ComboBoxText *>(w);
    if (combot) {
      set_string(group,key,combobox_get_active_value(combot));
      break;
    }
    
    Gtk::Entry *e = dynamic_cast<Gtk::Entry *>(w);
    if (e) {
      set_string(group,key,e->get_text());
      break;
    }
    
    Gtk::Expander *exp = dynamic_cast<Gtk::Expander *>(w);
    if (exp) {
      set_boolean(group,key,exp->get_expanded());
      break;
    }
    
    Gtk::Paned *paned = dynamic_cast<Gtk::Paned *>(w);
    if (paned) {
      set_integer(group,key,paned->get_position());
      break;
    }
	
    Gtk::ColorButton *cb = dynamic_cast<Gtk::ColorButton *>(w);
    if (cb) {
      get_colour_from_gui(builder, glade_name);
      break;
    }
    
    Gtk::TextView *tv = dynamic_cast<Gtk::TextView *>(w);
    if (tv) {
      set_string(group,key,tv->get_buffer()->get_text());
      break;
    }
    
    cerr << _("Did not get setting from  ") << glade_name << endl;
    m_user_changed = false;
    break;
  } while (0);
  
  if (key == "E0Material") {
    SetTargetTemps(builder);
    ps_to_gui(builder, PS_GetMember(qualmat.Get("materials", get_string("Slicing", "E0Material").c_str()), "settings", NULL));
  }
  
  if (key == "E1Material") {
    SetTargetTemps(builder);
    ps_to_gui(builder, PS_GetMember(qualmat.Get("materials", get_string("Slicing", "E1Material").c_str()), "settings", NULL));
  }
  
  if (key == "Quality") {
    ps_to_gui(builder, PS_GetMember(qualmat.Get("quality", get_string("Slicing", "Quality").c_str()), "settings", NULL));
  }
  
  m_signal_visual_settings_changed.emit();
}

void Settings::get_colour_from_gui(Builder &builder, const string &glade_name) {
  string group,key;
  if (!splitpoint(glade_name, group,key)) return;
  Gdk::Color c;
  Gtk::ColorButton *w = NULL;
  builder->get_widget (glade_name, w);
  if (!w) return;

  c = w->get_color();

  // FIXME: detect 'changed' etc.
  vector<double> d(4);
  d[0] = c.get_red_p();
  d[1] = c.get_green_p();
  d[2] = c.get_blue_p();
  d[3] = (float) (w->get_alpha()) / 65535.0;

  set_double_list(group, key, d);

  m_signal_visual_settings_changed.emit();
}

// whole group or all groups
void Settings::set_to_gui(Builder &builder, const string filter) {
  Inhibitor inhibit(&inhibit_callback);

  vector< Glib::ustring > groups = get_groups();
  for (uint g = 0; g < groups.size(); g++) {
    vector< Glib::ustring > keys = get_keys(groups[g]);
    for (uint k = 0; k < keys.size(); k++) {
      set_to_gui(builder, groups[g], keys[k]);
    }
  }

  //set_filltypes_to_gui (builder);

  if (filter == "" || filter == "Misc") {
    Gtk::Window *pWindow = NULL;
    builder->get_widget("main_window", pWindow);
    try {
      int w = get_integer("Misc","WindowWidth");
      int h = get_integer("Misc","WindowHeight");
      if (pWindow && w > 0 && h > 0) pWindow->resize(w,h);
      int x = get_integer("Misc","WindowPosX");
      int y = get_integer("Misc","WindowPosY");
      if (pWindow && x > 0 && y > 0) pWindow->move(x,y);
    } catch (const Glib::KeyFileError &err) {
      cout << _("Exception ") << err.what() << _(" loading setting\n");
    }
  }

  // Set serial speed. Find the row that holds this value
  if (filter == "" || filter == "Hardware") {
    Gtk::ComboBoxText *portspeed = NULL;
    builder->get_widget("Hardware.SerialSpeed", portspeed);
    if (portspeed) {
      ostringstream ostr;
      ostr << get_integer("Hardware","SerialSpeed");
      //cerr << "portspeed " << get_integer("Hardware","SerialSpeed") << endl;
      combobox_set_to(portspeed, ostr.str());
    }
  }
}

void Settings::connect_to_ui(Builder &builder) {
  if (has_group("Ranges")) {
    vector<string> ranges = get_keys("Ranges");
    for (uint i = 0; i < ranges.size(); i++) {
      // get min, max, increment, page-incr.
      vector<double> vals = get_double_list("Ranges", ranges[i]);
      Gtk::Widget *w = NULL;
      try {
	builder->get_widget(ranges[i], w);
	if (!w) {
	  cerr << "Missing user interface item " << ranges[i] << "\n";
	  continue;
	}
	
	Gtk::SpinButton *spin = dynamic_cast<Gtk::SpinButton *>(w);
	if (spin) {
	  spin->set_range(vals[0],vals[1]);
	  spin->set_increments(vals[2],vals[3]);
	  continue;
	}
	
	Gtk::Range *range = dynamic_cast<Gtk::Range *>(w); // sliders etc.
	if (range) {
	  range->set_range(vals[0],vals[1]);
	  range->set_increments(vals[2],vals[3]);
	  continue;
	}
      } catch (Glib::Exception &ex) {
      }
    }
  }

  // add signal callbacks to GUI elements
  vector< Glib::ustring > groups = get_groups();
  for (uint g = 0; g < groups.size(); g++) {
    if (groups[g] == "Ranges") continue; // done that above
    vector< Glib::ustring > keys = get_keys(groups[g]);
    for (uint k = 0; k < keys.size(); k++) {
      string glade_name = groups[g] + "." + keys[k];
      if (!builder->get_object(glade_name))
	continue;
      Gtk::Widget *w = NULL;
      try {
	builder->get_widget(glade_name, w);
	if (!w) {
	  cerr << "Missing user interface item " << glade_name << "\n";
	  continue;
	}
	
	Gtk::CheckButton *check = dynamic_cast<Gtk::CheckButton *>(w);
	if (check) {
	  check->signal_toggled().connect
	    (sigc::bind(sigc::bind<string>(sigc::mem_fun(*this, &Settings::get_from_gui), glade_name), builder));
	  continue;
	}
	
	Gtk::Switch *swit = dynamic_cast<Gtk::Switch *>(w);
	if (swit) {
	  swit->property_active().signal_changed().connect
	    (sigc::bind(sigc::bind<string>(sigc::mem_fun(*this, &Settings::get_from_gui), glade_name), builder));
	  continue;
	}
	
	Gtk::SpinButton *spin = dynamic_cast<Gtk::SpinButton *>(w);
	if (spin) {
	  spin->signal_value_changed().connect
	    (sigc::bind(sigc::bind<string>(sigc::mem_fun(*this, &Settings::get_from_gui), glade_name), builder));
	  continue;
	}
	
	Gtk::Range *range = dynamic_cast<Gtk::Range *>(w);
	if (range) {
	  range->signal_value_changed().connect
	    (sigc::bind(sigc::bind<string>(sigc::mem_fun(*this, &Settings::get_from_gui), glade_name), builder));
	  continue;
	}
	
	Gtk::ComboBoxText *combot = dynamic_cast<Gtk::ComboBoxText *>(w);
	if (combot) {
	  if (glade_name == "Hardware.SerialSpeed") {
	    vector<string> speeds(serialspeeds,
				  serialspeeds+sizeof(serialspeeds)/sizeof(string));
	    set_up_combobox(combot, speeds);
	  } else if (glade_name == "Slicing.Quality") {
	    set_up_combobox(combot, Psv::GetNames(PS_GetMember(qualmat(), "quality", NULL)));
	  } else if (glade_name == "Slicing.E0Material" || glade_name == "Slicing.E1Material") {
	    set_up_combobox(combot, Psv::GetNames(PS_GetMember(qualmat(), "materials", NULL)));
	  } else if (glade_name == "Slicing.Adhesion") {
	    set_up_combobox(combot, Psv::GetNames(PS_GetMember(PS_GetMember(PS_GetMember(PS_GetMember(ps(), "#global", NULL), "#set", NULL), "adhesion_type", NULL), "options", NULL)));
	  }
	  combot->signal_changed().connect
	    (sigc::bind(sigc::bind<string>(sigc::mem_fun(*this, &Settings::get_from_gui), glade_name), builder));
	  continue;
	}
	
	Gtk::Entry *e = dynamic_cast<Gtk::Entry *>(w);
	if (e) {
	  e->signal_changed().connect
	    (sigc::bind(sigc::bind<string>(sigc::mem_fun(*this, &Settings::get_from_gui), glade_name), builder));
	  continue;
	}

	Gtk::Paned *paned = dynamic_cast<Gtk::Paned *>(w);
	if (paned) {
	  paned->property_position().signal_changed().connect
	    (sigc::bind(sigc::bind<string>(sigc::mem_fun(*this, &Settings::get_from_gui), glade_name), builder));
	  continue;
	}
	
	Gtk::Expander *exp = dynamic_cast<Gtk::Expander *>(w);
	if (exp) {
	  exp->property_expanded().signal_changed().connect
	    (sigc::bind(sigc::bind<string>(sigc::mem_fun(*this, &Settings::get_from_gui), glade_name), builder));
	  continue;
	}
	
	Gtk::ColorButton *cb = dynamic_cast<Gtk::ColorButton *>(w);
	if (cb) {
	  cb->signal_color_set().connect
	    (sigc::bind(sigc::bind<string>(sigc::mem_fun(*this, &Settings::get_from_gui), glade_name), builder));
	  continue;
	}
	
	Gtk::TextView *tv = dynamic_cast<Gtk::TextView *>(w);
	if (tv) {
	  tv->get_buffer()->signal_changed().connect
	    (sigc::bind(sigc::bind<string>(sigc::mem_fun(*this, &Settings::get_from_gui), glade_name), builder));
	  continue;
	}
      } catch (Glib::Exception &ex) {
      }
    }
  }
  
  SetTargetTemps(builder);
  
  /* Update UI with defaults */
  m_signal_update_settings_gui.emit();
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

Vector3d Settings::getPrintVolume() const {
  return Vector3d(PS_AsFloat(dflt.Get("#global", "machine_width")),
		  PS_AsFloat(dflt.Get("#global", "machine_depth")),
		  PS_AsFloat(dflt.Get("#global", "machine_height")));
}

vmml::vec3d Settings::getPrintMargin() const {
  return Vector3d(0, 0, 0);
}

bool Settings::set_user_button(const string &name, const string &gcode) {
  try {
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
  } catch (const Glib::KeyFileError &err) {
  }
  return true;
}

string Settings::get_user_gcode(const string &name) {
  try {
    vector<string> buttonlabels = get_string_list("UserButtons","Labels");
    vector<string> buttongcodes = get_string_list("UserButtons","GCodes");
    for (uint i = 0; i < buttonlabels.size(); i++){
      if (buttonlabels[i] == name)
	return buttongcodes[i];
    }
  } catch (const Glib::KeyFileError &err) {
  }
  return "";
}

bool Settings::del_user_button(const string &name) {
  try {
    vector<string> buttonlabels = get_string_list("UserButtons","Labels");
    vector<string> buttongcodes = get_string_list("UserButtons","GCodes");
    for (uint i = 0; i < buttonlabels.size(); i++){
      if (buttonlabels[i] == name) {
	buttonlabels.erase(buttonlabels.begin()+i);
	buttongcodes.erase(buttongcodes.begin()+i);
	return true;
      }
    }
  } catch (const Glib::KeyFileError &err) {
  }
  return false;
}

void Settings::ps_to_gui(Builder &builder, const ps_value_t *set) {
  ps_value_t *ext, *val;
  
  if (set == NULL)
    return;
  
  if ((ext = PS_GetMember(set, "#global", NULL))) {
    if ((val = PS_GetMember(ext, "infill_sparse_density", NULL))) {
      set_double("Slicing", "InfillPercent", PS_AsFloat(val));
      set_to_gui(builder, "Slicing", "InfillPercent");
    }
    
    if ((val = PS_GetMember(ext, "adhesion_type", NULL))) {
      set_string("Slicing", "Adhesion", PS_GetString(val));
      set_to_gui(builder, "Slicing", "Adhesion");
    }
    
    if ((val = PS_GetMember(ext, "magic_spiralize", NULL))) {
      set_boolean("Slicing", "Spiralize", PS_AsBoolean(val));
      set_to_gui(builder, "Slicing", "Spiralize");
    }
    
    if ((val = PS_GetMember(ext, "wall_line_count", NULL))) {
      set_integer("Slicing", "ShellCount", PS_AsInteger(val));
      set_to_gui(builder, "Slicing", "ShellCount");
    }
    
    if ((val = PS_GetMember(ext, "top_layers", NULL))) {
      set_integer("Slicing", "Skins", PS_AsInteger(val));
      set_to_gui(builder, "Slicing", "Skins");
    }
    
    if ((val = PS_GetMember(ext, "bottom_layers", NULL))) {
      set_integer("Slicing", "Skins", PS_AsInteger(val));
      set_to_gui(builder, "Slicing", "Skins");
    }
  }
}

int Settings::GetENo(string name, int model_specific) const {
  string ext = get_string("Extruder", name);

  if (string(ext) == "Model Specific")
    return model_specific;

  size_t len = GetExtruderText().size();

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
  
  double dia = PS_AsFloat(dflt.Get("#global", "material_diameter"));
  
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
    string matname = get_string("Slicing", "E" + to_string(e_no) + "Material");
    const ps_value_t *mat = qualmat.Get("materials", matname.c_str());
    if (mat == NULL)
      cout << endl << "Unknown material: " << matname << endl;
    
    double efeed = PS_AsFloat(PS_GetItem(PS_GetItem(PS_GetMember(mat, "nozzle-feedrate", NULL), 0), 1));
    const ps_value_t *matwh = PS_GetMember(mat, "width/height", NULL);
    
    if (matwh && PS_AsFloat(matwh) > 0)
      wh = PS_AsFloat(matwh);
    
    double matspeed = efeed * dia * dia / (h * h * wh);
    if (speed < matspeed)
      matspeed = speed;
    espeed[e_no] = matspeed;
  }
  
  int eshell   = GetENo("Shell",   model_specific);
  int eskin    = GetENo("Skin",    model_specific);
  int einfill  = GetENo("Infill",  model_specific);
  int esupport = GetENo("Support", model_specific);
  
  Psv set(PS_BlankSettings(ps()));
  set.Set("#global", "material_diameter", dia);
  set.Set("#global", "extruders_enabled_count", (int) PS_ItemCount(dflt()) - 1);
  
  set.Set("#global", "speed_print",     speed);
  set.Set("#global", "speed_wall",      espeed[eshell  ] * ratio);
  set.Set("#global", "speed_topbottom", espeed[eskin   ] * ratio);
  set.Set("#global", "speed_infill",    espeed[einfill ]);
  set.Set("#global", "speed_support",   espeed[esupport]);
  
  PS_MergeSettings(set(), PS_GetMember(qual, "settings", NULL));
  for (int e_no = num_e - 1; e_no >= 0; e_no--) {
    string matname = get_string("Slicing", "E" + to_string(e_no) + "Material");
    const ps_value_t *mat = qualmat.Get("materials", matname.c_str());

    string ext = to_string(e_no);
    set.MergeActive(ext.c_str(), PS_GetMember(mat, "settings", NULL));
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
  
  Psv eval(PS_EvalAllDflt(ps(), set(), dflt()));
  Psv all(PS_CopyValue(dflt()));
  PS_MergeSettings(all(), eval());
  
  ps_value_t *allv = all();
  all.Disown();
  
  return allv;
}

void Settings::SetTargetTemps(Builder &builder) {
  Psv set(FullSettings());
  
  Gtk::SpinButton *sp = NULL;

  int max_e = getNumExtruders();
  if (max_e > 5)
    max_e = 5;
  for (int e_no = 0; e_no < max_e; e_no++) {
    builder->get_widget("E" + to_string(e_no) + "Target", sp);
    if (sp) {
      try {
	string ext = to_string(e_no);
	sp->set_value(PS_AsFloat(set.Get(ext.c_str(), "material_print_temperature_layer_0")));
      } catch (exception &e) {
	cerr << "Could not set Extruder " << e_no << " target" << endl;
      }
    }
  }
  builder->get_widget("BedTarget", sp);
  if (sp) {
    try {
      sp->set_value(PS_AsFloat(set.Get("#global", "material_bed_temperature")));
    } catch (exception &e) {
      cerr << "Could not set bed target" << endl;
    }
  }
}
