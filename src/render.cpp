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

#include <epoxy/gl.h>

#include "config.h"
#include "stdafx.h"
#include "render.h"
#include "settings.h"
#include "ui/view.h"
#include "model.h"
#include "geometry.h"
#include "transform3d.h"

//#define TRYFONTS "helvetica,arial,dejavu sans,sans,courier"

const double z_center = -750;

static inline Vector3d hom(Vector4d v) {
  Vector3d ret = {v[0]/v[3], v[1]/v[3], v[2]/v[3]};
  
  return ret;
}

inline GtkWidget *Render::get_widget() {
  return GTK_WIDGET(gobj());
}

inline Model *Render::get_model() const { return m_view->get_model(); }

Render::Render (View *view, Glib::RefPtr<Gtk::TreeSelection> selection) :
  m_view (view), m_selection(selection) {

  set_events (Gdk::POINTER_MOTION_MASK |
	      Gdk::BUTTON_MOTION_MASK |
	      Gdk::BUTTON_PRESS_MASK |
	      Gdk::BUTTON_RELEASE_MASK |
	      Gdk::BUTTON1_MOTION_MASK |
	      Gdk::BUTTON2_MOTION_MASK |
	      Gdk::BUTTON3_MOTION_MASK |
	      Gdk::KEY_PRESS_MASK |
	      Gdk::KEY_RELEASE_MASK |
	      Gdk::SCROLL_MASK
	      );

  set_can_focus(true);

  Transform3D trans;
  /* FIXME: Set initial transform from settings */
  double bedw = 300;
  double bedd = 220;
  double bedh = 300;
  trans.move(Vector3d(-bedw/2, -bedd/2, 0));
  m_transform = trans.getTransform();

  trans.identity();
  trans.rotate_to(Vector3d(0, 0, 0), -1.3, 0, 0);
  trans.move(Vector3d(0, 0, z_center));
  m_transform = trans.getTransform() * m_transform;
  
  m_zoom = atan(max(max(bedw, bedh), bedd) / 2 / fabs(z_center)) * 180 / M_PI;

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

void Render::init_shaders() {
  auto vertex = create_shader(GL_VERTEX_SHADER,
			      "#version 330\n"
			      "in vec3 vp;"
 			      "uniform mat4 trans;"
			      "void main() {"
			      "  gl_Position = trans * vec4(vp, 1.0);"
			      "}");

  auto fragment = create_shader(GL_FRAGMENT_SHADER,
				"#version 330\n"
				"out vec4 frag_color;"
				"uniform vec4 color;"
				"void main() {"
				"  frag_color = color;"
				"}");
  
  m_program = glCreateProgram();
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
  
  m_trans = glGetUniformLocation(m_program, "trans");
  m_color = glGetUniformLocation(m_program, "color");
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

    realized = true;
  } catch (const Gdk::GLError& gle) {
    cerr << "An error occured making the context current during realize:" << endl;
    cerr << gle.domain() << "-" << gle.code() << "-" << gle.what() << endl;
  }
  
  cout << "Render Initialized" << endl;
  
  queue_draw();
}

bool Render::on_configure_event(GdkEventConfigure* event) {
  return true;
}

void Render::SetTrans(const Matrix4d &trans) {
  GLfloat val[16], *cur = val;
  
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      *cur++ = trans(j, i);
  
  glUniformMatrix4fv(m_trans, 1, GL_FALSE, val);
}

bool Render::on_draw(const ::Cairo::RefPtr< ::Cairo::Context >& cr) {
  // cout << "Render::on_draw" << endl;
  // cout << "Transform:" << endl;
  // cout << m_transform;
  // cout << "Zoom: " << m_zoom << endl;
  
  if (!realized)
    return Gtk::GLArea::on_draw(cr);
  
  /*glClearColor(0.5, 0.5, 0.5, 1.0);*/
  make_current();
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glUseProgram(m_program);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  //define blending factors

  glDisable(GL_DEPTH_TEST);

  //glEnable(GL_CULL_FACE);
  
  glEnable(GL_LINE_SMOOTH);
  
  Matrix4d view_mat = m_transform;
  
  Matrix4d camera_mat;
  vmml::frustum< double > camera;
  camera.set_perspective(m_zoom,
			 ((double) get_width()) / ((double) get_height()),
			 0.1,
			 5000);
  camera.compute_matrix(camera_mat);
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
  
  return true;
}

void Render::set_model_transform(const Matrix4d &trans) {
  m_comb_transform = m_full_transform * trans;
  SetTrans(m_comb_transform);
}

void Render::set_default_transform(void) {
  m_comb_transform = m_full_transform;
  SetTrans(m_full_transform);
}

static void AddPolyLines(RenderVert &vert, const double *pts, double x_offset) {
  while (!isnan(*pts)) {
    pts += 2;
    while (!isnan(*pts)) {
      vert.add(x_offset + pts[-2], pts[-1], 0);
      vert.add(x_offset + pts[ 0], pts[ 1], 0);
      pts += 2;
    }
    pts++;
  }
}

static const double char_0[] = {0.35,1, 0.7,0.5, 0.7,-0.5, 0.35,-1, -0.35,-1, -0.7,-0.5, -0.7,0.5, -0.35,1, 0.35,1, NAN, NAN};
static const double char_1[] = {-0.35,0.5, 0,1, 0,-1, NAN, NAN};
static const double char_2[] = {-0.7,0.5, -0.35,1, 0.35,1, 0.7,0.5, 0.7,0.2, -0.7,-1, 0.7,-1, NAN, NAN};
static const double char_3[] = {-0.7,0.67, -0.35,1, 0.35,1, 0.7,0.67, 0.7,0.33, 0.35,0, -0.35,0, NAN,
				0.35,0, 0.7,-0.33, 0.7,-0.67, 0.35,-1, -0.35,-1, -0.7,-0.67, NAN, NAN};
static const double char_4[] = {-0.7,1, -0.7,0, 0.7,0, NAN, 0.7,1,0.7,-1, NAN, NAN};
static const double char_5[] = {0.7,1, -0.7,1, -0.7,0, 0.35,0, 0.7,-0.33, 0.7,-0.67, 0.35,-1, -0.35,-1, -0.7,-0.67, NAN, NAN};
static const double char_6[] = {-0.7,-0.33, -0.35,0, 0.35,0, 0.7,-0.33, 0.7,-0.67, 0.35,-1, -0.35,-1, -0.7,-0.67, NAN,
				-0.7,-0.67, -0.7,0.67, -0.35,1, 0.35,1, 0.7,0.67, NAN, NAN};
static const double char_7[] = {-0.7,1, 0.7,1, -0.35,-1, NAN, NAN};
static const double char_8[] = {-0.35,0, -0.7,0.33, -0.7,0.67, -0.35,1, 0.35,1, 0.7,0.67, 0.7,0.33, 0.35,0, -0.35,0, NAN,
				0.35,0, 0.7,-0.33, 0.7,-0.67, 0.35,-1, -0.35,-1, -0.7,-0.67, -0.7,-0.33, -0.35,0, NAN, NAN};
static const double char_9[] = {0.7,0.33, 0.35,0, -0.35,0, -0.7,0.33, -0.7,0.67, -0.35,1, 0.35,1, 0.7,0.67, NAN,
				0.7,0.67, 0.7,-0.67, 0.35,-1, -0.35,-1, -0.7,-0.67, NAN, NAN};
static const double period[] = {0.2,-1, -0.2,-1, -0.2,-0.7, 0.2,-0.7, 0.2,-1, NAN, NAN};
static const double comma[]  = {0.2,-1 -0.2,-1, -0.2,-0.7, 0.2,-0.7, 0.2,-1.2, NAN, NAN};
static const double hyphen[] = {-0.7,0, 0.7,0, NAN, NAN};
static const double char_x[] = {-0.7,1,  0.7,-1, NAN,
				-0.7,-1, 0.7,1,  NAN, NAN};

static void AddChar(RenderVert &vert, char ch, double x_offset) {
  const double *lines;
  
  switch (ch) {
  case '0': lines = char_0; break;
  case '1': lines = char_1; break;
  case '2': lines = char_2; break;
  case '3': lines = char_3; break;
  case '4': lines = char_4; break;
  case '5': lines = char_5; break;
  case '6': lines = char_6; break;
  case '7': lines = char_7; break;
  case '8': lines = char_8; break;
  case '9': lines = char_9; break;
  case '.': lines = period; break;
  case ',': lines = comma;  break;
  case '-': lines = hyphen; break;
  default:  lines = char_x; break;
  }

  AddPolyLines(vert, lines, x_offset);
}

void Render::draw_string(const float color[4], const Vector3d &pos, const string s, double fontheight) {
  if (fontheight <= 0) return;

  size_t len = s.size();
  double char_height = fontheight / get_height();
  double char_width  = fontheight * 5.0 / 8.0 / get_width();
  
  Vector4d pos4 = {pos.x(), pos.y(), pos.z(), 1};
  Vector3d tpos = hom(m_comb_transform * pos4);
  double x = tpos.x() - char_width * len;
  double y = tpos.y();

  // cout << "String: " << s << " @ " << tpos << endl;
  
  Transform3D trans;
  trans.move(Vector3d(x, y, 1));
  trans.scale_x(char_width);
  trans.scale_y(char_height);
  SetTrans(trans.getTransform());

  float bg_color[4] = {0, 0, 0, 0.5};
  RenderVert vert;
  vert.add(-1.2, -1.2, 0);
  vert.add(-1.2,  1.2, 0);
  vert.add(2.0 * len - 0.8, -1.2, 0);

  vert.add(2.0 * len - 0.8, -1.2, 0);
  vert.add(2.0 * len - 0.8,  1.2, 0);
  vert.add(-1.2,  1.2, 0);
  draw_triangles(bg_color, vert);
  
  vert.clear();
  for (size_t count = 0; count < len; count++)
    AddChar(vert, s[count], 2.0 * count);

  draw_lines(color, vert, 1.0);
  
  SetTrans(m_comb_transform);
}

void Render::draw_triangles(const float color[4], const RenderVert &vert) {
  //cout << "draw_triangles tranform:" << endl;
  
  glUniform4fv(m_color, 1, color);

  glBindBuffer(GL_ARRAY_BUFFER, m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_buffer);
  glBufferData(GL_ARRAY_BUFFER, vert.size(), vert.data(), GL_STREAM_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

  glDrawArrays(GL_TRIANGLES, 0, vert.size() / (3 * sizeof(GLfloat)));  
}

void Render::draw_lines(const float color[4], const RenderVert &vert, float line_width) {
  glUniform4fv(m_color, 1, color);
  glLineWidth(line_width);

  glBindBuffer(GL_ARRAY_BUFFER, m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_buffer);
  glBufferData(GL_ARRAY_BUFFER, vert.size(), vert.data(), GL_STREAM_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

  glDrawArrays(GL_LINES, 0, vert.size() / (3 * sizeof(GLfloat)));
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

bool Render::on_key_release_event(GdkEventKey* event) {
  switch (event->keyval) {
  case GDK_KEY_Up: case GDK_KEY_KP_Up:
  case GDK_KEY_Down: case GDK_KEY_KP_Down:
  case GDK_KEY_Left: case GDK_KEY_KP_Left:
  case GDK_KEY_Right: case GDK_KEY_KP_Right:
    return false;
  }
  return true;
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

  if ( event->button == 1 &&
       (event->state & GDK_SHIFT_MASK || event->state & GDK_CONTROL_MASK) )  {
    guint index = find_object_at(event->x, event->y);
    if (index) {
      Gtk::TreeModel::iterator iter = get_model()->objtree.find_stl_by_index(index);
      if (!m_selection->is_selected(iter)) {
	m_selection->unselect_all();
	m_selection->select(iter);
      }
    }
  }
  return true;
}

bool Render::on_button_release_event(GdkEventButton* event) {
  return true;
}

bool Render::on_scroll_event(GdkEventScroll* event) {
  double factor = 1.05;
  if (event->direction == GDK_SCROLL_UP)
    factor = 1.0/factor;
  
  if (event->state & GDK_SHIFT_MASK) {
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
    
    if (m_zoom < 0.1)
      m_zoom = 0.2;
    else if (m_zoom > 30)
      m_zoom = 30;
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
    if (event->state & GDK_BUTTON1_MASK) {
      // Shape Movement
      Vector3d movevec;
      if (event->state & GDK_SHIFT_MASK) {
	// Shape XY Movement
	Vector3d mouse_down = mouse_on_plane(m_down_point);
	Vector3d mouse_plat = mouse_on_plane(mouse);
	
	movevec = Vector3d(mouse_plat.x() - mouse_down.x(),
			   mouse_plat.y() - mouse_down.y(),
			   0);
      } else {
	// Shape Z Movement
	double scale = 2 * fabs(z_center) * tan(m_zoom * M_PI / 180.0);
	movevec = Vector3d(0, 0, scale * delta.y());
      }
      
      m_view->move_selection(movevec.x(), movevec.y(), movevec.z());
      queue_draw();
    } else if (event->state & GDK_BUTTON3_MASK) {
      if (event->state & GDK_SHIFT_MASK) {
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
	  // cout << "Center: " << center << endl;
	  // cout << "Angle: " << angle << endl;
	  m_view->rotate_selection(axis, angle);
	  queue_draw();
	}
      }
    }
    
    m_down_point = mouse;
    return true;
  }
  
  // Adjust View
  if (event->state & (GDK_BUTTON1_MASK | GDK_BUTTON3_MASK)) {
    Transform3D trans;
    if (event->state & GDK_BUTTON1_MASK) {
      // Translate view
      double scale = 2 * fabs(z_center) * tan(m_zoom * M_PI / 180.0);
      trans.move(Vector3d(scale * delta.x(), scale * delta.y()));
    } else {
      // Rotate view
      trans.rotate(Vector3d(0, 0, z_center), Vector3d(1, 0, 0), -delta.y());
      trans.rotate(Vector3d(0, 0, z_center), Vector3d(0, 1, 0), delta.x());
    }
    m_transform = trans.getTransform() * m_down_trans;
    queue_draw();
  }

  return true;
}

void Render::CenterView() {
  /* FIXME */
  /* Vector3d c = get_model()->GetViewCenter(); */
}

guint Render::find_object_at(gdouble x, gdouble y) {
  /* FIXME */
  
  return 0;
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
