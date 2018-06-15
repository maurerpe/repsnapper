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

#include <stdint.h>
#include <epoxy/gl.h>

#include "config.h"
#include "stdafx.h"
#include "render.h"
#include "settings.h"
#include "ui/view.h"
#include "model.h"
#include "geometry.h"
#include "transform3d.h"

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

const Vector4d light_dir = {1, -1, 2, 0};
const double z_center = -750;

static Vector3d hom(Vector4d v) {
  if (v[3] == 0)
    return Vector3d(v[0], v[1], v[2]);
  
  return Vector3d(v[0]/v[3], v[1]/v[3], v[2]/v[3]);
}

inline GtkWidget *Render::get_widget() {
  return GTK_WIDGET(gobj());
}

inline Model *Render::get_model() const { return m_view->get_model(); }

Render::Render(View *view, Glib::RefPtr<Gtk::TreeSelection> selection) :
  m_realized(false), m_drawn_once(false), m_get_object_mode(false),
  m_view(view), m_selection(selection) {

  set_events(Gdk::POINTER_MOTION_MASK |
	     Gdk::BUTTON_MOTION_MASK |
	     Gdk::BUTTON_PRESS_MASK |
	     Gdk::BUTTON_RELEASE_MASK |
	     Gdk::BUTTON1_MOTION_MASK |
	     Gdk::BUTTON2_MOTION_MASK |
	     Gdk::BUTTON3_MOTION_MASK |
	     Gdk::KEY_PRESS_MASK |
	     Gdk::KEY_RELEASE_MASK |
	     Gdk::SCROLL_MASK);

  set_can_focus(true);

  Transform3D trans;
  double bedw = view->m_model->settings.get_double("Hardware", "Volume.X");
  double bedd = view->m_model->settings.get_double("Hardware", "Volume.Y");
  double bedh = view->m_model->settings.get_double("Hardware", "Volume.Z");
  trans.move(Vector3d(-bedw/2, -bedd/2, 0));
  m_transform = trans.getTransform();

  trans.identity();
  trans.rotate_to(Vector3d(0, 0, 0), -1.3, 0, 0);
  trans.move(Vector3d(0, 0, z_center));
  m_transform = trans.getTransform() * m_transform;
  
  m_zoom = 2 * atan(max(max(bedw, bedh), bedd) / fabs(z_center)) * 180 / M_PI;

  m_selection->signal_changed().connect (sigc::mem_fun(*this, &Render::selection_changed));
}

void Render::init_buffers() {
  glGenVertexArrays(1, &m_vao);
  glBindVertexArray(m_vao);

  glGenBuffers(1, &m_buffer);
}

static GLuint create_shader(int type, const char *src) {
  auto shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);

  int status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE) {
    int log_len;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
    string log_space(log_len+1, ' ');
    glGetShaderInfoLog(shader, log_len, nullptr, (GLchar *) log_space.c_str());

    cerr << "Compile failure int " <<
      (type == GL_VERTEX_SHADER ? "vertex" : "fragment") <<
      " shader: " << log_space << endl;
    
    glDeleteShader(shader);

    return 0;
  }

  return shader;
}

static GLuint compile_program(const char *vertex_code, const char *fragment_code) {
  auto vertex = create_shader(GL_VERTEX_SHADER,  vertex_code);
  auto fragment = create_shader(GL_FRAGMENT_SHADER, fragment_code);
  
  GLuint m_program = glCreateProgram();
  glAttachShader(m_program, vertex);
  glAttachShader(m_program, fragment);

  glLinkProgram(m_program);
  int status;
  glGetProgramiv(m_program, GL_LINK_STATUS, &status);
  if (status == GL_FALSE) {
    int log_len;
    glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &log_len);
    
    string log_space(log_len+1, ' ');
    glGetProgramInfoLog(m_program, log_len, nullptr, (GLchar*)log_space.c_str());
    
    cerr << "Linking failure: " << log_space << endl;
    
    glDeleteProgram(m_program);
    m_program = 0;
  }
  
  return m_program;
}

void Render::init_shaders() {
  m_line_program = compile_program(
    // Vertex shader
    "#version 330 core\n"
    "layout (location = 0) in vec3 vp;"
    "uniform mat4 trans;"
    "void main() {"
    "  gl_Position = trans * vec4(vp, 1.0);"
    "}",

    // Fragment shader
    "#version 330 core\n"
    "out vec4 frag_color;"
    "uniform vec4 color;"
    "void main() {"
    "  frag_color = color;"
    "}");
  
  m_line_trans = glGetUniformLocation(m_line_program, "trans");
  m_line_color = glGetUniformLocation(m_line_program, "color");

  m_tri_program = compile_program(
    // Vertex shader
    "#version 330 core\n"
    "layout (location = 0) in vec3 vp;"
    "layout (location = 1) in vec3 norm;"
    "uniform mat4 trans;"
    "uniform vec3 light;"
    "out float brightness;"
    "void main() {"
    "  brightness = clamp(0.1 + 0.9*abs(dot(norm,light)), 0, 1);"
    "  gl_Position = trans * vec4(vp, 1.0);"
    "}",

    // Fragment shader
    "#version 330 core\n"
    "in float brightness;"
    "out vec4 frag_color;"
    "uniform vec4 color;"
    "void main() {"
    "  frag_color = vec4(color.rgb * brightness, color.a);"
    "}");
  
  m_tri_trans = glGetUniformLocation(m_tri_program, "trans");
  m_tri_light = glGetUniformLocation(m_tri_program, "light");
  m_tri_color = glGetUniformLocation(m_tri_program, "color");
  
  m_str_program = compile_program(
    // Vertex shader
    "#version 330 core\n"
    "layout (location = 0) in vec3 vp;"
    "uniform mat4 trans;"
    "out vec2 textr_uv;"
    "void main() {"
    "  textr_uv = vec2(vp.x, 1-vp.y);"
    "  gl_Position = trans * vec4(vp, 1.0);"
    "}",

    // Fragment shader
    "#version 330 core\n"
    "in vec2 textr_uv;"
    "out vec4 frag_color;"
    "uniform sampler2D textr;"
    "void main() {"
    "  frag_color = texture(textr, textr_uv);"
    "}");
  
  m_str_trans = glGetUniformLocation(m_str_program, "trans");
  m_str_textr = glGetUniformLocation(m_str_program, "textr");
  
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  float border[] = { 0.0f, 0.0f, 0.0f, 0.0f };
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
}

Render::~Render() {
}

void Render::on_realize() {
  Gtk::GLArea::on_realize();

  cout << "Initializing render" << endl;
  make_current();
  try {
    throw_if_error();
    init_buffers();
    init_shaders();
    
    set_hexpand(true);
    set_vexpand(true);
    set_auto_render(true);

    m_realized = true;
  } catch (const Gdk::GLError& gle) {
    cerr << "An error occured making the context current during realize:" << endl;
    cerr << gle.domain() << "-" << gle.code() << "-" << gle.what() << endl;
  }
  
  cout << "Render Initialized" << endl;
}

bool Render::on_configure_event(GdkEventConfigure* event) {
  cout << "on_configure_event" << endl;
  
  return true;
}

static void SetUniform(GLuint mat, const Matrix4d &trans) {
  GLfloat val[16], *cur = val;
  
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      *cur++ = trans(j, i);
  
  glUniformMatrix4fv(mat, 1, GL_FALSE, val);
}

static void SetUniform(GLuint vec, const Vector3d &vv) {
  glUniform3f(vec, vv.x(), vv.y(), vv.z());
}

static void computeProjection(Matrix4d &mat, double fov, double aspect, double zNear, double zFar) {
  mat = Matrix4d::IDENTITY;

  if (fov <= 0 || aspect == 0)
    return;

  double inv_depth = 1.0 / fabs(zFar - zNear);
  mat(1,1) = 1.0 / tan(0.5 * fov * M_PI / 180);
  mat(0,0) = mat(1,1) / aspect;
  mat(2,2) = -(zFar + zNear) * inv_depth;
  mat(2,3) = -2 * zFar * zNear * inv_depth;
  mat(3,2) = -1;
  mat(3,3) = 0;
}

bool Render::on_draw(const ::Cairo::RefPtr< ::Cairo::Context >& cr) {
  // cout << "Render::on_draw" << endl;
  // cout << "Transform:" << endl;
  // cout << m_transform;
  // cout << "Zoom: " << m_zoom << endl;
  
  if (!m_realized)
    return Gtk::GLArea::on_draw(cr);
  
  /*glClearColor(0.5, 0.5, 0.5, 1.0);*/
  make_current();
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  //define blending factors

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_ALWAYS);
  
  glDisable(GL_CULL_FACE);
  
  glEnable(GL_LINE_SMOOTH);
  
  Matrix4d view_mat = m_transform;
  
  Matrix4d camera_mat;
  computeProjection(camera_mat,
		    m_zoom,
		    ((double) get_width()) / ((double) get_height()),
		    0.5 * fabs(z_center),
		    1.5 * fabs(z_center));
  m_full_transform = camera_mat * view_mat;
  
  // cout << "on_draw: m_full_transform" << endl << m_full_transform << endl;

  set_default_transform();
  
  vector<Gtk::TreeModel::Path> selpath = m_selection->get_selected_rows();
  m_view->Draw(selpath);

  // set_default_transform();
  // float color[4] = {0, 1, 1, 1};
  // draw_string(color, Vector3d(150, 110, 300), string("01234.56789"), 70);
  
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glUseProgram(0);
  
  glFlush();
  
  Gtk::GLArea::on_draw(cr);

  if (!m_drawn_once)
    queue_draw();
  m_drawn_once = true;
  
  return false;
}

void Render::set_model_transform(const Matrix4d &trans) {
  m_comb_transform = m_full_transform * trans;
  (m_transform * trans).inverse(m_inv_model);
}

void Render::set_default_transform(void) {
  m_comb_transform = m_full_transform;
  m_transform.inverse(m_inv_model);
}

const string font_face("Sans");

void Render::draw_string(const float color[4], const Vector3d &pos, const string s, double fontheight) {
  if (fontheight <= 0 || m_get_object_mode)
    return;
  fontheight *= 1.5;

  // Determine text size
  Cairo::RefPtr<Cairo::ImageSurface> dummy_surface =
    Cairo::ImageSurface::create(Cairo::FORMAT_A1, 1, 1);
  Cairo::RefPtr< Cairo::Context > dummy = Cairo::Context::create(dummy_surface);
  Cairo::TextExtents te;
  dummy->select_font_face(font_face, Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
  dummy->set_font_size(fontheight);
  dummy->get_text_extents(s, te);
  
  //cout << "Fontheight: " << fontheight << ", " << te.height << endl;  
  
  size_t border = max(fontheight / 5.0, 3.0);
  size_t width  = te.width + 2 * border;
  size_t height = te.height + 2 * border;
  size_t radius = 2*border;
  
  // Start transparent
  // Draw partially transparent rounded rect
  // Finally draw the text
  Cairo::RefPtr<Cairo::ImageSurface> surface =
    Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, width, height);
  Cairo::RefPtr< Cairo::Context > cr = Cairo::Context::create(surface); 
  cr->select_font_face(font_face, Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
  cr->set_font_size(fontheight);
  cr->set_source_rgba(0.0, 0.0, 0.0, 0.0);
  cr->paint();
  cr->set_source_rgba(0.0, 0.0, 0.0, 0.5);
  cr->begin_new_sub_path();
  cr->arc(        radius,          radius, radius,  M_PI,     -M_PI/2.0);
  cr->arc(width - radius,          radius, radius, -M_PI/2.0,  0);
  cr->arc(width - radius, height - radius, radius,  0,         M_PI/2.0);
  cr->arc(        radius, height - radius, radius,  M_PI/2.0,  M_PI);
  cr->close_path();
  cr->fill();
  cr->set_source_rgba(1.0, 0.0, 0.0, 1.0);
  cr->move_to(border - te.x_bearing,
	      border - te.y_bearing);
  cr->show_text(s);
  const uint32_t *data = (const uint32_t *) surface->get_data();

  // Convert to rgba for use as a texture
  const uint32_t *dcur = data, *dend = data + (width * height);
  unsigned char image[width*height*4], *icur = image;
  while (dcur < dend) {
    *icur++ = *dcur >> 16;
    *icur++ = *dcur >> 8;
    *icur++ = *dcur;
    *icur++ = *dcur++ >> 24;
  }

  // Map texture
  double w = 2.0 * ((double) width) / get_width();
  double h = 2.0 * ((double) height) / get_height();
  Vector4d pos4 = {pos.x(), pos.y(), pos.z(), 1};
  Vector3d tpos = hom(m_comb_transform * pos4);

  Transform3D trans;
  trans.move(Vector3d(tpos.x() - w/2, tpos.y() - h/2, 1));
  trans.scale_x(w);
  trans.scale_y(h);

  RenderVert vert;
  vert.add(0, 0, 0);
  vert.add(1, 0, 0);
  vert.add(1, 1, 0);
  
  vert.add(0, 0, 0);
  vert.add(1, 1, 0);
  vert.add(0, 1, 0);

  glUseProgram(m_str_program);
  SetUniform(m_str_trans, trans.getTransform());
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
	       GL_UNSIGNED_BYTE, image);
  
  glBindBuffer(GL_ARRAY_BUFFER, m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_buffer);
  glBufferData(GL_ARRAY_BUFFER, vert.size(), vert.data(), GL_STREAM_DRAW);
  
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr);
  
  glDrawArrays(GL_TRIANGLES, 0, vert.len() / 3);
}

static Vector3d GetCamera(const GLfloat *pos, const Matrix4d &trans) {
  Vector4d model(pos[0], pos[1], pos[2], 1.0);
  
  return hom(trans * model);
}

void Render::draw_triangles(const float color[4], const RenderVert &vert, size_t index) {
  //cout << "draw_triangles tranform:" << endl;
  
  if (m_get_object_mode) {
    if (index == 0)
      return;
    
    // cout << "Pick index: " << index << endl;
    
    // Slow but effective
    for (size_t count = 0; count < vert.len() - 17; count += 18) {
      Vector3d p0 = GetCamera(vert.data() + count, m_comb_transform);
      Vector3d p1 = GetCamera(vert.data() + count + 6, m_comb_transform);
      Vector3d p2 = GetCamera(vert.data() + count + 12, m_comb_transform);
      
      double area = 0.5 *(-p1.y()*p2.x() + p0.y()*(-p1.x() + p2.x()) + p0.x()*(p1.y() - p2.y()) + p1.x()*p2.y());

      double s = 1/(2*area)*(p0.y()*p2.x() - p0.x()*p2.y() + (p2.y() - p0.y())*m_down_point.x() + (p0.x() - p2.x())*m_down_point.y());
      double t = 1/(2*area)*(p0.x()*p1.y() - p0.y()*p1.x() + (p0.y() - p1.y())*m_down_point.x() + (p1.x() - p0.x())*m_down_point.y());
      double u = 1 - s - t;
      if (s < 0 || t < 0 || u < 0)
	continue;

      double z = p0.z() * s + p1.z() * t + p2.z() * u;
      // cout << "Found z = " << z << endl;
      if (z < m_object_z) {
	m_object_z = z;
	m_object_index = index;
      }
    }
    
    return;
  }
  
  glUseProgram(m_tri_program);
  SetUniform(m_tri_trans, m_comb_transform);
  SetUniform(m_tri_light, normalized(hom(m_inv_model * light_dir)));
  glUniform4fv(m_tri_color, 1, color);

  glBindBuffer(GL_ARRAY_BUFFER, m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_buffer);
  glBufferData(GL_ARRAY_BUFFER, vert.size(), vert.data(), GL_STREAM_DRAW);
  
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), nullptr);
  
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), BUFFER_OFFSET(3 * sizeof(GLfloat)));

  // cout << endl << "Drawing triangls:" << endl;
  // for (size_t i = 0; i < vert.len(); i += 6) {
  //   const GLfloat *pos = vert.data() + i;
  //   Vector4d model(pos[0], pos[1], pos[2], 1.0);
  //   Vector3d camera = hom(m_comb_transform * model);
  //   cout << model << " -> " << camera << endl;
  // }
  
  glDrawArrays(GL_TRIANGLES, 0, vert.len() / 6);  
}

void Render::draw_lines(const float color[4], const RenderVert &vert, float line_width) {
  if (m_get_object_mode)
    return;
  
  glUseProgram(m_line_program);
  SetUniform(m_line_trans, m_comb_transform);
  glUniform4fv(m_line_color, 1, color);
  glLineWidth(line_width);

  glBindBuffer(GL_ARRAY_BUFFER, m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_buffer);
  glBufferData(GL_ARRAY_BUFFER, vert.size(), vert.data(), GL_STREAM_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

  glDrawArrays(GL_LINES, 0, vert.len() / 3);
}

bool Render::on_key_press_event(GdkEventKey* event) {
  // cerr << "key " << event->keyval << endl;
  bool moveZ = (event->state & GDK_SHIFT_MASK);
  bool rotate = (event->state & GDK_CONTROL_MASK);
  double tendeg = M_PI/12.;

  bool ret = false;
  switch (event->keyval) {
  case GDK_KEY_Up:
  case GDK_KEY_KP_Up:
    if (rotate)     ret = m_view->rotate_selection(Vector3d(1.,0.,0.), tendeg);
    else if (moveZ) ret = m_view->move_selection( 0.0,  0.0, 1.0 );
    else            ret = m_view->move_selection( 0.0,  1.0 );
    break;
  case GDK_KEY_Down:
  case GDK_KEY_KP_Down:
    if (rotate)     ret = m_view->rotate_selection(Vector3d(1.,0.,0.), -tendeg);
    else if (moveZ) ret = m_view->move_selection( 0.0,  0.0, -1.0 );
    else            ret = m_view->move_selection( 0.0, -1.0 );
    break;
  case GDK_KEY_Left:
  case GDK_KEY_KP_Left:
    if (rotate)     ret = m_view->rotate_selection(Vector3d(0.,0.,1.), tendeg);
    else            ret = m_view->move_selection( -1.0, 0.0 );
    break;
  case GDK_KEY_Right:
  case GDK_KEY_KP_Right:
    if (rotate)     ret = m_view->rotate_selection(Vector3d(0.,0.,1.), -tendeg);
    else            ret = m_view->move_selection(  1.0, 0.0 );
    break;
  }
  if (ret) {
    m_view->get_model()->ModelChanged();
    queue_draw();
  }
  grab_focus();
  return ret;
}

Vector2d Render::get_scaled(double x, double y) {
  double w = get_width();
  double h = get_height();
  return Vector2d(2 * x/w - 1, 2 * (1 - y / h) - 1);
}

bool Render::on_button_press_event(GdkEventButton* event) {
  grab_focus();
  m_down_point = get_scaled(event->x, event->y);
  m_down_trans = m_transform;
  m_down_rot   = 0;

  if (event->button == 1) {
    guint index = find_object();
    if (!(event->state & GDK_SHIFT_MASK))
      m_selection->unselect_all();
    if (index) {
      Gtk::TreeModel::iterator iter = get_model()->objtree.find_stl_by_index(index);
      m_selection->select(iter);
    }
  }
  
  return true;
}

bool Render::on_scroll_event(GdkEventScroll* event) {
  double factor;

  if (event->state & GDK_SHIFT_MASK)
    factor = 1.01;
  else
    factor = 1.05;
  
  if (event->direction == GDK_SCROLL_UP)
    factor = 1.0/factor;
  
  if (event->state & GDK_CONTROL_MASK) {
    // Scale selection
    vector<Shape*> shapes;
    vector<TreeObject*>objects;
    if (!m_view->get_selected_objects(objects, shapes))
      return true;
    
    if (shapes.size()>0) {
      for (uint s=0; s<shapes.size(); s++)
	shapes[s]->Scale(shapes[s]->getScaleFactor()*factor, true);
      m_view->update_scale_value();
      queue_draw();
    }
  } else {
    // Zoom view
    m_zoom *= factor;
    
    if (m_zoom < 0.02)
      m_zoom = 0.02;
    else if (m_zoom > 90)
      m_zoom = 90;
  }
  
  // cout << "Render::on_scroll_event: " << m_zoom << endl;

  queue_draw();
  return true;
}

bool Render::on_motion_notify_event(GdkEventMotion* event) {
  Vector2d mouse = get_scaled(event->x, event->y);
  Vector2d delta = mouse - m_down_point;
  
  if (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) {
    // Action on shape
    if (event->state & GDK_BUTTON2_MASK) {
      if (event->state & GDK_CONTROL_MASK) {
	vector<Shape*> shapes;
	vector<TreeObject*>objects;
	if (!m_view->get_selected_objects(objects, shapes))
	  return true;
    
	// Rotate shape about Z
	Vector3d mouse_down = mouse_on_plane(m_down_point);
	Vector3d mouse_plat = mouse_on_plane(mouse);
	Vector3d axis = {0, 0, 1};
	if (shapes.size() > 0) {
	  Vector3d center = shapes[0]->Center;
	  Vector3d start = mouse_down - center;
	  Vector3d stop = mouse_plat - center;
	  double angle = atan2(stop.y(), stop.x()) - atan2(start.y(), start.x());
	  
	  if (event->state & GDK_SHIFT_MASK) {
	    double quant = 15.0 * M_PI / 180.0;
	    angle = round(angle / quant) * quant;
	  }
	  
	  // cout << "Center: " << center << endl;
	  // cout << "Angle: " << angle << endl;
	  m_view->rotate_selection(axis, angle - m_down_rot);
	  m_down_rot = angle;
	  queue_draw();
	}
      }
    } else if (event->state & GDK_BUTTON3_MASK) {
      // Shape Movement
      Vector3d movevec;
      if (event->state & GDK_CONTROL_MASK) {
	// Shape XY Movement
	Vector3d mouse_down = mouse_on_plane(m_down_point);
	Vector3d mouse_plat = mouse_on_plane(mouse);
	
	movevec = Vector3d(mouse_plat.x() - mouse_down.x(),
			   mouse_plat.y() - mouse_down.y(),
			   0);
      } else {
	// Shape Z Movement
	double scale = fabs(z_center) * tan(m_zoom * M_PI / 180.0 / 2.0);
	movevec = Vector3d(0, 0, scale * delta.y());
      }
      
      m_view->move_selection(movevec.x(), movevec.y(), movevec.z());
      m_down_point = mouse;
      queue_draw();
    }
    
    return true;
  }
  
  // Adjust View
  if (event->state & (GDK_BUTTON2_MASK | GDK_BUTTON3_MASK)) {
    Transform3D trans;
    if (event->state & GDK_BUTTON2_MASK) {
      // Rotate view
      trans.rotate(Vector3d(0, 0, z_center), Vector3d(1, 0, 0), -delta.y());
      trans.rotate(Vector3d(0, 0, z_center), Vector3d(0, 1, 0), delta.x());
    } else {
      // Translate view
      double scale_y = fabs(z_center) * tan(m_zoom * M_PI / 180.0 / 2.0);
      double scale_x = scale_y * get_width() / get_height();
      trans.move(Vector3d(scale_x * delta.x(), scale_y * delta.y()));
    }
    m_transform = trans.getTransform() * m_down_trans;
    queue_draw();
  }

  return true;
}

guint Render::find_object(void) {
  RenderGetObjectMode gom(this);
  
  // cout << "Finding object" << endl;
  
  m_object_z = 10;
  m_object_index = 0;
  
  vector<Gtk::TreeModel::Path> selpath = m_selection->get_selected_rows();
  m_view->Draw(selpath);  
  
  return m_object_index;
}

Vector3d Render::mouse_on_plane(Vector2d scaled) const {
  Matrix4d inv;
  m_full_transform.inverse(inv);
  
  Vector4d mouse0 = {scaled.x(), scaled.y(), 1.01, 1};
  Vector4d mouse1 = {scaled.x(), scaled.y(), 0.99, 1};
  
  Vector3d view0 = hom(inv * mouse0);
  Vector3d view1 = hom(inv * mouse1);

  Vector3d vnorm = normalized(view1 - view0);
  double dz = 0 - view0.z();
  Vector3d onp = view0 + vnorm * (dz / vnorm.z());
  
  // cout << "Mouse on plane " << onp << endl;
  
  return onp;
}
