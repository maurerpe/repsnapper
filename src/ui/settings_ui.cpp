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
#include <glib/gi18n.h>
#include <gtkmm.h>

#include "ps_helper.h"
#include "settings_ui.h"
#include "settings.h"

Settings_ui::Settings_ui(Glib::RefPtr<Gtk::Builder> &builder, Settings *settings) {
  m_builder = builder;
  m_settings = settings;
}

// convert GUI name to group/key
static bool splitpoint(const string &glade_name, string &group, string &key) {
  int pos = glade_name.find(".");
  if (pos==(int)string::npos) return false;
  group = glade_name.substr(0,pos);
  key = glade_name.substr(pos+1);
  return true;
}

void Settings_ui::set_up_combobox(Gtk::ComboBoxText *combo, vector<string> values) {
  Glib::ustring prev = combo->get_active_text();

  {
    Inhibitor inhibit(&inhibit_callback);
    
    combo->remove_all();
    for (size_t i = 0; i < values.size(); i++)
      combo->append(Glib::ustring(values[i].c_str()));
  }
  
  if (!combo->get_has_entry()) {
    if (prev.size() > 0) {
      combo->set_active_text(prev);
      if (combo->get_active_text().size() == 0)
	combo->set_active(0);
    }
  }
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

///////////////////////////////////////////////////////////////////////

void Settings_ui::set_to_gui(const string &group, const string &key) {
  Inhibitor inhibit(&inhibit_callback);

  Glib::ustring glade_name = group + "." + key;
  // inhibit warning for settings not defined in glade UI:
  if (!m_builder->get_object (glade_name)) {
    //cerr << glade_name << _(" not defined in GUI!")<< endl;
    return;
  }

  Gtk::Widget *w = NULL;
  m_builder->get_widget(glade_name, w);
  if (!w) {
    cerr << _("Missing user interface item ") << glade_name << "\n";
    return;
  }
  
  Gtk::CheckButton *check = dynamic_cast<Gtk::CheckButton *>(w);
  if (check) {
    check->set_active(m_settings->get_boolean(group,key));
    return;
  }
  
  Gtk::Switch *swit = dynamic_cast<Gtk::Switch *>(w);
  if (swit) {
    swit->set_active(m_settings->get_boolean(group,key));
    return;
  }
  
  Gtk::SpinButton *spin = dynamic_cast<Gtk::SpinButton *>(w);
  if (spin) {
    spin->set_value(m_settings->get_double(group,key));
    return;
  }
  
  Gtk::Range *range = dynamic_cast<Gtk::Range *>(w);
  if (range) {
    range->set_value(m_settings->get_double(group,key));
    return;
  }
  
  Gtk::ComboBoxText *combot = dynamic_cast<Gtk::ComboBoxText *>(w);
  if (combot) {
    combobox_set_to(combot, m_settings->get_string(group,key));
    return;
  }
  
  Gtk::Entry *entry = dynamic_cast<Gtk::Entry *>(w);
  if (entry) {
    entry->set_text(m_settings->get_string(group,key));
    return;
  }
  
  Gtk::Expander *exp = dynamic_cast<Gtk::Expander *>(w);
  if (exp) {
    exp->set_expanded(m_settings->get_boolean(group,key));
    return;
  }
  
  Gtk::Paned *paned = dynamic_cast<Gtk::Paned *>(w);
  if (paned) {
    paned->set_position(m_settings->get_integer(group,key));
    return;
  }
  
  Gtk::ColorButton *col = dynamic_cast<Gtk::ColorButton *>(w);
  if(col) {
    vector<double> c = m_settings->get_double_list(group,key);
    Gdk::Color co; co.set_rgb_p(c[0],c[1],c[2]);
    col->set_use_alpha(true);
    col->set_color(co);
    col->set_alpha(c[3] * 65535.0);
    return;
  }
  
  Gtk::TextView *tv = dynamic_cast<Gtk::TextView *>(w);
  if (tv) {
    tv->get_buffer()->set_text(m_settings->get_string(group,key));
    return;
  }

  cerr << "set_to_gui of "<< glade_name << " not done!" << endl;
}


void Settings_ui::get_from_gui(const string &glade_name) {
  if (inhibit_callback)
    return;
  
  if (!m_builder->get_object(glade_name)) {
    cerr << "no such object " << glade_name << endl;
    return;
  }
  
  Gtk::Widget *w = NULL;
  m_builder->get_widget(glade_name, w);
  string group, key;
  if (w == NULL || !splitpoint(glade_name, group, key))
    return;
  
  do { // for using break ...
    //cerr << "get " << group  << "." << key << " from gui"<< endl;
    Gtk::CheckButton *check = dynamic_cast<Gtk::CheckButton *>(w);
    if (check) {
      m_settings->set_boolean(group, key, check->get_active());
      break;
    }
    
    Gtk::Switch *swit = dynamic_cast<Gtk::Switch *>(w);
    if (swit) {
      m_settings->set_boolean(group, key, swit->get_active());
      break;
    }
    
    Gtk::SpinButton *spin = dynamic_cast<Gtk::SpinButton *>(w);
    if (spin) {
      m_settings->set_double(group, key, spin->get_value());
      break;
    }
    
    Gtk::Range *range = dynamic_cast<Gtk::Range *>(w);
    if (range) {
      m_settings->set_double(group, key, range->get_value());
      break;
    }
    
    Gtk::ComboBoxText *combot = dynamic_cast<Gtk::ComboBoxText *>(w);
    if (combot) {
      m_settings->set_string(group,key,combobox_get_active_value(combot));
      break;
    }
    
    Gtk::Entry *e = dynamic_cast<Gtk::Entry *>(w);
    if (e) {
      m_settings->set_string(group,key,e->get_text());
      break;
    }
    
    Gtk::Expander *exp = dynamic_cast<Gtk::Expander *>(w);
    if (exp) {
      m_settings->set_boolean(group,key,exp->get_expanded());
      break;
    }
    
    Gtk::Paned *paned = dynamic_cast<Gtk::Paned *>(w);
    if (paned) {
      m_settings->set_integer(group,key,paned->get_position());
      break;
    }
	
    Gtk::ColorButton *cb = dynamic_cast<Gtk::ColorButton *>(w);
    if (cb) {
      get_colour_from_gui(glade_name);
      break;
    }
    
    Gtk::TextView *tv = dynamic_cast<Gtk::TextView *>(w);
    if (tv) {
      m_settings->set_string(group,key,tv->get_buffer()->get_text());
      break;
    }
    
    cerr << _("Did not get setting from  ") << glade_name << endl;
    break;
  } while (0);
  
  if (key == "Quality") {
    ps_value_t *qual = m_settings->GetQualMat()->Get("quality", m_settings->get_string("Slicing", "Quality").c_str());
    double rel_height = PS_AsFloat(PS_GetMember(qual, "height/nozzle", NULL));
    double nozzle = PS_AsFloat(m_settings->GetDflt()->Get("0", "machine_nozzle_size"));
    m_settings->set_double("Slicing", "LayerHeight", rel_height * nozzle);
    set_to_gui("Slicing", "LayerHeight");
    
    ps_to_gui(PS_GetMember(qual, "settings", NULL));
  }
  
  m_settings->m_signal_visual_settings_changed.emit();
}

void Settings_ui::get_colour_from_gui(const string &glade_name) {
  string group,key;
  if (!splitpoint(glade_name, group,key)) return;
  Gdk::Color c;
  Gtk::ColorButton *w = NULL;
  m_builder->get_widget (glade_name, w);
  if (!w) return;

  c = w->get_color();

  // FIXME: detect 'changed' etc.
  vector<double> d(4);
  d[0] = c.get_red_p();
  d[1] = c.get_green_p();
  d[2] = c.get_blue_p();
  d[3] = (float) (w->get_alpha()) / 65535.0;

  m_settings->set_double_list(group, key, d);

  m_settings->m_signal_visual_settings_changed.emit();
}

// whole group or all groups
void Settings_ui::set_to_gui() {
  Inhibitor inhibit(&inhibit_callback);

  vector< string > groups = m_settings->get_groups();
  for (uint g = 0; g < groups.size(); g++) {
    vector< string > keys = m_settings->get_keys(groups[g]);
    for (uint k = 0; k < keys.size(); k++) {
      set_to_gui(groups[g], keys[k]);
    }
  }

  Gtk::Window *pWindow = NULL;
  m_builder->get_widget("main_window", pWindow);

  int w = m_settings->get_integer("Misc","WindowWidth");
  int h = m_settings->get_integer("Misc","WindowHeight");
  if (pWindow && w > 0 && h > 0)
    pWindow->resize(w,h);
  
  int x = m_settings->get_integer("Misc","WindowPosX");
  int y = m_settings->get_integer("Misc","WindowPosY");
  if (pWindow && x > 0 && y > 0)
    pWindow->move(x,y);
}

void Settings_ui::connect_to_ui() {
  if (m_settings->has_group("Ranges")) {
    vector<string> ranges = m_settings->get_keys("Ranges");
    for (uint i = 0; i < ranges.size(); i++) {
      // get min, max, increment, page-incr.
      vector<double> vals = m_settings->get_double_list("Ranges", ranges[i]);
      Gtk::Widget *w = NULL;
      try {
	m_builder->get_widget(ranges[i], w);
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
  vector< string > groups = m_settings->get_groups();
  for (uint g = 0; g < groups.size(); g++) {
    if (groups[g] == "Ranges") continue; // done that above
    vector< string > keys = m_settings->get_keys(groups[g]);
    for (uint k = 0; k < keys.size(); k++) {
      string glade_name = groups[g] + "." + keys[k];
      if (!m_builder->get_object(glade_name))
	continue;
      Gtk::Widget *w = NULL;
      try {
	m_builder->get_widget(glade_name, w);
	if (!w) {
	  cerr << "Missing user interface item " << glade_name << "\n";
	  continue;
	}
	
	Gtk::CheckButton *check = dynamic_cast<Gtk::CheckButton *>(w);
	if (check) {
	  check->signal_toggled().connect
	    (sigc::bind<string>(sigc::mem_fun(*this, &Settings_ui::get_from_gui), glade_name));
	  continue;
	}
	
	Gtk::Switch *swit = dynamic_cast<Gtk::Switch *>(w);
	if (swit) {
	  swit->property_active().signal_changed().connect
	    (sigc::bind<string>(sigc::mem_fun(*this, &Settings_ui::get_from_gui), glade_name));
	  continue;
	}
	
	Gtk::SpinButton *spin = dynamic_cast<Gtk::SpinButton *>(w);
	if (spin) {
	  spin->signal_value_changed().connect
	    (sigc::bind<string>(sigc::mem_fun(*this, &Settings_ui::get_from_gui), glade_name));
	  continue;
	}
	
	Gtk::Range *range = dynamic_cast<Gtk::Range *>(w);
	if (range) {
	  range->signal_value_changed().connect
	    (sigc::bind<string>(sigc::mem_fun(*this, &Settings_ui::get_from_gui), glade_name));
	  continue;
	}
	
	Gtk::ComboBoxText *combot = dynamic_cast<Gtk::ComboBoxText *>(w);
	if (combot) {
	  if (glade_name == "Slicing.Quality") {
	    set_up_combobox(combot, Psv::GetNames(m_settings->GetQualMat()->Get("quality")));
	  } else if (glade_name == "Slicing.Adhesion") {
	    set_up_combobox(combot, Psv::GetNames(m_settings->GetPs()->Get("#global", "#set", "adhesion_type", "options")));
	  }
	  combot->signal_changed().connect
	    (sigc::bind<string>(sigc::mem_fun(*this, &Settings_ui::get_from_gui), glade_name));
	  continue;
	}
	
	Gtk::Entry *e = dynamic_cast<Gtk::Entry *>(w);
	if (e) {
	  e->signal_changed().connect
	    (sigc::bind<string>(sigc::mem_fun(*this, &Settings_ui::get_from_gui), glade_name));
	  continue;
	}

	Gtk::Paned *paned = dynamic_cast<Gtk::Paned *>(w);
	if (paned) {
	  paned->property_position().signal_changed().connect
	    (sigc::bind<string>(sigc::mem_fun(*this, &Settings_ui::get_from_gui), glade_name));
	  continue;
	}
	
	Gtk::Expander *exp = dynamic_cast<Gtk::Expander *>(w);
	if (exp) {
	  exp->property_expanded().signal_changed().connect
	    (sigc::bind<string>(sigc::mem_fun(*this, &Settings_ui::get_from_gui), glade_name));
	  continue;
	}
	
	Gtk::ColorButton *cb = dynamic_cast<Gtk::ColorButton *>(w);
	if (cb) {
	  cb->signal_color_set().connect
	    (sigc::bind<string>(sigc::mem_fun(*this, &Settings_ui::get_from_gui), glade_name));
	  continue;
	}
	
	Gtk::TextView *tv = dynamic_cast<Gtk::TextView *>(w);
	if (tv) {
	  tv->get_buffer()->signal_changed().connect
	    (sigc::bind<string>(sigc::mem_fun(*this, &Settings_ui::get_from_gui), glade_name));
	  continue;
	}
      } catch (Glib::Exception &ex) {
      }
    }
  }
  
  Gtk::ComboBoxText *combot;
  m_builder->get_widget("print_serialspeed", combot);
  set_up_combobox(combot, Settings::get_serial_speeds());
  
  m_builder->get_widget("printer_E0Material", combot);
  set_up_combobox(combot, Psv::GetNames(m_settings->GetQualMat()->Get("materials")));
  combot->signal_changed().connect
    (sigc::bind(sigc::bind<string>(sigc::mem_fun(*this, &Settings_ui::mat_changed), "0"), combot));
     
  m_builder->get_widget("printer_E1Material", combot);
  set_up_combobox(combot, Psv::GetNames(m_settings->GetQualMat()->Get("materials")));
  combot->signal_changed().connect
    (sigc::bind(sigc::bind<string>(sigc::mem_fun(*this, &Settings_ui::mat_changed), "1"), combot));
  
  SetTargetTemps();
  
  /* Update UI with defaults */
  m_settings->m_signal_update_settings_gui.emit();
}

void Settings_ui::mat_changed(Gtk::ComboBoxText *combo, string ext) {
  if (inhibit_callback)
    return;
  
  Glib::ustring mat(combo->get_active_text());

  if (mat.size() > 0) {
    PS_AddMember(m_settings->GetPrinter()->Get("repsnapper", ext.c_str()), "material", PS_NewString(mat.c_str()));
    
    SetTargetTemps();
    ps_to_gui(m_settings->GetQualMat()->Get("materials", mat.c_str(), "settings"));
  }
}

void Settings_ui::printer_changed(void) {
  {
    Inhibitor inhibit(&inhibit_callback);
    Gtk::ComboBoxText *w;
    m_builder->get_widget("printer_E0Material", w);
    const char *str = PS_GetString(m_settings->GetPrinter()->Get("repsnapper", "0", "material"));
    if (str)
      w->set_active_text(str);
    
    m_builder->get_widget("printer_E1Material", w);
    str = PS_GetString(m_settings->GetPrinter()->Get("repsnapper", "1", "material"));
    if (str)
      w->set_active_text(str);
  }
  
  SetTargetTemps();
  Psvi vi(m_settings->GetPrinter()->Get("repsnapper"));
  while (vi.Next()) {
    if (strcmp(vi.Key(), "#global") == 0)
      continue;
    
    ps_to_gui(m_settings->GetQualMat()->Get("materials", vi.Key(), "settings"));
  }
}

void Settings_ui::qualmat_changed(void) {
  Gtk::ComboBoxText *w;
  m_builder->get_widget("Slicing.Quality", w);
  set_up_combobox(w, Psv::GetNames(m_settings->GetQualMat()->Get("quality")));
    
  vector<string> mat(Psv::GetNames(m_settings->GetQualMat()->Get("materials")));
  m_builder->get_widget("printer_E0Material", w);
  set_up_combobox(w, mat);
  m_builder->get_widget("printer_E1Material", w);
  set_up_combobox(w, mat);
  m_builder->get_widget("ext_material", w);
  set_up_combobox(w, mat);
}

void Settings_ui::ps_to_gui(const ps_value_t *set) {
  ps_value_t *ext, *val;
  
  if (set == NULL)
    return;
  
  if ((ext = PS_GetMember(set, "#global", NULL))) {
    if ((val = PS_GetMember(ext, "infill_sparse_density", NULL))) {
      m_settings->set_double("Slicing", "InfillPercent", PS_AsFloat(val));
      set_to_gui("Slicing", "InfillPercent");
    }
    
    if ((val = PS_GetMember(ext, "adhesion_type", NULL))) {
      m_settings->set_string("Slicing", "Adhesion", PS_GetString(val));
      set_to_gui("Slicing", "Adhesion");
    }
    
    if ((val = PS_GetMember(ext, "magic_spiralize", NULL))) {
      m_settings->set_boolean("Slicing", "Spiralize", PS_AsBoolean(val));
      set_to_gui("Slicing", "Spiralize");
    }
    
    if ((val = PS_GetMember(ext, "wall_line_count", NULL))) {
      m_settings->set_integer("Slicing", "ShellCount", PS_AsInteger(val));
      set_to_gui("Slicing", "ShellCount");
    }
    
    if ((val = PS_GetMember(ext, "top_layers", NULL))) {
      m_settings->set_integer("Slicing", "Skins", PS_AsInteger(val));
      set_to_gui("Slicing", "Skins");
    }
    
    if ((val = PS_GetMember(ext, "bottom_layers", NULL))) {
      m_settings->set_integer("Slicing", "Skins", PS_AsInteger(val));
      set_to_gui("Slicing", "Skins");
    }
  }
}

void Settings_ui::SetTargetTemps() {
  Psv set(m_settings->FullSettings());
  
  Gtk::SpinButton *sp = NULL;

  int max_e = m_settings->getNumExtruders();
  if (max_e > 5)
    max_e = 5;
  for (int e_no = 0; e_no < max_e; e_no++) {
    m_builder->get_widget("E" + to_string(e_no) + "Target", sp);
    if (sp) {
      try {
	string ext = to_string(e_no);
	sp->set_value(PS_AsFloat(set.Get(ext.c_str(), "material_print_temperature_layer_0")));
      } catch (exception &e) {
	cerr << "Could not set Extruder " << e_no << " target" << endl;
      }
    }
  }
  m_builder->get_widget("BedTarget", sp);
  if (sp) {
    try {
      sp->set_value(PS_AsFloat(set.Get("#global", "material_bed_temperature")));
    } catch (exception &e) {
      cerr << "Could not set bed target" << endl;
    }
  }
}
