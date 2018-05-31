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
#ifndef RENDER_H
#define RENDER_H

#include <gtkmm.h>
#include <gtkmm/glarea.h>

class View;
class Model;
class Settings;

class RenderVert {
 private:
  vector< float > vec;

 public:
  RenderVert() {};
  ~RenderVert() {};
  void clear() {vec.clear();};
  void add(float x, float y, float z) {vec.push_back(x); vec.push_back(y); vec.push_back(z);};
  void add(float v[3]) {add(v[0], v[1], v[2]);};
  void add(double v[3]) {add(v[0], v[1], v[2]);};
  void add(Vector3d v) {add(v[0], v[1], v[2]);};
  
  size_t size() const {return vec.size() * sizeof(float);};
  const float *data() const {return &vec[0];};
};

class Render : public Gtk::GLArea {
 private:
  Matrix4d m_transform;
  Matrix4d m_full_transform;  
  View *m_view;
  Model *get_model() const;
  Glib::RefPtr<Gtk::TreeSelection> m_selection;
  
  Vector2d m_down_point;
  Matrix4d m_down_trans;
  
  void init_buffers();
  void init_shaders();

  GLuint m_trans;
  GLuint m_color;
  GLuint m_vao;
  GLuint m_buffer;
  GLuint m_program;
  
  // font rendering:
  static GLuint fontlistbase;
  static int fontheight;

  float m_zoom;
  void CenterView();
  void selection_changed();
  guint find_object_at(gdouble x, gdouble y);
  Vector3d mouse_on_plane(Vector2d scaled) const;
  Vector2d get_scaled(double x, double y);
  void SetTrans(const Matrix4d &trans);

 public:
  Render (View *view, Glib::RefPtr<Gtk::TreeSelection> selection);
  ~Render();

  GtkWidget *get_widget();
  void set_model (Model *model);
  void set_zoom (float zoom) {m_zoom=zoom;};

  void set_model_transform(const Matrix4d &trans);
  void set_default_transform(void);
  
  void draw_string(const Vector3d &pos, const string s);
  void draw_triangles(const float color[4], const RenderVert &vert);
  void draw_lines(const float color[4], const RenderVert &vert, float line_width);

  virtual void on_realize();
  virtual bool on_configure_event(GdkEventConfigure* event);
  virtual bool on_draw(const ::Cairo::RefPtr< ::Cairo::Context >& cr);
  virtual bool on_motion_notify_event(GdkEventMotion* event);
  virtual bool on_button_press_event(GdkEventButton* event);
  virtual bool on_button_release_event(GdkEventButton* event);
  virtual bool on_scroll_event(GdkEventScroll* event);
  virtual bool on_key_press_event(GdkEventKey* event);
  virtual bool on_key_release_event(GdkEventKey* event);
};

class RenderModelTrans {
 private:
  Render *m_render;
  
 public:
  RenderModelTrans(Render &render, const Matrix4f &trans) {m_render = &render; m_render->set_model_transform(trans);};
  ~RenderModelTrans() {m_render->set_default_transform();};
};

#endif // RENDER_H
