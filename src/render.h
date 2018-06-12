/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2010 Michael Meeks
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

#include <gtkmm.h>
#include <gtkmm/glarea.h>

#include <epoxy/gl.h>

class View;
class Model;

class RenderVert {
 private:
  vector< GLfloat > vec;

 public:
  RenderVert() {};
  ~RenderVert() {};
  void clear() {vec.clear();};

  void add(float x, float y, float z) {vec.push_back(x); vec.push_back(y); vec.push_back(z);};
  void add(float v[3]) {add(v[0], v[1], v[2]);};
  void add(double v[3]) {add(v[0], v[1], v[2]);};
  void add(Vector3d v) {add(v[0], v[1], v[2]);};

  size_t len() const {return vec.size();};
  size_t size() const {return vec.size() * sizeof(float);};
  const GLfloat *data() const {return &vec[0];};
};

class Render : public Gtk::GLArea {
 private:
  bool m_realized;
  bool m_drawn_once;

  bool m_get_object_mode;
  double m_object_z;
  size_t m_object_index;
  
  Matrix4d m_transform;
  Matrix4d m_full_transform;  
  Matrix4d m_comb_transform;
  Matrix4d m_inv_model;
  View *m_view;
  Model *get_model() const;
  Glib::RefPtr<Gtk::TreeSelection> m_selection;
  
  Vector2d m_down_point;
  Matrix4d m_down_trans;
  
  void init_buffers();
  void init_shaders();
  
  GLuint m_vao;
  GLuint m_buffer;
  
  GLuint m_line_trans;
  GLuint m_line_color;
  GLuint m_line_program;

  GLuint m_tri_trans;
  GLuint m_tri_light;
  GLuint m_tri_color;
  GLuint m_tri_program;

  GLuint m_str_trans;
  GLuint m_str_textr;
  GLuint m_str_program;
  
  float m_zoom;
  void selection_changed() {queue_draw();};
  guint find_object(void);
  Vector3d mouse_on_plane(Vector2d scaled) const;
  Vector2d get_scaled(double x, double y);
  
 public:
  Render (View *view, Glib::RefPtr<Gtk::TreeSelection> selection);
  ~Render();

  GtkWidget *get_widget();

  void set_model_transform(const Matrix4d &trans);
  void set_default_transform(void);
  
  void draw_string(const float color[4], const Vector3d &pos, const string s, double fontheight);
  void draw_triangles(const float color[4], const RenderVert &vert, size_t index);
  void draw_lines(const float color[4], const RenderVert &vert, float line_width);

  void object_mode(bool mode) {m_get_object_mode = mode;};
  
  virtual void on_realize();
  virtual bool on_configure_event(GdkEventConfigure* event);
  virtual bool on_draw(const ::Cairo::RefPtr< ::Cairo::Context >& cr);
  virtual bool on_motion_notify_event(GdkEventMotion* event);
  virtual bool on_button_press_event(GdkEventButton* event);
  virtual bool on_scroll_event(GdkEventScroll* event);
  virtual bool on_key_press_event(GdkEventKey* event);
};

class RenderModelTrans {
 private:
  Render *m_render;
  
 public:
  RenderModelTrans(Render &render, const Matrix4f &trans) {m_render = &render; m_render->set_model_transform(trans);};
  ~RenderModelTrans() {m_render->set_default_transform();};
};

class RenderGetObjectMode {
 private:
  Render *r;
  
 public:
  RenderGetObjectMode(Render *render) : r(render) {r->object_mode(true);};
  ~RenderGetObjectMode() {r->object_mode(false);};
};
