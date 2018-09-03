/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2010  Kulitorum

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

#include <gtkmm.h>
#include "shape.h"

class TreeObject {
public:
  TreeObject();
  ~TreeObject();
  string name;
  Transform3D transform3D;
  vector<Shape*> shapes;
  bool deleteShape(uint i);
  short dimensions;
  uint size(){return shapes.size();};
  int idx;
  Gtk::TreePath addShape(Shape *shape, std::string location);
  void move(const Vector3d &delta){ transform3D.move(delta); };
  Vector3d center() const;
};

class ObjectsTree {
  void update_model();
  bool inhibit_row_changed;
  void update_shapenames(Gtk::TreeModel::Children children);
  void extruders_changed_raw(Gtk::TreeModel::Children children);
  void on_row_changed(const Gtk::TreeModel::Path& path,
		      const Gtk::TreeModel::iterator& iter);
  
 public:
  class ModelColumns : public Gtk::TreeModelColumnRecord {
 public:
   ModelColumns() {
     add(m_name);
     add(m_object);
     add(m_shape);
     add(m_pickindex);
     add(m_extruder);
   }
   Gtk::TreeModelColumn<Glib::ustring> m_name;
   Gtk::TreeModelColumn<int>           m_object;
   Gtk::TreeModelColumn<int>           m_shape;
   Gtk::TreeModelColumn<int>           m_pickindex;
   Gtk::TreeModelColumn<Glib::ustring> m_extruder;
 };
 
 ObjectsTree();
 ~ObjectsTree();
 
 void clear();
 bool empty() const {return Objects.size()==0; }
 
 void DeleteSelected(vector<Gtk::TreeModel::Path> &iter);
 void newObject();
 Gtk::TreePath addShape(TreeObject *parent, Shape *shape, std::string location);
 void get_selected_objects(const vector<Gtk::TreeModel::Path> &iter,
			   vector<TreeObject*> &object, vector<Shape*> &shape) const;
 
 void get_selected_shapes(const vector<Gtk::TreeModel::Path> &iter,
			  vector<Shape*> &shape, vector<Matrix4d> &transforms) const;
 
 void get_all_shapes(vector<Shape*> &shapes, vector<Matrix4d> &transforms) const;
 
 Gtk::TreeModel::iterator find_stl_by_index(guint pickindex);
 
 void extruders_changed();
 Matrix4d getTransformationMatrix(int object, int shape=-1) const;
 
 TreeObject * getParent(const Shape *shape) const;
 vector<TreeObject*> Objects;
 Transform3D transform3D;
 float version;
 string m_filename;
 Glib::RefPtr<Gtk::TreeStore> m_model;
 ModelColumns   *m_cols;
 private:
 Gtk::TreeModel::iterator find_stl_in_children(Gtk::TreeModel::Children children,
					       guint pickindex);
};