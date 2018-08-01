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

// make "ExtruderN" group, if i<0 (not given), use current selected Extruder number
string Settings::numberedExtruder(const string &group, int num) const {
  if (group == "Extruder") {
    ostringstream oss; oss << "Extruder" << num;
    //cerr << "call for " << oss.str() << endl;
    return oss.str();
  }
  return group;
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

  set_double("Hardware","PrintMargin.X", 10);
  set_double("Hardware","PrintMargin.Y", 10);
}

void Settings::load_settings(Glib::RefPtr<Gio::File> file) {
  inhibit_callback = true;
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
  
  uint ne = getNumExtruders();
  for (uint k = 0; k < ne; k++) {
    if (!has_key(numberedExtruder("Extruder",k), "OffsetX"))
      set_double(numberedExtruder("Extruder",k), "OffsetX", 0);
    if (!has_key(numberedExtruder("Extruder",k), "OffsetY"))
      set_double(numberedExtruder("Extruder",k), "OffsetY", 0);
  }
  SelectExtruder(0);

  inhibit_callback = false;
  m_user_changed = false;
  m_signal_visual_settings_changed.emit();
  m_signal_update_settings_gui.emit();
}

void Settings::save_settings(Glib::RefPtr<Gio::File> file) {
  inhibit_callback = true;
  set_string("Global","Version",VERSION);

  Glib::ustring contents = to_data();
  // cerr << contents << endl;
  Glib::file_set_contents(file->get_path(), contents);

  SelectExtruder(selectedExtruder); // reload default extruder

  inhibit_callback = false;
  // all changes safely saved
  m_user_changed = false;
}

void Settings::load_printer_settings(void) {
  Psv search(PS_NewList());
  PS_AppendToList(search(), PS_NewString("/usr/share/cura/resources/definitions"));
  PS_AppendToList(search(), PS_NewString("/usr/share/cura/resources/extruders"));  
  ps.Take(PS_New("/home/maurerpe/.config/repsnapper/cr10mini.def.json", search()));
  dflt.Take(PS_GetDefaults(ps()));

  Psf config_file("/home/maurerpe/.config/repsnapper/cura_settings.json");
  config.Take(PS_ParseJsonFile(config_file()));
  config_file.close();
}

void Settings::set_to_gui(Builder &builder,
			  const string &group, const string &key) {
  inhibit_callback = true;
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
  
  // update currently edited extruder
  if (glade_name.substr(0,8) == "Extruder")
    copyGroup("Extruder",numberedExtruder("Extruder", selectedExtruder));
  
  if (key == "Material")
    SetTargetTemps(builder);
  
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
  inhibit_callback = true;
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
  inhibit_callback = false;
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
	    set_up_combobox(combot, Psv::GetNames(PS_GetItem(PS_GetItem(PS_GetMember(config(), "nozzles", NULL), 0), 1)));
	  } else if (glade_name == "Slicing.Material") {
	    set_up_combobox(combot, Psv::GetNames(PS_GetMember(config(), "materials", NULL)));
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

double Settings::GetExtrudedMaterialWidth(double layerheight) const {
  // ExtrudedMaterialWidthRatio is preset by user
  return min(max(get_double("Extruder","MinimumLineWidth"),
		 get_double("Extruder","ExtrudedMaterialWidthRatio") * layerheight),
	     get_double("Extruder","MaximumLineWidth"));
}

// TODO This depends whether lines are packed or not - ellipsis/rectangle

// how much mm filament material per extruded line length mm -> E gcode
double Settings::GetExtrusionPerMM(double layerheight) const {
  double f = get_double("Extruder","ExtrusionFactor"); // overall factor
  if (get_boolean("Extruder","CalibrateInput")) {  // means we use input filament diameter
    const double matWidth = GetExtrudedMaterialWidth(layerheight); // this is the goal
    // otherwise we just work back from the extruded diameter for now.
    const double filamentdiameter = get_double("Extruder","FilamentDiameter");
    f *= (matWidth * matWidth) / (filamentdiameter * filamentdiameter);
  } // else: we work in terms of output anyway;

  return f;
}

// return infill distance in mm
double Settings::GetInfillDistance(double layerthickness, float percent) const {
  double fullInfillDistance = GetExtrudedMaterialWidth(layerthickness);
  if (percent == 0) return 10000000;
  return fullInfillDistance * (100./percent);
}

uint Settings::getNumExtruders() const {
  vector< Glib::ustring > groups = get_groups();
  uint num=0;
  for (uint g = 0; g < groups.size(); g++)
    if (groups[g].substr(0,8) == "Extruder"
	&& groups[g].length() > 8 ) // count only numbered
      num++;
  return num;
}

vector<char> Settings::get_extruder_letters() const {
  uint num = getNumExtruders();
  vector<char> letters(num);
  for (uint i = 0; i < num; i++)
    letters[i] = get_string(numberedExtruder("Extruder",i),"GCLetter")[0];
  return letters;
}

uint Settings::GetSupportExtruder() const {
  uint num = getNumExtruders();
  for (uint i = 0; i < num; i++)
    if (get_boolean(numberedExtruder("Extruder",i),"UseForSupport"))
      return i;
  return 0;
}

Vector3d Settings::get_extruder_offset(uint num) const {
  string ext = numberedExtruder("Extruder",num);
  return Vector3d(get_double(ext, "OffsetX"),
		  get_double(ext, "OffsetY"), 0.);
}

void Settings::copyGroup(const string &from, const string &to) {
  vector<string> keys = get_keys(from);
  for (uint i = 0; i < keys.size(); i++)
    set_value(to, keys[i], get_value(from, keys[i]));
}

// create new
void Settings::CopyExtruder(uint num) {
  uint total = getNumExtruders();
  string from = numberedExtruder("Extruder",num);
  string to   = numberedExtruder("Extruder",total);
  copyGroup(from, to);
}

void Settings::RemoveExtruder(uint num) {
  ostringstream oss; oss << "Extruder"<<num;
  remove_group(oss.str());
}

void Settings::SelectExtruder(uint num, Builder *builder) {
  if (num >= getNumExtruders()) return;
  selectedExtruder = num;
  copyGroup(numberedExtruder("Extruder",num),"Extruder");
  // show Extruder settings on gui
  if (builder) {
    set_to_gui(*builder, "Extruder");
  }
}

Matrix4d Settings::getBasicTransformation(Matrix4d T) const {
  Vector3d t;
  T.get_translation(t);
  const Vector3d margin = getPrintMargin();
  double rsize = get_double("Raft","Size") * (get_boolean("Raft","Enable")?1:0);
  t+= Vector3d(margin.x() + rsize, margin.y() + rsize, 0);
  T.set_translation(t);
  return T;
}

Vector3d Settings::getPrintVolume() const {
  return Vector3d(PS_AsFloat(dflt.Get("#global", "machine_width")),
		  PS_AsFloat(dflt.Get("#global", "machine_depth")),
		  PS_AsFloat(dflt.Get("#global", "machine_height")));
}

Vector3d Settings::getPrintMargin() const {
  Vector3d margin(get_double("Hardware","PrintMargin.X"),
		  get_double("Hardware","PrintMargin.Y"),
		  0);
  Vector3d maxoff = Vector3d::ZERO;
  uint num = getNumExtruders();
  for (uint i = 0; i < num ; i++) {
    string ext = numberedExtruder("Extruder",i);
    double offx = 0, offy = 0;
    try {
      offx = abs(get_double(ext, "OffsetX"));
      offy = abs(get_double(ext, "OffsetY"));
    } catch (const Glib::KeyFileError &err) {
    }
    if (offx > abs(maxoff.x())) maxoff.x() = offx;
    if (offy > abs(maxoff.y())) maxoff.y() = offy;
  }
  return margin + maxoff;
}

// Locate it in relation to ourselves ...
string Settings::get_image_path() {
  string basename = Glib::path_get_dirname(filename);
  return Glib::build_filename (basename, get_string("Global","Image"));
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

ps_value_t *Settings::FullSettings() {
  string qualname = get_string("Slicing", "Quality");
  
  const ps_value_t *nn = PS_GetMember(config(), "nozzles", NULL);
  const ps_value_t *xx = PS_GetItem(PS_GetItem(nn, 0), 1);
  const ps_value_t *qual = PS_GetMember(xx, qualname.c_str(), NULL);
  
  double dia = PS_AsFloat(dflt.Get("#global", "material_diameter"));
  
  double noz = 0.4;
  //double h = PS_AsFloat(PS_GetMember(qual, "layer-height", NULL));
  double wh = PS_AsFloat(PS_GetMember(qual, "width/height", NULL));
  double speed = PS_AsFloat(PS_GetMember(qual, "speed", NULL));
  double ratio = PS_AsFloat(PS_GetMember(qual, "wall-speed-ratio", NULL));

  double h = get_double("Slicing", "LayerHeight");
  bool support = get_boolean("Slicing", "Support");
  double infill = get_double("Slicing", "InfillPercent");
  int shells = get_integer("Slicing", "ShellCount");
  int skins = get_integer("Slicing", "Skins");
  string matname = get_string("Slicing", "Material");
  bool spiralize = get_boolean("Slicing","Spiralize");
  
  const ps_value_t *mat = config.Get("materials", matname.c_str());
  if (mat == NULL)
    cout << endl << "Unknown material: " << matname << endl;
  double efeed = PS_AsFloat(PS_GetItem(PS_GetItem(PS_GetMember(mat, "nozzle-feedrate", NULL), 0), 1));
  const ps_value_t *matwh = PS_GetMember(mat, "width/height", NULL);
  if (matwh && PS_AsFloat(matwh) > 0)
    wh = PS_AsFloat(matwh);
  
  double espeed = efeed * dia * dia / (h * h * wh);
  if (espeed > 0 && espeed < speed)
    speed = espeed;
  
  Psv set(PS_BlankSettings(ps()));
  set.Set("#global", "material_diameter", dia);
  set.Set("#global", "machine_nozzle_size", noz);
  
  set.Set("0", "material_diameter", dia);
  set.Set("0", "machine_nozzle_size", noz);
  
  set.Set("#global", "layer_height", h);
  set.Set("#global", "line_width", h * wh);
  set.Set("#global", "speed_print", speed);
  set.Set("#global", "speed_wall", speed * ratio);
  set.Set("#global", "wall_line_count", spiralize ? 1 : shells);
  set.Set("#global", "top_layers", skins);
  set.Set("#global", "bottom_layers", skins);
  set.Set("#global", "infill_sparse_density", infill);
  set.Set("#global", "support_enable", support);
  set.Set("#global", "magic_spiralize", spiralize);
  
  PS_MergeSettings(set(), PS_GetMember(qual, "settings", NULL));
  PS_MergeSettings(set(), PS_GetMember(mat,  "settings", NULL));
  
  return PS_EvalAll(ps(), set());
}

void Settings::SetTargetTemps(Builder &builder) {
  Psv set(FullSettings());
  
  Gtk::SpinButton *sp = NULL;
  
  builder->get_widget("E1Target", sp);
  if (sp) {
    try {
      sp->set_value(PS_AsFloat(set.Get("#global", "material_print_temperature_layer_0")));
    } catch (exception &e) {
      cerr << "Could not set Extruder 1 target" << endl;
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
