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

#include <cstdlib>
#include <glib/gi18n.h>
#include <gtkmm.h>

#include "printer_dlg.h"
#include "filters.h"

//////////////////////////// PrinterWidget /////////////////////////////////

PrinterWidget::PrinterWidget(Gtk::Widget *widget, ps_value_t **dflt,
			     ps_value_t **printer, string section, string ext, string name) {
  m_widget = widget;

  m_dflt = dflt;
  m_printer = printer;
  m_section = section;
  m_ext = ext;
  m_name = name;

  m_inhibit_change = false;
}

PrinterWidget::~PrinterWidget() {
}

void PrinterWidget::LoadValue(void) {
}

int PrinterWidget::IsSet(void) {
  return GetValue() != NULL;
}

void PrinterWidget::SetActive(bool active) {
  m_widget->set_sensitive(active);

  if (active)
    Changed();
  else
    SetValue(NULL);
}

void PrinterWidget::ChangeExtruder(string ext) {
  m_ext = ext;
  
  LoadValue();
}

ps_value_t *PrinterWidget::GetValue() {
  ps_value_t *prime = PS_GetMember(PS_GetMember(PS_GetMember(*m_printer, m_section.c_str(), NULL), m_ext.c_str(), NULL), m_name.c_str(), NULL);

  if (prime || m_section == "repsnapper")
    return prime;

  m_inhibit_change = false;
  
  string ext = m_ext;
  if (m_ext != "#global")
    ext = "0";
  
  return PS_GetMember(PS_GetMember(*m_dflt, ext.c_str(), NULL), m_name.c_str(), NULL);
}

void PrinterWidget::SetValue(ps_value_t *v) {
  if (v == NULL)
    PS_RemoveMember(PS_GetMember(PS_GetMember(*m_printer, m_section.c_str(), NULL), m_ext.c_str(), NULL), m_name.c_str());
  else
    PS_AddMember(PS_GetMember(PS_GetMember(*m_printer, m_section.c_str(), NULL), m_ext.c_str(), NULL), m_name.c_str(), v);
}

void PrinterWidget::Changed(void) {
}

//////////////////////////// PrinterSpin ///////////////////////////////////

PrinterSpin::PrinterSpin(Gtk::Widget *widget, ps_value_t **dflt,
			 ps_value_t **printer, string section, string ext, string name) : PrinterWidget(widget, dflt, printer, section, ext, name) {

  Gtk::SpinButton *w = dynamic_cast<Gtk::SpinButton *>(m_widget);

  w->signal_value_changed().connect(sigc::mem_fun(*this, &PrinterSpin::Changed));

  LoadValue();
}

void PrinterSpin::LoadValue(void) {
  Gtk::SpinButton *w = dynamic_cast<Gtk::SpinButton *>(m_widget);

  Inhibitor inhibit(&m_inhibit_change);
  w->set_value(PS_AsFloat(GetValue()));
}

void PrinterSpin::Changed(void) {
  if (m_inhibit_change)
    return;
  
  Gtk::SpinButton *w = dynamic_cast<Gtk::SpinButton *>(m_widget);
  
  SetValue(PS_NewFloat(w->get_value()));
}

/////////////////////////// PrinterSwitch ///////////////////////////////////

PrinterSwitch::PrinterSwitch(Gtk::Widget *widget, ps_value_t **dflt,
			     ps_value_t **printer, string section, string ext, string name) :
  PrinterWidget(widget, dflt, printer, section, ext, name) {
  Gtk::Switch *w = dynamic_cast<Gtk::Switch *>(m_widget);
  
  w->property_active().signal_changed().connect
    (sigc::mem_fun(*this, &PrinterSwitch::Changed));
  
  LoadValue();
}

void PrinterSwitch::LoadValue(void) {
  Gtk::Switch *w = dynamic_cast<Gtk::Switch *>(m_widget);
  
  Inhibitor inhibit(&m_inhibit_change);
  w->set_active(PS_AsBoolean(GetValue()));
}

void PrinterSwitch::Changed(void) {
  if (m_inhibit_change)
    return;
  
  Gtk::Switch *w = dynamic_cast<Gtk::Switch *>(m_widget);
  
  SetValue(PS_NewBoolean(w->get_active()));
}

//////////////////////////// PrinterCombo //////////////////////////////////

PrinterCombo::PrinterCombo(Gtk::Widget *widget, ps_value_t **dflt,
			   const Psv *ps,
			   ps_value_t **printer, string section, string ext, string name) :
  PrinterWidget(widget, dflt, printer, section, ext, name), m_ps(ps) {
  Gtk::ComboBoxText *w = dynamic_cast<Gtk::ComboBoxText *>(m_widget);
  
  w->signal_changed().connect
    (sigc::mem_fun(*this, &PrinterCombo::Changed));
  
  LoadValue();
}

void PrinterCombo::LoadValue(void) {
  Gtk::ComboBoxText *w = dynamic_cast<Gtk::ComboBoxText *>(m_widget);
  
  SetupCombo();

  const char *str = PS_GetString(GetValue());
  if (str) {
    Inhibitor inhibit(&m_inhibit_change);
    w->set_active_text(Glib::ustring(str));
  }
}

void PrinterCombo::Changed(void) {
  if (m_inhibit_change)
    return;
  
  Gtk::ComboBoxText *w = dynamic_cast<Gtk::ComboBoxText *>(m_widget);
  
  SetValue(PS_NewString(w->get_active_text().c_str()));
}

void PrinterCombo::SetupCombo(void) {
  if (m_section == "repsnapper")
    return;
  
  SetupCombo(Psv::GetNames(m_ps->Get(m_ext.c_str(), "#set", m_name.c_str(), "options")));
}

void PrinterCombo::SetupCombo(vector<string> values) {
  Gtk::ComboBoxText *w = dynamic_cast<Gtk::ComboBoxText *>(m_widget);
  
  Glib::ustring prev = w->get_active_text();

  {
    Inhibitor inhibit(&m_inhibit_change);
    w->remove_all();
    
    for (size_t count = 0; count < values.size(); count++)
      w->append(values[count]);
  }
  
  if (prev.size() > 0) {
    w->set_active_text(prev);
    if (w->get_active_text().size() == 0)
      w->set_active(0);
  }
}

//////////////////////////////// PrinterEntry ////////////////////////////////

PrinterEntry::PrinterEntry(Gtk::Widget *widget, ps_value_t **dflt,
			   ps_value_t **printer, string section, string ext, string name) :
  PrinterWidget(widget, dflt, printer, section, ext, name) {
  m_buffer = Gtk::EntryBuffer::create();
  
  Gtk::Entry *w = dynamic_cast<Gtk::Entry *>(m_widget);
  w->set_buffer(m_buffer);
  
  m_buffer->property_text().signal_changed().connect
    (sigc::mem_fun(*this, &PrinterEntry::Changed));
  
  LoadValue();
}

void PrinterEntry::LoadValue(void) {
  const char *decoded = PS_GetString(GetValue());

  Inhibitor inhibit(&m_inhibit_change);
  m_buffer->set_text(decoded ? decoded : "");
}

void PrinterEntry::Changed(void) {
  if (m_inhibit_change)
    return;
  
  SetValue(PS_NewString(m_buffer->get_text().c_str()));
}

//////////////////////////////// PrinterText ////////////////////////////////

PrinterText::PrinterText(Gtk::Widget *widget, ps_value_t **dflt,
			 ps_value_t **printer, string section, string ext, string name) :
  PrinterWidget(widget, dflt, printer, section, ext, name) {
  m_buffer = Gtk::TextBuffer::create();
  
  Gtk::TextView *w = dynamic_cast<Gtk::TextView *>(m_widget);
  w->set_buffer(m_buffer);
  
  m_buffer->signal_changed().connect
    (sigc::mem_fun(*this, &PrinterText::Changed));
  
  LoadValue();
}

void PrinterText::LoadValue(void) {
  string decoded = Decode(GetValue());

  Inhibitor inhibit(&m_inhibit_change);
  m_buffer->set_text(decoded);
}

void PrinterText::Changed(void) {
  if (m_inhibit_change)
    return;
  
  SetValue(Encode(m_buffer->get_text()));
}

ps_value_t *PrinterText::Encode(Glib::ustring str) {
  return PS_NewString(str.c_str());
}

Glib::ustring PrinterText::Decode(const ps_value_t *v) {
  const char *str = PS_GetString(v);

  if (str == NULL)
    return "";
  
  return str;
}

//////////////////////////////// PrinterJson //////////////////////////////

PrinterJson::PrinterJson(Gtk::Widget *widget, ps_value_t **dflt,
			 ps_value_t **printer, string section, string ext, string name) : PrinterText(widget, dflt, printer, section, ext, name) {
}

ps_value_t *PrinterJson::Encode(Glib::ustring str) {
  return PS_ParseJsonString(str.c_str());
}

Glib::ustring PrinterJson::Decode(const ps_value_t *v) {
  return PS_ToString(v);
}

//////////////////////////////// PrinterCheck //////////////////////////////

PrinterCheck::PrinterCheck(Gtk::Widget *widget, ps_value_t **dflt, ps_value_t **printer, string section, string ext, string name) :
  PrinterWidget(widget, dflt, printer, section, ext, name) {
  Gtk::CheckButton *w = dynamic_cast<Gtk::CheckButton *>(m_widget);
  
  w->signal_toggled().connect
    (sigc::mem_fun(*this, &PrinterCheck::Changed));
}

void PrinterCheck::AddWidget(PrinterWidget *widget) {
  m_sub.push_back(widget);
}

void PrinterCheck::LoadValue(void) {
  Gtk::CheckButton *w = dynamic_cast<Gtk::CheckButton *>(m_widget);

  if (m_sub.size() < 0)
    return;
  
  bool active;
  if (m_section == "")
    active = m_sub[0]->IsSet();
  else
    active = PS_AsBoolean(GetValue());

  Inhibitor inhibit(&m_inhibit_change);
  w->set_active(active);
}

void PrinterCheck::SetActive(bool active) {
  PrinterWidget::SetActive(active);
  
  if (active) {
    LoadValue();
    return;
  }
  
  for (size_t count = 0; count < m_sub.size(); count++)
    m_sub[count]->SetActive(active);
}

void PrinterCheck::Changed(void) {
  if (m_inhibit_change)
    return;
  
  Gtk::CheckButton *w = dynamic_cast<Gtk::CheckButton *>(m_widget);
  
  bool active = w->get_active();
  
  if (m_section != "")
    SetValue(PS_NewBoolean(active));
  
  for (size_t count = 0; count < m_sub.size(); count++)
    m_sub[count]->SetActive(active);
}

////////////////////////////// PrinterDlg ///////////////////////////////////

PrinterDlg::PrinterDlg(Glib::RefPtr<Gtk::Builder> builder, Settings *settings, SetDlg *set) : m_cust_global(set, m_settings->GetPs()), m_cust_extruder(set, m_settings->GetPs()) {
  m_settings = settings;
  m_printer = PS_CopyValue((*m_settings->GetPrinter())());
  vector<string> ext = {"0"};
  Psv ps(m_settings->load_printer(ext));
  m_dflt = PS_GetDefaults(ps());
  m_import_folder = "/usr/share/cura/resources/definitions";
  
  Gtk::Box *b;
  builder->get_widget("print_cust", b);
  b->add(m_cust_global);
  m_cust_global.SetWindow(m_dlg);
  builder->get_widget("ext_cust", b);
  b->add(m_cust_extruder);
  m_cust_extruder.SetWindow(m_dlg);
  m_inhibit_extruder = false;

  builder->get_widget("ext_tree", m_tree);
  m_cols.add(m_name_col);
  m_store = Gtk::ListStore::create(m_cols);
  m_tree->set_model(m_store);
  m_tree->set_headers_visible(false);
  m_tree->append_column(_("Name"), m_name_col);

  builder->get_widget("ext_new", m_new);
  builder->get_widget("ext_remove", m_remove);
  m_new->signal_clicked().connect(sigc::mem_fun(*this, &PrinterDlg::New));
  m_remove->signal_clicked().connect(sigc::mem_fun(*this, &PrinterDlg::Remove));
  
  m_tree->get_selection()->signal_changed().connect
    (sigc::mem_fun(*this, &PrinterDlg::ExtruderSelected));
  
  Gtk::Button *bt;
  builder->get_widget("printer_ok", bt);
  bt->signal_clicked().connect(sigc::mem_fun(*this, &PrinterDlg::OK));
  builder->get_widget("printer_cancel", bt);
  bt->signal_clicked().connect(sigc::mem_fun(*this, &PrinterDlg::Cancel));
  builder->get_widget("printer_save", bt);
  bt->signal_clicked().connect(sigc::mem_fun(*this, &PrinterDlg::Save));
  builder->get_widget("printer_saveas", bt);
  bt->signal_clicked().connect(sigc::mem_fun(*this, &PrinterDlg::SaveAs));
  builder->get_widget("printer_load", bt);
  bt->signal_clicked().connect(sigc::mem_fun(*this, &PrinterDlg::Load));
  builder->get_widget("printer_import", bt);
  bt->signal_clicked().connect(sigc::mem_fun(*this, &PrinterDlg::Import));
  
  builder->get_widget("printer_dlg", m_dlg);
  
  Add(builder, "print_name", "repsnapper", "#global", "name");
  
  Add(builder, "print_shape", "overrides", "#global", "machine_shape");
  Add(builder, "print_flavor", "overrides", "#global", "machine_gcode_flavor");
  Add(builder, "print_center", "overrides", "#global", "machine_center_is_zero");
  Add(builder, "print_heatedbed", "overrides", "#global", "machine_heated_bed");
  
  Add(builder, "print_volx", "overrides", "#global", "machine_width");
  Add(builder, "print_voly", "overrides", "#global", "machine_depth");
  Add(builder, "print_volz", "overrides", "#global", "machine_height");

  Add(builder, "print_feedx", "overrides", "#global", "machine_max_feedrate_x");
  Add(builder, "print_feedy", "overrides", "#global", "machine_max_feedrate_y");
  Add(builder, "print_feedz", "overrides", "#global", "machine_max_feedrate_z");
  Add(builder, "print_feede", "overrides", "#global", "machine_max_feedrate_e");
  
  Add(builder, "print_homex", "repsnapper", "#global", "home_feedrate_x");
  Add(builder, "print_homey", "repsnapper", "#global", "home_feedrate_y");
  Add(builder, "print_homez", "repsnapper", "#global", "home_feedrate_z");

  Add(builder, "print_accelx", "overrides", "#global", "machine_max_acceleration_x");
  Add(builder, "print_accely", "overrides", "#global", "machine_max_acceleration_y");
  Add(builder, "print_accelz", "overrides", "#global", "machine_max_acceleration_z");
  Add(builder, "print_accele", "overrides", "#global", "machine_max_acceleration_e");
  AddCheck(builder, "print_accel_enable", "overrides", "#global", "acceleration_enabled", 4);

  Add(builder, "print_jerkxy", "overrides", "#global", "machine_max_jerk_xy");
  Add(builder, "print_jerkz", "overrides", "#global", "machine_max_jerk_z");
  Add(builder, "print_jerke", "overrides", "#global", "machine_max_jerk_e");
  AddCheck(builder, "print_jerk_enable", "overrides", "#global", "jerk_enabled", 3);

  Add(builder, "print_gantry", "overrides", "#global", "gantry_height");
  AddCheck(builder, "print_gantry_enable", "", "", "", 1);
  
  Add(builder, "print_retract", "overrides", "#global", "retraction_amount");
  AddCheck(builder, "print_retract_enable", "overrides", "#global", "retraction_enable", 1);
  
  Add(builder, "print_minlayer", "overrides", "#global", "cool_min_layer_time");
  Add(builder, "print_caltime", "repsnapper", "#global", "calibrate_time");
  Add(builder, "print_adhesion", "overrides", "#global", "adhesion_type");
  Add(builder, "print_skirt", "overrides", "#global", "skirt_line_count");
  Add(builder, "print_serialspeed", "repsnapper", "#global", "serial_speed");
  
  Add(builder, "gcode_start", "overrides", "#global", "machine_start_gcode");
  Add(builder, "gcode_end", "overrides", "#global", "machine_end_gcode");
  
  AddJson(builder, "poly_printhead", "overrides", "#global", "machine_head_polygon");
  AddCheck(builder, "poly_printhead_enable", "", "", "", 1);

  AddJson(builder, "poly_printfans", "overrides", "#global", "machine_head_with_fans_polygon");
  AddCheck(builder, "poly_printfans_enable", "", "", "", 1);
  
  AddJson(builder, "poly_bedcut", "repsnapper", "#global", "bed_cutouts");
  AddCheck(builder, "poly_bedcut_enable", "", "", "", 1);
  
  AddJson(builder, "poly_disallowed", "overrides", "#global", "machine_disallowed_areas");
  AddCheck(builder, "poly_disallowed_enable", "", "", "", 1);
  
  AddEList(builder, "ext_shell",   "repsnapper", "#global", "shell_extruder");
  AddEList(builder, "ext_skin",    "repsnapper", "#global", "skin_extruder");
  AddEList(builder, "ext_infill",  "repsnapper", "#global", "infill_extruder");
  AddEList(builder, "ext_support", "repsnapper", "#global", "support_extruder");
  
  AddExt(builder, "ext_material", "repsnapper", "0", "material");
  AddExt(builder, "ext_nozzle", "overrides", "0", "machine_nozzle_size");
  AddExt(builder, "ext_filament", "overrides", "0", "material_diameter");
  AddExt(builder, "ext_offsetx", "overrides", "0", "machine_nozzle_offset_x");
  AddExt(builder, "ext_offsety", "overrides", "0", "machine_nozzle_offset_y");
  
  LoadAll();
}

PrinterDlg::~PrinterDlg() {
  for (size_t count = 0; count < m_widgets.size(); count++)
    delete m_widgets[count];
}

void PrinterDlg::Show(Gtk::Window &trans) {
  PS_FreeValue(m_printer);
  m_printer = PS_CopyValue((*m_settings->GetPrinter())());
  
  LoadAll();
  
  m_dlg->set_transient_for(trans);
  m_dlg->show();
  m_dlg->raise();
}

void PrinterDlg::Add(Glib::RefPtr<Gtk::Builder> builder, string id, string section, string ext, string name) {
  Gtk::Widget *w;
  builder->get_widget(id, w);
  
  if (w == NULL)
    throw invalid_argument(_("Unknown widget ") + id);
  
  if (ext == "#global")
    m_cust_global.AddToMask(name);
  else
    m_cust_extruder.AddToMask(name);
  
  if (dynamic_cast<Gtk::SpinButton *> (w)) {
    m_widgets.push_back(new PrinterSpin(w, &m_dflt, &m_printer, section, ext, name));
    return;
  }

  if (dynamic_cast<Gtk::Switch *> (w)) {
    m_widgets.push_back(new PrinterSwitch(w, &m_dflt, &m_printer, section, ext, name));
    return;
  }

  if (dynamic_cast<Gtk::ComboBoxText *> (w)) {
    m_widgets.push_back(new PrinterCombo(w, &m_dflt, m_settings->GetPs(), &m_printer, section, ext, name));
    return;
  }

  if (dynamic_cast<Gtk::Entry *> (w)) {
    m_widgets.push_back(new PrinterEntry(w, &m_dflt, &m_printer, section, ext, name));
    return;
  }
  
  if (dynamic_cast<Gtk::TextView *> (w)) {
    m_widgets.push_back(new PrinterText(w, &m_dflt, &m_printer, section, ext, name));
    return;
  }
  
  throw invalid_argument(_("Unknown widget type for ") + id);
}

void PrinterDlg::AddJson(Glib::RefPtr<Gtk::Builder> builder, string id, string section, string ext, string name) {
  Gtk::TextView *w;
  builder->get_widget(id, w);

  if (w == NULL)
    throw invalid_argument(_("Unknown textview ") + id);
  
  if (ext == "#global")
    m_cust_global.AddToMask(name);
  else
    m_cust_extruder.AddToMask(name);
  
  m_widgets.push_back(new PrinterJson(w, &m_dflt, &m_printer, section, ext, name));
}

void PrinterDlg::AddCheck(Glib::RefPtr<Gtk::Builder> builder, string id, string section, string ext, string name, size_t num) {
  Gtk::CheckButton *w;
  builder->get_widget(id, w);

  if (w == NULL)
    throw invalid_argument(_("Unknown checkbutton ") + id);

  if (ext == "#global")
    m_cust_global.AddToMask(name);
  else
    m_cust_extruder.AddToMask(name);
  
  PrinterCheck *pw = new PrinterCheck(w, &m_dflt, &m_printer, section, ext, name);

  for (size_t count = 0; count < num; count++)
    pw->AddWidget(m_widgets[m_widgets.size() + count - num]);

  m_widgets.push_back(pw);
}

void PrinterDlg::AddExt(Glib::RefPtr<Gtk::Builder> builder, string id, string section, string ext, string name) {
  Add(builder, id, section, ext, name);
  
  m_ext_widgets.push_back(m_widgets.back());
}

void PrinterDlg::AddEList(Glib::RefPtr<Gtk::Builder> builder, string id, string section, string ext, string name) {
  Add(builder, id, section, ext, name);
  
  m_elist_widgets.push_back(dynamic_cast<PrinterCombo *> (m_widgets.back()));  
}

void PrinterDlg::LoadAll(void) {
  for (size_t count = 0; count < m_widgets.size(); count++)
    m_widgets[count]->LoadValue();
  
  m_cust_global.SetValue("#global", PS_GetMember(PS_GetMember(m_printer, "overrides", NULL), "#global", NULL));

  BuildEList();
  BuildExtruderList();
}

void PrinterDlg::BuildEList(void) {
  vector<string> values;

  values.push_back("Model Specific");
  
  Psvi vi(PS_GetMember(m_printer, "overrides", NULL));
  
  while (vi.Next()) {
    if (strcmp(vi.Key(), "#global") == 0)
      continue;
    
    values.push_back(Settings::GetExtruderText() + " " + vi.Key());
  }
  
  for (size_t count = 0; count < m_elist_widgets.size(); count++)
    m_elist_widgets[count]->SetupCombo(values);
}

void PrinterDlg::BuildExtruderList(void) {
  Psvi vi(PS_GetMember(m_printer, "overrides", NULL));
  size_t count = 0;
  
  {
    Inhibitor inhibit(&m_inhibit_extruder);
    m_store->clear();
    
    while (vi.Next()) {
      if (strcmp(vi.Key(), "#global") == 0)
	continue;
      
      Gtk::TreeModel::Row row = *m_store->append();
      row[m_name_col] = Glib::ustring(vi.Key());
      count++;
    }
  }
  
  if (count > 0) {
    // Select last
    Gtk::TreePath path;
    
    path.push_back(count - 1);
    
    m_tree->get_selection()->select(path);
  }
}

void PrinterDlg::New(void) {
  size_t num = PS_ItemCount(PS_GetMember(m_printer, "overrides", NULL));
  if (num > 0)
    num--;
  string name = to_string(num);

  PS_AddMember(PS_GetMember(m_printer, "overrides", NULL), name.c_str(), PS_ParseJsonString("{\"material_diameter\":1.75,\"machine_nozzle_size\":0.4,\"machine_nozzle_offset_x\":0,\"machine_nozzle_offset_y\":0}"));
  PS_AddMember(PS_GetMember(m_printer, "repsnapper", NULL), name.c_str(), PS_ParseJsonString("{\"material\":\"PLA\"}"));

  BuildEList();
  BuildExtruderList();
}

void PrinterDlg::Remove(void) {
  size_t num = PS_ItemCount(PS_GetMember(m_printer, "overrides", NULL));
  
  if (num <= 2)
    return;
  
  string name = to_string(num - 2);
  PS_RemoveMember(PS_GetMember(m_printer, "overrides", NULL), name.c_str());
  PS_RemoveMember(PS_GetMember(m_printer, "repsnapper", NULL), name.c_str());

  BuildEList();
  BuildExtruderList();
}

void PrinterDlg::ExtruderSelected(void) {
  vector< Gtk::TreeModel::Path > paths = m_tree->get_selection()->get_selected_rows();
  if (paths.size() == 0)
    return;
  Gtk::TreeModel::Row row = *m_store->children()[paths.front().back()];
  Glib::ustring name = row[m_name_col];
  
  for (size_t count = 0; count < m_ext_widgets.size(); count++)
    m_ext_widgets[count]->ChangeExtruder(name);

  m_cust_extruder.SetValue(name.c_str(), PS_GetMember(PS_GetMember(m_printer, "overrides", NULL), name.c_str(), NULL));
}

void PrinterDlg::OK() {
  m_dlg->hide();
  m_settings->SetPrinter(m_printer);
}

void PrinterDlg::Cancel() {
  m_dlg->hide();
}

void PrinterDlg::Save() {
  string path = m_settings->GetConfigPath("printer.json");
  Psf file(path.c_str(), "w");
  Pso os(PS_NewFileOStream(file()));
  PS_WriteValuePretty(os(), m_printer);
}

void PrinterDlg::SaveAs() {
  Gtk::FileChooserDialog dlg(*m_dlg, _("Save Printer"), Gtk::FILE_CHOOSER_ACTION_SAVE);
  dlg.add_button(_("Cancel"), 0);
  dlg.add_button(_("Save"), 1);
  dlg.set_default_response(1);
  dlg.set_current_folder(m_folder);
  dlg.set_do_overwrite_confirmation(true);
  
  Filters::attach_filters(dlg, Filters::JSON);
  
  if (dlg.run() != 1)
    return;
  
  string filename = dlg.get_filename();  
  m_folder = dlg.get_current_folder();
  
  Psf outfile(filename.c_str(), "w");
  Pso os(PS_NewFileOStream(outfile()));
  PS_WriteValuePretty(os(), m_printer);
}

void PrinterDlg::Load() {
  Gtk::FileChooserDialog dlg(*m_dlg, _("Load Printer"), Gtk::FILE_CHOOSER_ACTION_OPEN);
  dlg.add_button(_("Cancel"), 0);
  dlg.add_button(_("Load"), 1);
  dlg.set_default_response(1);
  dlg.set_current_folder(m_folder);
  
  Filters::attach_filters(dlg, Filters::JSON);

  if (dlg.run() != 1)
    return;
  
  string filename = dlg.get_filename();  
  m_folder = dlg.get_current_folder();
  
  Psf file(filename.c_str(), "r");
  PS_FreeValue(m_printer);
  m_printer = PS_ParseJsonFile(file());
  LoadAll();
}

void PrinterDlg::Import() {
  Gtk::FileChooserDialog dlg(*m_dlg, _("Import Printer"), Gtk::FILE_CHOOSER_ACTION_OPEN);
  dlg.add_button(_("Cancel"), 0);
  dlg.add_button(_("Import"), 1);
  dlg.set_default_response(1);
  dlg.set_current_folder(m_import_folder);
  
  Filters::attach_filters(dlg, Filters::JSON);
  
  if (dlg.run() != 1)
    return;
  
  string filename = dlg.get_filename();  
  m_import_folder = dlg.get_current_folder();

  try {
    Psv ps(m_settings->load_printer(filename));
    Psv set(PS_GetDefaults(ps()));
    Psv psdflt(m_settings->load_printer(set.GetNames()));
    Psv dflt(PS_GetDefaults(psdflt()));
    PS_PruneSettings(set(), dflt());
    
    PS_FreeValue(m_printer);
    m_printer = PS_NewObject();
    PS_AddMember(m_printer, "overrides", set());
    set.Disown();
    ps_value_t *rep = PS_NewObject();
    ps_value_t *glo = PS_NewObject();
    PS_AddMember(m_printer, "repsnapper", rep);
    PS_AddMember(rep, "#global", glo);
    ps_value_t *name = PS_GetMember(PS_GetMember(ps(), "#global", NULL), "name", NULL);
    if (name == NULL)
      name = PS_GetMember(PS_GetMember(set(), "#global", NULL), "machine_name", NULL);
    if (name)
      PS_AddMember(glo, "name", PS_CopyValue(name));
  } catch (exception &e) {
    Gtk::MessageDialog dialog(_("Import Failed"), false,
			      Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
    dialog.set_transient_for(*m_dlg);
    dialog.run();
    return;
  }
  
  LoadAll();
}
