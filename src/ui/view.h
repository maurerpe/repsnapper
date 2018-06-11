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
#pragma once

#include <vector>
#include <gtkmm.h>

#include "stdafx.h"
#include "printer/printer.h"

#include "rsfilechooser.h"

static bool UNUSED toggle_block = false; // blocks signals for togglebuttons etc

class View : public Gtk::ApplicationWindow
{
  class TempRow;
  class AxisRow;
  class TranslationSpinRow;
  class ExtruderRow;

  friend class PrintInhibitor;
  friend class Render;
  friend class RSFilechooser;

  bool get_userconfirm(string maintext, string secondarytext="") const;

  void toggle_fullscreen();
  void gcode_status();
  void load_gcode();
  void save_gcode();
  void convert_to_gcode();
  void load_stl();
  void save_stl();
  void do_load_stl();

  void send_gcode();
  void printing_changed();
  void power_toggled(Gtk::ToggleToolButton *button);
  void print_clicked();
  void pause_toggled(Gtk::ToggleToolButton *button);
  void reset_clicked();
  void update_scale_value();
  void scale_object_x();
  void scale_object_y();
  void scale_object_z();

  void update_rot_value();
  void rot_object_from_spinbutton();
  
  string m_folder;
  
  Printer *m_printer;
  ExtruderRow *m_extruder_row;
  PrefsDlg *m_settings_ui;


  Glib::RefPtr<Gtk::Builder> m_builder;
  Model *m_model;
  ViewProgress *m_progress;
  ConnectView *m_cnx_view;
  Gtk::Entry *m_gcode_entry;
  Render *m_renderer;

  RSFilechooser *m_filechooser;
  void on_controlnotebook_switch(Gtk::Widget* page, guint page_num);

  void on_gcodebuffer_cursor_set (const Gtk::TextIter &iter,
				  const Glib::RefPtr <Gtk::TextMark> &refMark);
  Gtk::TextView * m_gcodetextview;

  Gtk::TextView *log_view, *err_view, *echo_view;
  void log_msg(Gtk::TextView *view, string s);

  Gtk::ToolButton *m_print_button;
  Gtk::ToggleToolButton *m_pause_button;

  void connect_button(const char *name, const sigc::slot<void> &slot);
  void connect_activate(const char *name, const sigc::slot<void> &slot);
  void connect_toggled(const char *name, const sigc::slot<void,
		       Gtk::ToggleButton *> &slot);
  void connect_tooltoggled(const char *name, const sigc::slot<void,
			   Gtk::ToggleToolButton *> &slot);
  virtual bool on_delete_event(GdkEventAny* event);

  void hide_on_response(int, Gtk::Dialog *dialog);
  void about_dialog();
  void load_settings();
  void save_settings();
  void save_settings_as();
  void save_settings_to(Glib::RefPtr < Gio::File > file);

  // interactive bits
  void temp_monitor_enabled_toggled (Gtk::ToggleButton *button);
  void enable_logging_toggled (Gtk::ToggleButton *button);
  void fan_enabled_toggled (Gtk::ToggleButton *button);
  void run_extruder();
  void clear_logs();
  void home_all();
  Gtk::SpinButton *m_extruder_speed;
  Gtk::SpinButton *m_extruder_length;
  Gtk::SpinButton *m_extruder_speed_mult;
  Gtk::SpinButton *m_extruder_length_mult;
  Gtk::SpinButton *m_fan_voltage;
  AxisRow *m_axis_rows[3];

  TempRow *m_temps[TEMP_LAST];
  void temp_changed();

  void edit_custombutton(string name="", string code="", Gtk::ToolButton *button=NULL);
  void new_custombutton() {edit_custombutton();};
  void hide_custombutton_dlg(int code, Gtk::Dialog *dialog);
  void add_custombutton(string name, string gcode);
  void custombutton_pressed(string name, Gtk::ToolButton *button);


  Gtk::TreeView * extruder_treeview;
  Glib::RefPtr< Gtk::ListStore > extruder_liststore;
  Gtk::TreeModelColumn<Glib::ustring> extrudername;
  void copy_extruder();
  void remove_extruder();
  void extruder_selected();
  void update_extruderlist();

  // rfo bits
  Gtk::TreeView *m_treeview;
  TranslationSpinRow *m_translation_row;
  void tree_selection_changed();
  void delete_selected_objects();
  void duplicate_selected_objects();
  void split_selected_objects();
  void merge_selected_objects();
  void divide_selected_objects();
  void update_settings_gui();
  void handle_ui_settings_changed();
  bool key_pressed_event(GdkEventKey *event);

  void setModel (Model *model);

  bool saveWindowSizeAndPosition(Settings &settings) const;

  bool statusBarMessage(Glib::ustring message);
  void stop_progress();

 public:
  Model *get_model() { return m_model; }
  ViewProgress *get_view_progress() { return m_progress; }
  bool get_selected_objects(vector<TreeObject*> &objects, vector<Shape*> &shapes);
  bool get_selected_shapes(vector<Shape*> &shapes, vector<Matrix4d> &transforms);

  View(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder);
  virtual ~View();
  static View *create (Model *model);

  void progess_bar_start (const char *label, double max);

  void inhibit_print_changed();
  void alert (Gtk::MessageType t, const char *message,
	      const char *secondary);
  bool move_selection( float x, float y, float z=0);
  bool rotate_selection (Vector3d axis, double angle);
  void scale_selection();
  void scale_selection_to(const double factor);
  void mirror_selection ();
  void placeonplatform_selection ();
  void stl_added (Gtk::TreePath &path);
  void model_changed ();

  void gcode_changed ();
  void set_SliderBBox(double max_z);

  void show_notebooktab (string name, string notebookname) const;
  void show_widget (string name, bool visible) const;

  void preview_file(Glib::RefPtr<Gio::File> file);

  void show_dialog(const char *name);
  void show_preferences();
  Glib::RefPtr<Gtk::Builder> getBuilder() const { return m_builder; };

  void err_log(string s);
  void comm_log(string s);
  void echo_log(string s);

  sigc::connection logprint_timeout;
  void set_logging(bool);
  bool logprint_timeout_cb();

  void Draw(vector<Gtk::TreeModel::Path> selected);
  void DrawGrid();
  void showCurrentPrinting(unsigned long line);

  Glib::Mutex mutex;

  Glib::RefPtr<Gio::File> iconfile;
  void set_icon_file(Glib::RefPtr<Gio::File> iconfile);

};

#ifdef MODEL_IMPLEMENTATION
#  error "The whole point is to avoid coupling the model to the view - think again"
#endif
