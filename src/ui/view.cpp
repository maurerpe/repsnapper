/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2011 Michael Meeks
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

#include "config.h"

#include "view.h"
#include "model.h"
#include "objtree.h"
#include "render.h"
#include "settings.h"
#include "prefs_dlg.h"
#include "progress.h"
#include "connectview.h"
#include "widgets.h"
#include "platform.h"

bool View::on_delete_event(GdkEventAny* event) {
  Gtk::Main::quit();
  return false;
}

void View::connect_button(const char *name, const sigc::slot<void> &slot) {
  Gtk::Widget *bw = NULL;
  m_builder->get_widget (name, bw);
  // try Button
  Gtk::Button *button  = dynamic_cast<Gtk::Button*>(bw);
  if (button)
    button->signal_clicked().connect (slot);
  else {
    // try ToolButton
    Gtk::ToolButton *button  = dynamic_cast<Gtk::ToolButton*>(bw);
    if (button)
      button->signal_clicked().connect (slot);
    else {
      cerr << "missing button " << name << endl;
    }
  }
}

void View::connect_activate(const char *name, const sigc::slot<void> &slot) {
  Gtk::MenuItem *item = NULL;
  m_builder->get_widget(name, item);
  if (item)
    item->signal_activate().connect(slot);
  else {
    cerr << "missing menu item " << name << endl;
  }
}

void View::connect_toggled(const char *name, const sigc::slot<void, Gtk::ToggleButton *> &slot) {
  Gtk::ToggleButton *button = NULL;
  m_builder->get_widget (name, button);
  if (button)
    button->signal_toggled().connect (sigc::bind(slot, button));
  else {
    cerr << "missing toggle button " << name << endl;
  }
}

void View::connect_tooltoggled(const char *name, const sigc::slot<void, Gtk::ToggleToolButton *> &slot) {
  Gtk::ToggleToolButton *button = NULL;
  m_builder->get_widget (name, button);
  if (button)
    button->signal_toggled().connect (sigc::bind(slot, button));
  else {
    cerr << "missing toggle button " << name << endl;
  }
}

void View::convert_to_gcode() {
  extruder_selected(); // be sure to get extruder settings from gui
  PrintInhibitor inhibitPrint(m_printer);
  if (m_printer->IsPrinting())
    {
      m_printer->error (_("Complete print before converting"),
		     _("Converting to GCode while printing will abort the print"));
      return;
    }
  m_model->ConvertToGCode();

  gcode_status();
}

void View::preview_file (Glib::RefPtr< Gio::File > file) {
  if (!m_model) return;
  m_model->preview_shapes.clear();
  if (!m_model->settings.get_boolean("Display","PreviewLoad")) return;
  if (!file)    return;
  //cerr << "view " <<file->get_path() << endl;
  m_model->preview_shapes  = m_model->ReadShapes(file,2000000);
  bool display_poly = m_model->settings.get_boolean("Display","DisplayPolygons");
  m_model->settings.set_boolean("Display","DisplayPolygons", true);
  if (m_model->preview_shapes.size()>0) {
    Vector3d pMax = Vector3d(G_MINDOUBLE, G_MINDOUBLE, G_MINDOUBLE);
    Vector3d pMin = Vector3d(G_MAXDOUBLE, G_MAXDOUBLE, G_MAXDOUBLE);
    double bedx = m_model->settings.get_double("Hardware", "Volume.X");
    double bedy = m_model->settings.get_double("Hardware", "Volume.Y");
    Vector3d trans = {bedx/2, bedy/2, 0};
    for (uint i = 0; i < m_model->preview_shapes.size(); i++) {
      m_model->preview_shapes[i]->move(trans);
      m_model->preview_shapes[i]->PlaceOnPlatform();
      Vector3d stlMin = m_model->preview_shapes[i]->Min;
      Vector3d stlMax = m_model->preview_shapes[i]->Max;
      for (uint k = 0; k < 3; k++) {
	pMin[k] = min(stlMin[k], pMin[k]);
	pMax[k] = max(stlMax[k], pMax[k]);
      }
    }
  }
  queue_draw();
  m_model->settings.set_boolean("Display","DisplayPolygons",display_poly);
}

void View::load_stl () {
  show_notebooktab("file_tab", "controlnotebook");
}

void View::toggle_fullscreen() {
  static bool is_fullscreen = false;
  if (is_fullscreen) {
    unfullscreen();
    is_fullscreen = false;
  } else {
    fullscreen();
    is_fullscreen = true;
  }
}

void View::do_load_stl() {
  m_model->preview_shapes.clear();

  vector< Glib::RefPtr < Gio::File > > files = m_filechooser->get_files();
  for (uint i= 0; i < files.size(); i++)
    m_model->Read(files[i]);

  m_filechooser->set_path(m_model->settings.STLPath);
  show_notebooktab("model_tab", "controlnotebook");
}

bool View::get_userconfirm(string maintext, string secondarytext) const {
  Gtk::MessageDialog *dialog = new Gtk::MessageDialog(maintext);
  if (secondarytext != "")
    dialog->set_secondary_text (secondarytext);
  dialog->add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
  int response = dialog->run();
  bool result = false;
  if (response == Gtk::RESPONSE_OK)
    result = true;
  delete dialog;
  return result;
}

void View::save_stl() {
  Gtk::FileChooserDialog dlg(*this, "Save Model", Gtk::FILE_CHOOSER_ACTION_SAVE);
  dlg.add_button("Cancel", 0);
  dlg.add_button("Save", 1);
  dlg.set_default_response(1);
  dlg.set_current_folder(m_folder);
  
  Filters::attach_filters(dlg, Filters::MODEL);

  if (dlg.run() != 1)
    return;
  
  m_folder = dlg.get_current_folder();
  
  vector< Glib::RefPtr < Gio::File > > files = dlg.get_files();
  if (files.size()>0) {
    if (files[0]->query_exists())
      if (!get_userconfirm(_("Overwrite File?"), files[0]->get_basename()))
	return;

    m_model->SaveStl(files[0]);
  }
}

void View::gcode_status() {
  if (!m_model->gcode.empty()) {
    stringstream ostr;
    double time = m_model->gcode.GetTimeEstimation();
    int hr = floor(time / 3600);
    int min = fmod(floor(time / 60), 60);
    double sec = fmod(time, 60);
    ostr << "GCode: " << hr << "h " << min << "m " << sec << "s & " << (m_model->gcode.GetTotalExtruded() / 1000) << "m";
    statusBarMessage(ostr.str());
  }
}

void View::load_gcode() {
  if (m_printer->IsPrinting())
  {
    m_printer->error (_("Complete print before reading"),
  		   _("Reading GCode while printing will abort the print"));
    return;
  }
  PrintInhibitor inhibitPrint(m_printer);
  
  Gtk::FileChooserDialog dlg(*this, "Load GCode", Gtk::FILE_CHOOSER_ACTION_OPEN);
  dlg.add_button("Cancel", 0);
  dlg.add_button("Save", 1);
  dlg.set_default_response(1);
  dlg.set_current_folder(m_folder);
  
  Filters::attach_filters(dlg, Filters::GCODE);
  
  if (dlg.run() != 1)
    return;
  
  m_folder = dlg.get_current_folder();
  
  vector< Glib::RefPtr < Gio::File > > files = dlg.get_files();
  for (uint i= 0; i < files.size(); i++)
    m_model->Read(files[i]);
  
  gcode_status();
}

void View::save_gcode() {
  Gtk::FileChooserDialog dlg(*this, "Save GCode", Gtk::FILE_CHOOSER_ACTION_SAVE);
  dlg.add_button("Cancel", 0);
  dlg.add_button("Save", 1);
  dlg.set_default_response(1);
  dlg.set_current_folder(m_folder);
  
  Filters::attach_filters(dlg, Filters::GCODE);
  
  if (dlg.run() != 1)
    return;
  
  m_folder = dlg.get_current_folder();
  
  vector< Glib::RefPtr < Gio::File > > files = dlg.get_files();
  if (files.size()>0) {
    if (files[0]->query_exists())
      if (!get_userconfirm(_("Overwrite File?"), files[0]->get_basename()))
	return;
    
    m_model->WriteGCode(files[0]);
  }
}

void View::send_gcode () {
  m_printer->SendAsync(m_gcode_entry->get_text());
  m_gcode_entry->select_region(0,-1);
}

View *View::create(Model *model) {
  vector<string> dirs = Platform::getConfigPaths();
  Glib::ustring ui;
  for (vector<string>::const_iterator i = dirs.begin();
       i != dirs.end(); ++i) {
    string f_name = Glib::build_filename (*i, "repsnapper.ui");
    Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(f_name);
    try {
      char *ptr;
      gsize length;
      file->load_contents(ptr, length);
      ui = Glib::ustring(ptr);
      g_free(ptr);
      break;
    } catch(Gio::Error &e) {
      switch(e.code()) {
      case Gio::Error::NOT_FOUND:
        continue;

      default:
        Gtk::MessageDialog dialog(_("Error reading UI description!!"), false,
                                  Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
        dialog.set_secondary_text(e.what());
        dialog.run();
        return NULL;
      }
    }
  }

  if(ui.empty()) {
    Gtk::MessageDialog dialog(_("Couldn't find UI description!"), false,
                              Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
    dialog.set_secondary_text (_("Check that repsnapper has been correctly installed."));
    dialog.run();
    return NULL;
  }

  Glib::RefPtr<Gtk::Builder> builder;
  try {
    builder = Gtk::Builder::create_from_string(ui);
  }
  catch(const Gtk::BuilderError& ex)
  {
    Gtk::MessageDialog dialog(_("Error loading UI!"), false,
                              Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
    dialog.set_secondary_text(ex.what());
    dialog.run();
    throw ex;
  }

  View *view = 0;
  builder->get_widget_derived("main_window", view);
  view->setModel (model);

  return view;
}

void View::printing_changed() {
  bool printing = m_printer->IsPrinting();

  if (printing)
    m_progress->start(_("Printing"), 100.0);
  else
    m_progress->stop(_("Done"));

  m_model->SetIsPrinting(printing);

  if (printing)
    m_pause_button->set_active( false );
}

void View::enable_logging_toggled(Gtk::ToggleButton *button) {
  m_model->settings.set_boolean("Printer","Logging", button->get_active());
}

void View::temp_monitor_enabled_toggled(Gtk::ToggleButton *button) {
  m_model->settings.set_boolean("Misc","TempReadingEnabled", button->get_active());
  m_printer->UpdateTemperatureMonitor();
}

void View::fan_enabled_toggled(Gtk::ToggleButton *button) {
  if (toggle_block)
    return;
  
  if (!button->get_active()) {
    if (!m_printer->SendAsync("M107")) {
      toggle_block = true;
      button->set_active(true);
      toggle_block = false;
    }
  } else {
    stringstream oss;
    oss << "M106 S" << (int)m_fan_voltage->get_value();
    if (!m_printer->SendAsync(oss.str())) {
      toggle_block = true;
      button->set_active(false);
      toggle_block = false;
    }
  }
}

void View::run_extruder() {
  double amount = m_extruder_length->get_value();
  m_printer->RunExtruder(m_extruder_speed->get_value() * 60,
			 amount,
			 false,
			 m_extruder_row->get_selected());
}

void View::clear_logs() {
  log_view ->get_buffer()->set_text("");
  echo_view->get_buffer()->set_text("");
  err_view ->get_buffer()->set_text("");
  m_model->ClearLogs();
}

// open dialog to edit user gcode button
void View::edit_custombutton(string name, string code, Gtk::ToolButton *button) {
  Gtk::Dialog *dialog;
  m_builder->get_widget("custom_button_dialog", dialog);
  Gtk::Entry *nameentry;
  m_builder->get_widget("custom_name", nameentry);
  nameentry->set_text(name);
  //if (name!="") nameentry->set_sensitive(false);
  Gtk::TextView *tview;
  m_builder->get_widget ("custom_gcode", tview);
  tview->get_buffer()->set_text(code);
  dialog->set_transient_for(*this);
  dialog->show();
  // send result:
  dialog->signal_response().connect(sigc::bind(sigc::mem_fun(*this, &View::hide_custombutton_dlg), dialog));
}

void View::hide_custombutton_dlg(int code, Gtk::Dialog *dialog) {
  Gtk::Entry *nameentry;
  m_builder->get_widget("custom_name", nameentry);
  string name = nameentry->get_text();
  Gtk::TextView *tview;
  m_builder->get_widget("custom_gcode", tview);
  string gcode = tview->get_buffer()->get_text();
  if (code==1) {  // OK clicked
    add_custombutton(name, gcode);
    // save in settings:
    m_model->settings.set_user_button(name, gcode);
  }
  dialog->hide();
}

void View::add_custombutton(string name, string gcode) {
  Gtk::Toolbar *toolbar;
  m_builder->get_widget ("i_custom_toolbar", toolbar);
  if (toolbar) {
    //cerr << toolbar->get_n_items() << " items" << endl;
    Gtk::ToolButton *button = new Gtk::ToolButton(name);
    button->set_is_important(true);
    toolbar->append(*button,
		    sigc::bind(sigc::mem_fun(*this,
					     &View::custombutton_pressed), name, button));
    button->set_tooltip_text(gcode);
    button->set_sensitive(true);
    toolbar->set_sensitive(true);
    toolbar->show_all();
  } else cerr << "no Toolbar for button!" << endl;
}

void View::custombutton_pressed(string name, Gtk::ToolButton *button) {
  Gtk::ToggleButton *rembutton;
  m_builder->get_widget("i_remove_custombutton", rembutton);
  Gtk::ToggleButton *editbutton;
  m_builder->get_widget("i_edit_custombutton", editbutton);
  Gtk::Toolbar *toolbar;
  m_builder->get_widget("i_custom_toolbar", toolbar);
  
  if (!toolbar)
    return;
  
  if (rembutton->get_active()) {
    rembutton->set_active(false);
    if (m_model->settings.del_user_button(name)) {
      toolbar->remove(*button);
    }
  } else if (editbutton->get_active()) {
    editbutton->set_active(false);
    edit_custombutton(name, m_model->settings.get_user_gcode(name), button);
  } else {
    m_printer->SendAsync(m_model->settings.get_user_gcode(name));
  }
}


void View::log_msg(Gtk::TextView *tview, string s) {
  //Glib::Mutex::Lock lock(mutex);
  if (!tview || s.length() == 0)
    return;
  
  if (!m_model || !m_model->settings.get_boolean("Printer","Logging"))
    return;

  Glib::RefPtr<Gtk::TextBuffer> c_buffer = tview->get_buffer();
  Gtk::TextBuffer::iterator tend = c_buffer->end();
  c_buffer->insert (tend, s);
  tend = c_buffer->end();
  tview->scroll_to(tend);
}

void View::err_log(string s) {
  log_msg(err_view,s);
}

void View::comm_log(string s) {
  log_msg(log_view,s);
}

void View::echo_log(string s) {
  log_msg(echo_view,s);
}

void View::set_logging(bool logging) {
}

bool View::logprint_timeout_cb() {
  return true;
}

void View::hide_on_response(int, Gtk::Dialog *dialog) {
  dialog->hide();
}

void View::set_icon_file(Glib::RefPtr<Gio::File> file) {
  iconfile = file;
  if (iconfile) {
    set_icon_from_file(iconfile->get_path());
    m_settings_ui->set_icon_from_file(iconfile->get_path());
  } else {
    set_icon_name("gtk-convert");
  }
}


void View::show_dialog(const char *name)
{
  Gtk::Dialog *dialog;
  m_builder->get_widget (name, dialog);
  if (!dialog) {
    cerr << "no such dialog " << name << endl;
    return;
  }
  if (iconfile)
    dialog->set_icon_from_file(iconfile->get_path());
  else
    dialog->set_icon_name("gtk-convert");
  dialog->signal_response().connect(sigc::bind(sigc::mem_fun(*this, &View::hide_on_response), dialog));
  dialog->set_transient_for(*this);
  dialog->show();
}

void View::show_preferences() {
  m_settings_ui->show(*this);
}

void View::about_dialog() {
  show_dialog("about_dialog");
}

void View::load_settings() {
  Gtk::FileChooserDialog dlg(*this, "Load Settings", Gtk::FILE_CHOOSER_ACTION_OPEN);
  dlg.add_button("Cancel", 0);
  dlg.add_button("Save", 1);
  dlg.set_default_response(1);
  dlg.set_current_folder(m_folder);
    
  Filters::attach_filters(dlg, Filters::SETTINGS);
  
  if (dlg.run() != 1)
    return;
  
  m_folder = dlg.get_current_folder();
  
  vector< Glib::RefPtr < Gio::File > > files = dlg.get_files();
  for (uint i= 0; i < files.size(); i++)
    m_model->LoadConfig(files[i]);
}

// save to standard config file
void View::save_settings() {
  vector<string> user_config_bits(3);
  user_config_bits[0] = Glib::get_user_config_dir();
  user_config_bits[1] = "repsnapper";
  user_config_bits[2] = "repsnapper3.conf";

  string user_config_file = Glib::build_filename(user_config_bits);
  Glib::RefPtr<Gio::File> conffile = Gio::File::create_for_path(user_config_file);

  save_settings_to(conffile);
}

// gets config file from user
void View::save_settings_as() {
  Gtk::FileChooserDialog dlg(*this, "Save Settings", Gtk::FILE_CHOOSER_ACTION_SAVE);
  dlg.add_button("Cancel", 0);
  dlg.add_button("Save", 1);
  dlg.set_default_response(1);
  dlg.set_current_folder(m_folder);
    
  Filters::attach_filters(dlg, Filters::SETTINGS);
  
  if (dlg.run() != 1)
    return;
  
  m_folder = dlg.get_current_folder();
  
  vector< Glib::RefPtr < Gio::File > > files = dlg.get_files();
  if (files.size()>0) {
    if (files[0]->query_exists())
      if (!get_userconfirm(_("Overwrite File?"), files[0]->get_basename()))
	return;

    save_settings_to(files[0]);
  }
}

// save to given config file
void View::save_settings_to(Glib::RefPtr < Gio::File > file) {
  m_model->settings.SettingsPath = file->get_parent()->get_path();
  saveWindowSizeAndPosition(m_model->settings);
  m_model->SaveConfig(file);
}

void View::inhibit_print_changed() {
  if (m_printer->IsInhibited()) {
    if (!m_printer->IsPrinting())
      m_pause_button->set_sensitive (false);
    m_print_button->set_sensitive (false);
  } else {
    m_pause_button->set_sensitive (true);
    m_print_button->set_sensitive (true);
  }
}

void View::alert (Gtk::MessageType t, const char *message,
		  const char *secondary) {
  Gtk::MessageDialog dialog (*this, message, false /* markup */,
			     t, Gtk::BUTTONS_CLOSE, true);
  if (secondary)
    dialog.set_secondary_text (secondary);
  dialog.run();
}

void View::rot_object_from_spinbutton() {
  if (toggle_block)
    return;
  
  Gtk::SpinButton *spB;
  m_builder->get_widget("rot_x", spB);
  const double xangle = spB->get_value()*M_PI/180.;
  m_builder->get_widget("rot_y", spB);
  const double yangle = spB->get_value()*M_PI/180.;
  m_builder->get_widget("rot_z", spB);
  const double zangle = spB->get_value()*M_PI/180.;
  vector<Shape*> shapes;
  vector<TreeObject*> objects;
  get_selected_objects (objects, shapes);
  if (shapes.size()>0)
    for (uint i=0; i<shapes.size(); i++) {
      shapes[i]->transform3D.rotate_to(shapes[i]->Center, xangle, yangle, zangle);
    }
  else if (objects.size()>0)
    for (uint i=0; i<objects.size(); i++) {
      objects[i]->transform3D.rotate_to(objects[i]->center(), xangle, yangle, zangle);
    }
  update_scale_value();
  m_model->ModelChanged();
  queue_draw();
}

bool View::rotate_selection(Vector3d axis, double angle) {
  vector<Shape*> shapes;
  vector<TreeObject*> objects;
  get_selected_objects (objects, shapes);
  if (objects.size()>0)
    for (uint o=0; o<objects.size(); o++) {
      Transform3D &transf = objects[o]->transform3D;
      transf.rotate(objects[o]->center(),axis, angle);
    }
  else if (shapes.size()>0) {
    Vector3d center = {0, 0, 0};
    for (uint i=0; i<shapes.size() ; i++)
      center += shapes[i]->Center;
    center /= shapes.size();
    for (uint i=0; i<shapes.size() ; i++)
      shapes[i]->Rotate(center, axis, angle);
  }
  update_scale_value();
  update_rot_value();
  return true;
}

void View::update_rot_value() {
  toggle_block = true;
  vector<Shape*> shapes;
  vector<TreeObject*> objects;
  get_selected_objects (objects, shapes);
  Transform3D *trans = NULL;
  if (objects.size()>0)  {
    trans = &objects.back()->transform3D;
  }
  else if (shapes.size()>0)  {
    trans = &shapes.back()->transform3D;
  }
  if (trans != NULL) {
    Gtk::SpinButton *rot_sb;
    m_builder->get_widget("rot_x", rot_sb);
    rot_sb->set_value(trans->getRotX()*180/M_PI);
    m_builder->get_widget("rot_y", rot_sb);
    rot_sb->set_value(trans->getRotY()*180/M_PI);
    m_builder->get_widget("rot_z", rot_sb);
    rot_sb->set_value(trans->getRotZ()*180/M_PI);
  }
  toggle_block = false;
}

void View::placeonplatform_selection () {
  vector<Shape*> shapes;
  vector<TreeObject*> objects;
  get_selected_objects (objects, shapes);
  if (shapes.size()>0)
    for (uint i=0; i<shapes.size() ; i++)
      m_model->PlaceOnPlatform(shapes[i], NULL);
  else
    for (uint i=0; i<objects.size() ; i++)
      m_model->PlaceOnPlatform(NULL, objects[i]);

  queue_draw();
}

void View::mirror_selection () {
  vector<Shape*> shapes;
  vector<TreeObject*> objects;
  get_selected_objects (objects, shapes);
  if (shapes.size()>0)
    for (uint i=0; i<shapes.size() ; i++)
      m_model->Mirror(shapes[i], NULL);
  else
    for (uint i=0; i<objects.size() ; i++)
      m_model->Mirror(NULL, objects[i]);

  queue_draw();
}

void View::stl_added (Gtk::TreePath &path) {
  m_treeview->expand_all();
  m_treeview->get_selection()->unselect_all();
  m_treeview->get_selection()->select (path);
}

void View::set_SliderBBox(double max_z) {
  double smax = max(0.001, max_z);
  Gtk::Scale *scale;
  m_builder->get_widget("Display.PolygonOpacity", scale);
  if (scale)
    scale->set_range (0, 1);
  m_builder->get_widget ("Display.GCodeLayer", scale);
  if (scale)
    scale->set_range(0, smax);
}

void View::model_changed () {
  m_translation_row->selection_changed();
  set_SliderBBox(m_model->Max.z());
  show_notebooktab("model_tab", "controlnotebook");
  queue_draw();
}

void View::show_widget (string name, bool visible) const {
  Gtk::Widget *w;
  m_builder->get_widget (name, w);
  if (w)
    if (visible) w->show();
    else w->hide();
  else cerr << "no '" << name << "' in GUI" << endl;
}


void View::show_notebooktab (string name, string notebookname) const {
  Gtk::Notebook *nb;
  m_builder->get_widget (notebookname, nb);
  if (!nb) { cerr << "no notebook " << notebookname << endl; return;}
  Gtk::Widget *w;
  m_builder->get_widget (name, w);
  if (!w)  { cerr << "no widget " << name << endl; return;}
  int num = nb->page_num(*w);
  if (num >= 0)
    nb->set_current_page(num);
  else cerr << "no page " << num << endl;
}

void View::gcode_changed () {
  set_SliderBBox(m_model->gcode.max_z());
  // show gcode result
  show_notebooktab("gcode_tab", "controlnotebook");
}

void View::power_toggled(Gtk::ToggleToolButton *button) {
  m_printer->SwitchPower (button->get_active());
}

void View::print_clicked() {
  m_printer->StartPrinting();
}

void View::pause_toggled(Gtk::ToggleToolButton *button) {
  if (button->get_active())
    m_printer->StopPrinting();
  else
    m_printer->ContinuePrinting();
}

void View::reset_clicked() {
  if (get_userconfirm(_("Reset Printer?"))) {
    m_printer->Reset();
  }
}

void View::home_all() {
  if (m_printer->IsPrinting()) return;
  m_printer->SendAsync ("G28");
  for (uint i = 0; i < 3; i++)
    m_axis_rows[i]->notify_homed();
}


void View::update_settings_gui() {
  // awful cast-ness to avoid having glibmm headers everywhere.
  m_model->settings.set_to_gui(m_builder);

  Gtk::AboutDialog *about;
  m_builder->get_widget("about_dialog", about);
  about->set_version(VERSION);

  Gtk::Toolbar *toolbar;
  m_builder->get_widget("i_custom_toolbar", toolbar);
  if (toolbar) {
    vector<Gtk::Widget*> buts = toolbar->get_children();
    for (guint i=buts.size(); i>0; i--) {
      toolbar->remove(*buts[i-1]);
    }
    vector<string> buttonlabels =
      m_model->settings.get_string_list("UserButtons","Labels");
    for (guint i=0; i< buttonlabels.size(); i++) {
      add_custombutton(buttonlabels[i],
		       m_model->settings.get_user_gcode(buttonlabels[i]));
    }
  }
  update_extruderlist();
}

void View::handle_ui_settings_changed() {
  queue_draw();
}


void View::temp_changed() {
  for (int i = 0; i < TEMP_LAST; i++)
    m_temps[i]->update_temp (m_printer->get_temp((TempType) i));
}

bool View::move_selection(float x, float y, float z) {
  vector<Shape*> shapes;
  vector<TreeObject*> objects;
  get_selected_objects (objects, shapes);
  if (shapes.size()>0) {
    for (uint s=0; s<shapes.size(); s++) {
      shapes[s]->move(Vector3d(x,y,z));
    }
  } else if (objects.size()>0) {
    for (uint o=0; o<objects.size(); o++) {
      objects[o]->transform3D.move(Vector3d(x,y,z));
    }
  }
  return true;
}

bool View::key_pressed_event(GdkEventKey *event) {
  //  cerr << "key " << event->keyval << endl;
  // if (m_treeview->get_selection()->count_selected_rows() <= 0)
  //   return false;
  switch (event->keyval) {
  case GDK_KEY_Tab:
    if (event->state & GDK_CONTROL_MASK) {
      Gtk::Notebook *nb;
      m_builder->get_widget ("controlnotebook", nb);
      if (nb) {
	if (event->state & GDK_SHIFT_MASK)
	  nb->prev_page();
	else
	  nb->next_page();
      }
      return true;
    }
    break;
  case GDK_KEY_Escape:
    stop_progress();
    return true;
    
  case GDK_KEY_Delete:
  case GDK_KEY_KP_Delete:
    delete_selected_objects();
    return true;
    
  default:
    return false;
  }
  return false;
}

View::View(BaseObjectType* cobject,
	   const Glib::RefPtr<Gtk::Builder>& builder)
  : Gtk::ApplicationWindow(cobject),
  m_builder(builder), m_model(NULL), m_renderer(NULL) {
  toggle_block = false;

  // Menus
  connect_activate("file.loadstl",         sigc::mem_fun(*this, &View::load_stl) );
  connect_activate("file.loadgcode",       sigc::mem_fun(*this, &View::load_gcode) );
  connect_activate("file.loadsettings",    sigc::mem_fun(*this, &View::load_settings));
  connect_activate("file.savesettings",    sigc::mem_fun(*this, &View::save_settings));
  connect_activate("file.savesettingsas",  sigc::mem_fun(*this, &View::save_settings_as));
  connect_activate("file.quit",            sigc::ptr_fun(&Gtk::Main::quit));
   
  connect_activate("edit.fullscreen",      sigc::mem_fun(*this, &View::toggle_fullscreen) );
  connect_activate("edit.preferences",     sigc::mem_fun(*this, &View::show_preferences) );
  
  connect_activate("help.about",           sigc::mem_fun(*this, &View::about_dialog));
  Gtk::Widget *w;
  m_builder->get_widget ("help.about", w);
  if (w)
    w->set_sensitive(true);

  // View tab
  connect_button ("m_save_stl",      sigc::mem_fun(*this, &View::save_stl) );
  connect_button ("m_delete",        sigc::mem_fun(*this, &View::delete_selected_objects) );
  connect_button ("m_duplicate",     sigc::mem_fun(*this, &View::duplicate_selected_objects) );
  connect_button ("m_split",         sigc::mem_fun(*this, &View::split_selected_objects) );
  connect_button ("m_merge",         sigc::mem_fun(*this, &View::merge_selected_objects) );
  connect_button ("m_divide",        sigc::mem_fun(*this, &View::divide_selected_objects) );
  connect_button ("m_platform",      sigc::mem_fun(*this, &View::placeonplatform_selection));
  connect_button ("m_mirror",        sigc::mem_fun(*this, &View::mirror_selection));

  connect_button ("progress_stop",   sigc::mem_fun(*this, &View::stop_progress));

  connect_button ("copy_extruder",   sigc::mem_fun(*this, &View::copy_extruder));
  connect_button ("remove_extruder",   sigc::mem_fun(*this, &View::remove_extruder));

  m_builder->get_widget ("m_treeview", m_treeview);
  // Insert our keybindings all around the place
  signal_key_press_event().connect (sigc::mem_fun(*this, &View::key_pressed_event) );
  m_treeview->signal_key_press_event().connect (sigc::mem_fun(*this, &View::key_pressed_event) );
  // m_treeview->set_reorderable(true); // this is too simple
  m_treeview->get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);
  m_translation_row = new TranslationSpinRow (this, m_treeview);

  Gtk::SpinButton *scale_value;
  m_builder->get_widget("m_scale_value", scale_value);
  scale_value->set_range(0.01, 10.0);
  scale_value->set_value(1.0);
  m_treeview->get_selection()->signal_changed().connect
    (sigc::mem_fun(*this, &View::tree_selection_changed));
  scale_value->signal_value_changed().connect
    (sigc::mem_fun(*this, &View::scale_selection));
  m_builder->get_widget("scale_x", scale_value);
  scale_value->set_range(0.01, 10.0);
  scale_value->set_value(1.0);
  scale_value->signal_value_changed().connect
    (sigc::mem_fun(*this, &View::scale_object_x));
  m_builder->get_widget("scale_y", scale_value);
  scale_value->set_range(0.01, 10.0);
  scale_value->set_value(1.0);
  scale_value->signal_value_changed().connect
    (sigc::mem_fun(*this, &View::scale_object_y));
  m_builder->get_widget("scale_z", scale_value);
  scale_value->set_range(0.01, 10.0);
  scale_value->set_value(1.0);
  scale_value->signal_value_changed().connect
    (sigc::mem_fun(*this, &View::scale_object_z));

  Gtk::SpinButton *rot_value;
  m_builder->get_widget("rot_x", rot_value);
  rot_value->set_range(0.00, 360.0);
  rot_value->set_value(0);
  rot_value->signal_value_changed().connect
    (sigc::mem_fun(*this, &View::rot_object_from_spinbutton));
  m_builder->get_widget("rot_y", rot_value);
  rot_value->set_range(0.00, 360.0);
  rot_value->set_value(0);
  rot_value->signal_value_changed().connect
    (sigc::mem_fun(*this, &View::rot_object_from_spinbutton));
  m_builder->get_widget("rot_z", rot_value);
  rot_value->set_range(0.00, 360.0);
  rot_value->set_value(0);
  rot_value->signal_value_changed().connect
    (sigc::mem_fun(*this, &View::rot_object_from_spinbutton));

  // GCode tab
  m_builder->get_widget ("g_gcode", m_gcode_entry);
  m_gcode_entry->set_activates_default();
  m_gcode_entry->signal_activate().connect (sigc::mem_fun(*this, &View::send_gcode));;

  connect_button ("g_load_gcode",    sigc::mem_fun(*this, &View::load_gcode) );
  connect_button ("g_convert_gcode", sigc::mem_fun(*this, &View::convert_to_gcode) );
  connect_button ("g_save_gcode",    sigc::mem_fun(*this, &View::save_gcode) );
  connect_button ("g_send_gcode",    sigc::mem_fun(*this, &View::send_gcode) );

  // Print tab
  m_builder->get_widget ("p_print", m_print_button);
  connect_button ("p_print",         sigc::mem_fun(*this, &View::print_clicked) );
  m_builder->get_widget ("p_pause", m_pause_button);
  connect_tooltoggled("p_pause",     sigc::mem_fun(*this, &View::pause_toggled));
  connect_button ("p_home",          sigc::mem_fun(*this, &View::home_all));
  connect_button ("p_reset",         sigc::mem_fun(*this, &View::reset_clicked));
  connect_tooltoggled("p_power",     sigc::mem_fun(*this, &View::power_toggled) );

  // Interactive tab
  connect_toggled ("Printer.Logging", sigc::mem_fun(*this, &View::enable_logging_toggled));
  connect_button ("Printer.ClearLog",      sigc::mem_fun(*this, &View::clear_logs) );
  m_builder->get_widget ("Printer.ExtrudeSpeed", m_extruder_speed);
  m_builder->get_widget ("Printer.ExtrudeAmount", m_extruder_length);
  connect_toggled ("Misc.TempReadingEnabled", sigc::mem_fun(*this, &View::temp_monitor_enabled_toggled));
  connect_toggled ("i_fan_enabled", sigc::mem_fun(*this, &View::fan_enabled_toggled));
  m_builder->get_widget ("Printer.FanVoltage", m_fan_voltage);

  connect_button ("i_extrude_length", sigc::mem_fun(*this, &View::run_extruder) );

  connect_button ("i_new_custombutton", sigc::mem_fun(*this, &View::new_custombutton) );
  
  m_settings_ui = new PrefsDlg(m_builder);

  // file chooser
  m_filechooser = new RSFilechooser(this);
  // show_widget("save_buttons", false);

  // signal callback for notebook page-switch
  Gtk::Notebook *cnoteb;
  m_builder->get_widget ("controlnotebook", cnoteb);
  if (cnoteb)
    cnoteb->signal_switch_page().connect
      (sigc::mem_fun(*this, &View::on_controlnotebook_switch));
  else cerr << "no 'controlnotebook' in GUI" << endl;

  m_printer = NULL;

  m_builder->get_widget ("extruder_treeview", extruder_treeview);
  if (extruder_treeview) {
    extruder_treeview->signal_cursor_changed().connect (sigc::mem_fun(*this, &View::extruder_selected) );
    Gtk::TreeModel::ColumnRecord colrec;
    colrec.add(extrudername);
    extruder_liststore = Gtk::ListStore::create(colrec);
    extruder_treeview->set_model(extruder_liststore);
    extruder_treeview->set_headers_visible(false);
    extruder_treeview->append_column("Extruder",extrudername);
  }

  update_extruderlist();  
  show();
}

void View::extruder_selected() {
  vector< Gtk::TreeModel::Path > path =
    extruder_treeview->get_selection()->get_selected_rows();
  if(path.size()>0 && path[0].size()>0) {
    // copy selected extruder from Extruders to current Extruder
    m_model->settings.SelectExtruder(path[0][0], &m_builder);
  }
  queue_draw();
}

void View::copy_extruder() {
  if (!m_model)
    return;
  
  vector< Gtk::TreeModel::Path > path =
    extruder_treeview->get_selection()->get_selected_rows();
  if(path.size()>0 && path[0].size()>0) {
    m_model->settings.CopyExtruder(path[0][0]);
  }
  update_extruderlist();
  Gtk::TreeNodeChildren ch = extruder_treeview->get_model()->children();
  Gtk::TreeIter row = ch[ch.size()-1];
  extruder_treeview->get_selection()->select(row);
  extruder_selected();
}

void View::remove_extruder() {
  if (!m_model)
    return;
  
  vector< Gtk::TreeModel::Path > path =
    extruder_treeview->get_selection()->get_selected_rows();
  if (path.size()>0 && path[0].size()>0) {
    m_model->settings.RemoveExtruder(path[0][0]);
  }
  update_extruderlist();
}

void View::update_extruderlist() {
  if (!m_model || !extruder_treeview)
    return;
  
  extruder_liststore->clear();
  uint num = m_model->settings.getNumExtruders();
  if (num==0) return;
  Gtk::TreeModel::Row row;
  for (uint i = 0; i < num ; i++) {
    row = *(extruder_liststore->append());
    ostringstream o; o << "Extruder " << i+1;
    //cerr << o.str() << m_model->settings.Extruders[i].UseForSupport<< endl;
    row[extrudername] = o.str();
    //row[extrudername] = m_model->settings.Extruders[i].name;
  }
  Gtk::TreeModel::Row firstrow = extruder_treeview->get_model()->children()[0];
  extruder_treeview->get_selection()->select(firstrow);
  extruder_selected();
  m_extruder_row->set_number(num);
}

//  stop file preview when leaving file tab
void View::on_controlnotebook_switch(Gtk::Widget* page, guint page_num) {
  if (!page)
    return;
  
  if (m_model)
    m_model->preview_shapes.clear();
}

View::~View() {
  delete m_settings_ui;
  delete m_translation_row;
  for (uint i = 0; i < 3; i++) {
    delete m_axis_rows[i];
  }
  delete m_extruder_row;
  delete m_temps[TEMP_NOZZLE];
  delete m_temps[TEMP_BED];
  delete m_cnx_view;
  delete m_progress; m_progress = NULL;
  delete m_printer;
  delete m_gcodetextview;
  RSFilechooser *chooser = m_filechooser;
  m_filechooser = NULL;
  delete chooser;
  m_renderer = NULL;
}

bool View::saveWindowSizeAndPosition(Settings &settings) const {
  Gtk::Window *pWindow = NULL;
  m_builder->get_widget("main_window", pWindow);
  if (pWindow) {
    settings.set_integer("Misc","WindowWidth", pWindow->get_width());
    settings.set_integer("Misc","WindowHeight", pWindow->get_height());
    int x,y;
    pWindow->get_position(x,y);
    settings.set_integer("Misc","WindowPosX", x);
    settings.set_integer("Misc","WindowPosY", y);
    return true;
  }
  return false;
}

void View::setModel(Model *model) {
  m_model = model;

  m_model->settings.m_signal_visual_settings_changed.connect
    (sigc::mem_fun(*this, &View::handle_ui_settings_changed));
  m_model->settings.m_signal_update_settings_gui.connect
    (sigc::mem_fun(*this, &View::update_settings_gui));

  m_treeview->set_model(m_model->objtree.m_model);
  m_treeview->append_column_editable("Name", m_model->objtree.m_cols->m_name);

  // m_treeview->append_column_editable("Extruder", m_model->objtree.m_cols->m_material);
  //m_treeview->append_column("Extruder", m_model->objtree.m_cols->m_extruder);
  //  m_treeview->set_headers_visible(true);

  m_gcodetextview = NULL;
  m_builder->get_widget ("GCode.Result", m_gcodetextview);
  m_gcodetextview->set_buffer (m_model->GetGCodeBuffer());
  m_gcodetextview->get_buffer()->signal_mark_set().
    connect( sigc::mem_fun(this, &View::on_gcodebuffer_cursor_set) );


  // Main view progress bar
  Gtk::Box *box = NULL;
  Gtk::ProgressBar *bar = NULL;
  m_builder->get_widget("progress_box", box);
  m_builder->get_widget("progress_bar", bar);
  // Create ViewProgress widget and inform model about it
  m_progress = new ViewProgress(box, bar);
  m_model->SetViewProgress(m_progress);
  
  Gtk::Statusbar *sbar = NULL;
  m_builder->get_widget("statusbar", sbar);
  m_model->statusbar = sbar;

  m_builder->get_widget("i_txt_comms", log_view);
  log_view->set_buffer(Gtk::TextBuffer::create());
  log_view->set_reallocate_redraws(false);
  m_builder->get_widget("i_txt_errs", err_view);
  err_view->set_buffer(m_model->errlog);
  err_view->set_reallocate_redraws(false);
  m_builder->get_widget("i_txt_echo", echo_view);
  echo_view->set_buffer(m_model->echolog);
  echo_view->set_reallocate_redraws(false);

  m_printer = new Printer(this);
  m_printer->signal_temp_changed.connect
    (sigc::mem_fun(*this, &View::temp_changed));
  m_printer->signal_printing_changed.connect
    (sigc::mem_fun(*this, &View::printing_changed));
  m_printer->signal_now_printing.connect
    (sigc::mem_fun(*this, &View::showCurrentPrinting));
  // Connect / dis-connect button
  m_cnx_view = new ConnectView(m_printer, &m_model->settings);
  Gtk::Box *connect_box = NULL;
  m_builder->get_widget ("p_connect_button_box", connect_box);
  connect_box->add (*m_cnx_view);

  Gtk::Box *temp_box;
  m_builder->get_widget ("i_temp_box", temp_box);
  m_temps[TEMP_NOZZLE] = new TempRow(m_model, m_printer, TEMP_NOZZLE);
  m_temps[TEMP_BED] = new TempRow(m_model, m_printer, TEMP_BED);
  temp_box->add (*m_temps[TEMP_NOZZLE]);
  temp_box->add (*m_temps[TEMP_BED]);

  Gtk::Box *axis_box;
  m_builder->get_widget ("i_axis_controls", axis_box);
  for (uint i = 0; i < 3; i++) {
    m_axis_rows[i] = new AxisRow (m_model, m_printer, i);
    axis_box->add (*m_axis_rows[i]);
  }
  axis_box->show_all();
  
  Gtk::Box *extruder_box;
  m_builder->get_widget ("i_extruder_box", extruder_box);
  m_extruder_row = new ExtruderRow(m_printer);
  extruder_box->add(*m_extruder_row);

  inhibit_print_changed();
  m_printer->signal_inhibit_changed.
    connect (sigc::mem_fun(*this, &View::inhibit_print_changed));
  m_model->m_signal_stl_added.connect (sigc::mem_fun(*this, &View::stl_added));
  m_model->m_model_changed.connect (sigc::mem_fun(*this, &View::model_changed));
  m_model->m_signal_gcode_changed.connect (sigc::mem_fun(*this, &View::gcode_changed));
  m_model->signal_alert.connect (sigc::mem_fun(*this, &View::alert));
  m_printer->signal_alert.connect (sigc::mem_fun(*this, &View::alert));

  // connect settings
  m_model->settings.connect_to_ui(m_builder);

  m_printer->setModel(m_model);
  
  // 3D preview of the bed
  Gtk::Box *pBox = NULL;
  m_builder->get_widget("viewarea", pBox);
  if (!pBox)
    cerr << "missing box!";
  else {
    m_renderer = manage(new Render(this, m_treeview->get_selection()));
    pBox->add(*m_renderer);
    m_renderer->show_all();
  }

  m_renderer->queue_draw();
}

void View::on_gcodebuffer_cursor_set(const Gtk::TextIter &iter,
				     const Glib::RefPtr <Gtk::TextMark> &refMark) {
  if (m_renderer)
    m_renderer->queue_draw();
}

void View::delete_selected_objects() {
  vector<Gtk::TreeModel::Path> path = m_treeview->get_selection()->get_selected_rows();
  m_model->DeleteObjTree(path);
  m_treeview->expand_all();
}

void View::tree_selection_changed() {
  if (m_model) {
    m_model->m_current_selectionpath = m_treeview->get_selection()->get_selected_rows();
    vector<Shape*> shapes;
    vector<TreeObject*> objects;
    get_selected_objects (objects, shapes);
    if (shapes.size() > 0) {
      ostringstream ostr;
      ostr << shapes.back()->filename << ": " << shapes.back()->size() << " triangles ";
      statusBarMessage(ostr.str());
    }
    m_model->m_inhibit_modelchange = true;
    update_scale_value();
    update_rot_value();
    m_model->m_inhibit_modelchange = false;
  }
}

bool View::get_selected_objects(vector<TreeObject*> &objects, vector<Shape*> &shapes) {
  vector<Gtk::TreeModel::Path> iter = m_treeview->get_selection()->get_selected_rows();
  m_model->objtree.get_selected_objects(iter, objects, shapes);
  return objects.size() != 0 || shapes.size() != 0;
}

bool View::get_selected_shapes(vector<Shape*> &shapes, vector<Matrix4d> &transforms) {
  vector<Gtk::TreeModel::Path> iter = m_treeview->get_selection()->get_selected_rows();
  m_model->objtree.get_selected_shapes(iter, shapes, transforms);
  return shapes.size() != 0;
}

void View::duplicate_selected_objects() {
  vector<Shape*> shapes;
  vector<TreeObject*> objects;
  get_selected_objects (objects, shapes);
  if (shapes.size()>0)
    for (uint i=0; i<shapes.size() ; i++) {
      Shape * newshape;
      newshape = new Shape(*shapes[i]);
      // duplicate
      TreeObject* object = m_model->objtree.getParent(shapes[i]);
      if (object !=NULL)
	m_model->AddShape (object, newshape, shapes[i]->filename);
      queue_draw();
    }
}

void View::split_selected_objects() {
  vector<Shape*> shapes;
  vector<TreeObject*> objects;
  get_selected_objects (objects, shapes);
  if (shapes.size()>0) {
    for (uint i=0; i<shapes.size() ; i++) {
      TreeObject* object = m_model->objtree.getParent(shapes[i]);
      if (object !=NULL)
	if (m_model->SplitShape (object, shapes[i], shapes[i]->filename) > 1) {
	// delete shape?
      }
    }
    queue_draw();
  }
}

void View::merge_selected_objects() {
  vector<Shape*> shapes;
  vector<TreeObject*> objects;
  get_selected_objects (objects, shapes);
  if (shapes.size()>0) {
    TreeObject* parent = m_model->objtree.getParent(shapes[0]);
    m_model->MergeShapes(parent, shapes);
    queue_draw();
  }
}

void View::divide_selected_objects() {
}

/* Handler for widget rollover. Displays a message in the window status bar */
bool View::statusBarMessage(Glib::ustring message) {
  Gtk::Statusbar *statusbar;
  m_builder->get_widget("statusbar", statusbar);
  statusbar->push(message);
  
  return false;
}


void View::stop_progress() {
  m_progress->stop_running();
}


void View::scale_selection() {
  if (toggle_block) return;
  double scale=1;
  Gtk::SpinButton *scale_value;
  m_builder->get_widget("m_scale_value", scale_value);
  scale = scale_value->get_value();
  scale_selection_to(scale);
}

void View::scale_selection_to(const double factor) {
  vector<Shape*> shapes;
  vector<TreeObject*> objects;
  get_selected_objects (objects, shapes);
  if (shapes.size()>0)
    for (uint i=0; i<shapes.size() ; i++)
      shapes[i]->transform3D.scale(factor);
  else if (objects.size()>0)
    for (uint i=0; i<objects.size() ; i++)
      objects[i]->transform3D.scale(factor);
  
  m_model->ModelChanged();
}

void View::scale_object_x() {
  if (toggle_block)
    return;
  
  double scale=1;
  Gtk::SpinButton *scale_value;
  m_builder->get_widget("scale_x", scale_value);
  scale = scale_value->get_value();
  vector<Shape*> shapes;
  vector<TreeObject*> objects;
  get_selected_objects (objects, shapes);
  if (shapes.size()>0)
    for (uint i=0; i<shapes.size() ; i++)
      shapes[i]->transform3D.scale_x(scale);
  else if (objects.size()>0)
    for (uint i=0; i<objects.size() ; i++)
      objects[i]->transform3D.scale_x(scale);
  
  m_model->ModelChanged();
}

void View::scale_object_y() {
  if (toggle_block)
    return;
  
  double scale=1;
  Gtk::SpinButton *scale_value;
  m_builder->get_widget("scale_y", scale_value);
  scale = scale_value->get_value();
  vector<Shape*> shapes;
  vector<TreeObject*> objects;
  get_selected_objects (objects, shapes);
  if (shapes.size()>0)
    for (uint i=0; i<shapes.size() ; i++)
      shapes[i]->transform3D.scale_y(scale);
  else if (objects.size()>0)
    for (uint i=0; i<objects.size() ; i++)
      objects[i]->transform3D.scale_y(scale);
  
  m_model->ModelChanged();
}

void View::scale_object_z() {
  if (toggle_block) return;
  double scale=1;
  Gtk::SpinButton *scale_value;
  m_builder->get_widget("scale_z", scale_value);
  scale = scale_value->get_value();
  vector<Shape*> shapes;
  vector<TreeObject*> objects;
  get_selected_objects (objects, shapes);
  if (shapes.size()>0)
    for (uint i=0; i<shapes.size() ; i++)
      shapes[i]->transform3D.scale_z(scale);
  else if (objects.size()>0)
    for (uint i=0; i<objects.size() ; i++)
      objects[i]->transform3D.scale_z(scale);
  
  m_model->ModelChanged();
}

/* Updates the scale value when a new STL is selected,
 * giving it the new STL's current scale factor */
void View::update_scale_value() {
  toggle_block = true;
  vector<Shape*> shapes;
  vector<TreeObject*> objects;
  get_selected_objects (objects, shapes);
  if (shapes.size()>0) {
    Gtk::SpinButton *scale_sb;
    m_builder->get_widget("m_scale_value", scale_sb);
    scale_sb->set_value(shapes.back()->getScaleFactor());
    m_builder->get_widget("scale_x", scale_sb);
    scale_sb->set_value(shapes.back()->getScaleFactorX());
    m_builder->get_widget("scale_y", scale_sb);
    scale_sb->set_value(shapes.back()->getScaleFactorY());
    m_builder->get_widget("scale_z", scale_sb);
    scale_sb->set_value(shapes.back()->getScaleFactorZ());
  } else if (objects.size()>0) {
    Gtk::SpinButton *scale_sb;
    m_builder->get_widget("m_scale_value", scale_sb);
    scale_sb->set_value(objects.back()->transform3D.get_scale());
    m_builder->get_widget("scale_x", scale_sb);
    scale_sb->set_value(objects.back()->transform3D.get_scale_x());
    m_builder->get_widget("scale_y", scale_sb);
    scale_sb->set_value(objects.back()->transform3D.get_scale_y());
    m_builder->get_widget("scale_z", scale_sb);
    scale_sb->set_value(objects.back()->transform3D.get_scale_z());
  }
  
  toggle_block = false;
}

void View::DrawGrid(void) {
  if (!m_renderer)
    return;
  
  Vector3d volume = m_model->settings.getPrintVolume();

  RenderVert vert;
  
  // Boarder lines
  // left edge
  vert.add(0.0f, 0.0f, 0.0f);
  vert.add(0.0f, volume.y(), 0.0f);
  // near edge
  vert.add(0.0f, 0.0f, 0.0f);
  vert.add(volume.x(), 0.0f, 0.0f);
  // right edge
  vert.add(volume.x(), 0.0f, 0.0f);
  vert.add(volume.x(), volume.y(), 0.0f);
  // far edge
  vert.add(0.0f, volume.y(), 0.0f);
  vert.add(volume.x(), volume.y(), 0.0f);
  // left edge
  vert.add(0.0f, 0.0f, volume.z());
  vert.add(0.0f, volume.y(), volume.z());
  // near edge
  vert.add(0.0f, 0.0f, volume.z());
  vert.add(volume.x(), 0.0f, volume.z());
  // right edge
  vert.add(volume.x(), 0.0f, volume.z());
  vert.add(volume.x(), volume.y(), volume.z());
  // far edge
  vert.add(0.0f, volume.y(), volume.z());
  vert.add(volume.x(), volume.y(), volume.z());
	 
  // verticals at rear
  vert.add(0.0f, volume.y(), 0);
  vert.add(0.0f, volume.y(), volume.z());
  vert.add(volume.x(), volume.y(), 0);
  vert.add(volume.x(), volume.y(), volume.z());

  float color[4] = {0.5f, 0.5f, 0.5f, 1.0f}; // Gray
  m_renderer->draw_lines(color, vert, 2.0);

  // Draw thin internal lines
  vert.clear();
  for (uint x = 10; x < volume.x(); x += 10) {
    vert.add(x, 0.0f, 0.0f);
    vert.add(x, volume.y(), 0.0f);
  }

  for (uint y = 10; y < volume.y(); y += 10) {
    vert.add(0.0f, y, 0.0f);
    vert.add(volume.x(), y, 0.0f);
  }
  m_renderer->draw_lines(color, vert, 1.0);  
}

void AddRectangle(RenderVert &vert, double x1, double y1, double x2, double y2, double z) {
  vert.add(x1, y1, 0);
  vert.add(x1, y2, 0);
  vert.add(x2, y1, 0);
  
  vert.add(x1, y2, 0);
  vert.add(x2, y1, 0);
  vert.add(x2, y2, 0);
  
  vert.add(x1, y1, z);
  vert.add(x1, y2, z);
  vert.add(x2, y1, z);
  
  vert.add(x1, y2, z);
  vert.add(x2, y1, z);
  vert.add(x2, y2, z);
}

void View::DrawMargins(void) {
  Vector3d margin = m_model->settings.getPrintMargin();
  Vector3d volume = m_model->settings.getPrintVolume();
  
  // cout << "View::DrawMargins m: " << margin << " v: " << volume << endl;
  
  RenderVert vert;
  if (margin.x()) {
    AddRectangle(vert, 0, 0, margin.x(), volume.y(), volume.z());
    AddRectangle(vert, volume.x() - margin.x(), 0, volume.x(), volume.y(), volume.z());
  }
  
  if (margin.y()) {
    AddRectangle(vert, margin.x(), 0, volume.x() - margin.x(), margin.y(), volume.z());
    AddRectangle(vert, margin.x(), volume.y() - margin.y(), volume.x() - margin.x(), volume.y(), volume.z());
  }
  
  float color[4] = {0.3, 0, 0, 1};
  m_renderer->draw_triangles(color, vert);
}

void View::DrawGCode(void) {
  if (!m_renderer)
    return;
  
  // Draw GCode
  if (!m_model->isCalculating()) {
    if (m_gcodetextview->has_focus()) {
      size_t line = m_gcodetextview->get_buffer()->get_insert()->get_iter().get_line();
      m_model->GlDrawGCode(*m_renderer, m_model->gcode.getLayerNo(line));
    }
    else {
      m_model->GlDrawGCode(*m_renderer, m_model->settings.get_double("Display", "GCodeLayer"));
    }
  }
}

void View::DrawShapes(vector<Gtk::TreeModel::Path> selected) {
  if (!m_renderer)
    return;
  
  m_model->drawShapes(*m_renderer, selected);
}

void View::DrawBBoxes(void) {
  if (!m_renderer)
    return;
  
  m_model->drawBBoxes(*m_renderer);
}

void View::showCurrentPrinting(unsigned long lineno) {
  if (lineno == 0) {
    m_progress->stop(_("Done"));
    return;
  }
  bool cont = true;
  cont = m_progress->update(m_model->gcode.percentDone(lineno),
			    true,
			    m_model->gcode.timeLeft(lineno));
  if (!cont) { // stop by progress bar
    m_printer->Pause();
  }
  m_model->setCurrentPrintingLine(lineno);
  queue_draw();
}
