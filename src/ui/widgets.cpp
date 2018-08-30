/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2011 Michael Meeks

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

#include <glib/gi18n.h>

#include "widgets.h"
#include "objtree.h"
#include "model.h"

static const char *axis_names[] = {"X", "Y", "Z"};

/************************** TranslationSpinRow ****************************/

static int GetENo(Gtk::ComboBoxText *w) {
  string ext = w->get_active_text();
  if (ext.length() <= 8)
    return -1;
  
  return stoi(ext.substr(8));
}

void View::TranslationSpinRow::spin_value_changed(int axis) {
  if (m_inhibit_update)
    return;
  
  vector<Shape*> shapes;
  vector<TreeObject*> objects;
  
  if (!m_view->get_selected_objects(objects, shapes))
    return;
  
  if (shapes.size()==0 && objects.size()==0)
    return;
  
  /* FIXME: don't modify tranform3D.tranform directly */
  double val = m_xyz[axis]->get_value();
  Matrix4d *mat;
  if (shapes.size()!=0)
    for (uint s=0; s<shapes.size(); s++) {
      mat = &shapes[s]->transform3D.transform;
      double scale = (*mat)[3][3];
      Vector3d trans;
      mat->get_translation(trans);
      trans[axis] = val*scale;
      mat->set_translation(trans);
    }
  else
    for (uint o=0; o<objects.size(); o++) {
      mat = &objects[o]->transform3D.transform;
      double scale = (*mat)[3][3];
      Vector3d trans;
      mat->get_translation(trans);
      trans[axis] = val*scale;
      mat->set_translation(trans);
    }
  
  m_view->get_model()->ModelChanged();
}

void View::TranslationSpinRow::combobox_changed() {
  if (m_inhibit_update)
    return;
  
  int e_no = GetENo(m_extruder);

  vector<Shape*> shapes;
  vector<TreeObject*> objects;
  
  if (!m_view->get_selected_objects(objects, shapes))
    return;
  
  if (shapes.size()==0 && objects.size()==0)
    return;
  
  for (uint s=0; s<shapes.size(); s++)
    shapes[s]->extruder = e_no;
  
  for (uint o=0; o<objects.size(); o++) {
    vector<Shape *> &shapes = objects[o]->shapes;
    for (uint s=0; s<shapes.size(); s++)
      shapes[s]->extruder = e_no;
  }

  m_view->m_model->objtree.extruders_changed();
}

// Changed STL Selection - must update translation values
void View::TranslationSpinRow::selection_changed() {
  Inhibitor inhibit(&m_inhibit_update);
  
  vector<Shape*> shapes;
  vector<TreeObject*> objects;
  
  if (!m_view->get_selected_objects(objects, shapes))
    return;
  
  if (shapes.size()==0 && objects.size()==0)
    return;
  
  /* FIXME: don't modify tranform3D.tranform directly */
  Matrix4d *mat;
  int e_no = 1;
  if (shapes.size()==0) {
    if (objects.size()==0) {
      for (uint i = 0; i < 3; i++)
	m_xyz[i]->set_value(0.0);
      return;
    } else {
      mat = &objects.back()->transform3D.transform;
    }
  }
  else {
    mat = &shapes.back()->transform3D.transform;
    e_no = shapes.back()->extruder;
  }
  Vector3d trans;
  mat->get_translation(trans);
  double scale = (*mat)[3][3];
  for (uint i = 0; i < 3; i++)
    m_xyz[i]->set_value(trans[i]/scale);
  m_extruder->set_active_text("Extruder " + to_string(e_no));
}

View::TranslationSpinRow::TranslationSpinRow(View *view, Gtk::TreeView *treeview) :
  m_inhibit_update(false), m_view(view){
  view->m_builder->get_widget("translate_x", m_xyz[0]);
  view->m_builder->get_widget("translate_y", m_xyz[1]);
  view->m_builder->get_widget("translate_z", m_xyz[2]);
  view->m_builder->get_widget("m_extruder",  m_extruder);
  
  for (uint i = 0; i < 3; i++) {
    m_xyz[i]->signal_value_changed().connect
      (sigc::bind(sigc::mem_fun(*this, &TranslationSpinRow::spin_value_changed), (int)i));
  }
  m_extruder->signal_changed().connect
    (sigc::mem_fun(*this, &TranslationSpinRow::combobox_changed));
  selection_changed();
  
  treeview->get_selection()->signal_changed().connect
    (sigc::mem_fun(*this, &TranslationSpinRow::selection_changed));
}

View::TranslationSpinRow::~TranslationSpinRow() {
  for (uint i = 0; i < 3; i++)
    delete m_xyz[i];
}

/************************** AxisRow ***********************************/

void View::AxisRow::home_clicked() {
  m_printer->Home(std::string (axis_names[m_axis]));
  m_target->set_value(0.0);
}

void View::AxisRow::spin_value_changed() {
  if (m_inhibit_update)
    return;
  m_printer->Goto (std::string (axis_names[m_axis]), m_target->get_value());
}

void View::AxisRow::nudge_clicked (double nudge) {
  Inhibitor inhibit(&m_inhibit_update);
  m_target->set_value (MAX (m_target->get_value () + nudge, 0.0));
  m_printer->Move (std::string (axis_names[m_axis]), nudge);
}

void View::AxisRow::add_nudge_button(double nudge) {
  std::stringstream label;
  if (nudge > 0)
    label << "+";
  label << nudge;
  Gtk::Button *button = new Gtk::Button(label.str());
  add(*button);
  button->signal_clicked().connect
    (sigc::bind(sigc::mem_fun (*this, &AxisRow::nudge_clicked), nudge));
}

void View::AxisRow::notify_homed() {
  Inhibitor inhibit(&m_inhibit_update);
  m_target->set_value(0.0);
}

View::AxisRow::AxisRow(Model *model, Printer *printer, int axis) :
  m_inhibit_update(false), m_model(model), m_printer(printer), m_axis(axis) {
  add(*manage(new Gtk::Label(axis_names[axis])));
  Gtk::Button *home = new Gtk::Button(_("Home"));
  home->signal_clicked().connect
    (sigc::mem_fun (*this, &AxisRow::home_clicked));
  add (*home);
  
  add_nudge_button (-10.0);
  add_nudge_button (-1.0);
  add_nudge_button (-0.1);
  m_target = manage (new Gtk::SpinButton());
  m_target->set_digits (1);
  m_target->set_increments (0.1, 1);
  m_target->set_range(-200.0, +200.0);
  m_target->set_value(0.0);
  add (*m_target);
  m_target->signal_value_changed().connect
    (sigc::mem_fun(*this, &AxisRow::spin_value_changed));
  
  add_nudge_button (+0.1);
  add_nudge_button (+1.0);
  add_nudge_button (+10.0);
}
