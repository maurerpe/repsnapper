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

#include <epoxy/gl.h>

#include "config.h"
#include "stdafx.h"
#include "render.h"
#include "arcball.h"
#include "gllight.h"
#include "settings.h"
#include "ui/view.h"
#include "model.h"
#include "geometry.h"

#define TRYFONTS "helvetica,arial,dejavu sans,sans,courier"
#define FONTSIZE 8
GLuint Render::fontlistbase = 0;
int Render::fontheight = 0;

inline GtkWidget *Render::get_widget() {
  return GTK_WIDGET(gobj());
}

inline Model *Render::get_model() const { return m_view->get_model(); }

Render::Render (View *view, Glib::RefPtr<Gtk::TreeSelection> selection) :
  m_arcBall(new ArcBall()), m_view (view), m_selection(selection) {

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

  set_can_focus (true);

  memset (&m_transform.M, 0, sizeof (m_transform.M));

  Matrix3fT identity;
  Matrix3fSetIdentity(&identity);

  // set initial rotation
  identity.s.M11 = identity.s.M22 = 0.5253; // cos -58.3
  identity.s.M12 = 0.851; // -sin -58.3
  identity.s.M21 = -0.851; // sin -58.3

  Matrix4fSetRotationScaleFromMatrix3f(&m_transform, &identity);
  m_transform.s.SW = 1.0;

  m_zoom = 120.0;

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
  delete m_arcBall;
}

void Render::set_model(Model *model) {
  if (!model)
    return;
  
  m_zoom = model->settings.getPrintVolume().find_max();
  setArcballTrans(m_transform,
		  model->settings.getPrintMargin() );
  queue_draw();
}

void Render::selection_changed() {
  queue_draw();
}

void Render::on_realize() {
  Gtk::GLArea::on_realize();

  cout << "Initalizing render" << endl;
  make_current();
  init_buffers();
  init_shaders();
  
  set_hexpand(true);
  set_vexpand(true);
  set_auto_render(true);
  cout << "Render Initialized" << endl;
  
  /* FIXME: Find fonts */

  queue_draw();
}

bool Render::on_configure_event(GdkEventConfigure* event) {
  const int w = get_width(), h = get_height();
  
  if (w > 1 && h > 1) // Limit arcball minimum size or it asserts
    m_arcBall->setBounds(w, h);

  return true;
}

bool Render::on_draw(const ::Cairo::RefPtr< ::Cairo::Context >& cr) {
  // cout << "Render::on_draw" << endl;
  // cout << "Transform:" << endl;
  // cout << m_transform;
  // cout << "Zoom: " << m_zoom << endl;
  
  /*glClearColor(0.5, 0.5, 0.5, 1.0);*/
  make_current();
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glUseProgram(m_program);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  //define blending factors

  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);

  //glEnable(GL_CULL_FACE);
  
  glEnable(GL_LINE_SMOOTH);
  
  Matrix4fT view_mat = m_transform;
  view_mat.s.M23 -= m_zoom / 100;
  
  Matrix4d camera_mat;
  vmml::frustum< double > camera;
  camera.set_perspective(45.0,
			 ((double) get_width()) / ((double) get_height()),
			 0.1,
			 100);
  camera.compute_matrix(camera_mat);
  for (int i = 0; i < 4; i ++) {
    for (int j = 0; j < 4; j++) {
      m_full_transform.M[4*i+j] =
	view_mat.M[4*i+0] * camera_mat(j,0) +
	view_mat.M[4*i+1] * camera_mat(j,1) +
	view_mat.M[4*i+2] * camera_mat(j,2) +
	view_mat.M[4*i+3] * camera_mat(j,3);
    }
  }
  
  glUniformMatrix4fv(m_trans, 1, GL_FALSE, m_full_transform.M);
  
  vector<Gtk::TreeModel::Path> selpath = m_selection->get_selected_rows();
  m_view->Draw(selpath);
  
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glUseProgram(0);
  
  glFlush();
  
  Gtk::GLArea::on_draw(cr);
  
  return true;
}

void Render::set_model_transform(const Matrix4f &trans) {
  Matrix4fT comb_trans;
  
  for (int i = 0; i < 4; i ++) {
    for (int j = 0; j < 4; j++) {
      comb_trans.M[4*i+j] =
	trans(0,i) * m_full_transform.M[ 0+j] +
	trans(1,i) * m_full_transform.M[ 4+j] +
	trans(2,i) * m_full_transform.M[ 8+j] +
	trans(3,i) * m_full_transform.M[12+j];
    }
  }
  
  glUniformMatrix4fv(m_trans, 1, GL_FALSE, comb_trans.M);
}

void Render::set_default_transform(void) {
  glUniformMatrix4fv(m_trans, 1, GL_FALSE, m_full_transform.M);
}

void Render::draw_string(const Vector3d &pos, const string s) {
  if (fontheight == 0) return;
  /* FIXME: Draw string */
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
  double tendeg = M_PI/18.;

  bool ret = false;
  switch (event->keyval)
    {
    case GDK_KEY_Up: case GDK_KEY_KP_Up:
      if (rotate)     ret = m_view->rotate_selection(Vector3d(1.,0.,0.), tendeg);
      else if (moveZ) ret = m_view->move_selection( 0.0,  0.0, 1.0 );
      else            ret = m_view->move_selection( 0.0,  1.0 );
      break;
    case GDK_KEY_Down: case GDK_KEY_KP_Down:
      if (rotate)     ret = m_view->rotate_selection(Vector3d(1.,0.,0.), -tendeg);
      else if (moveZ) ret = m_view->move_selection( 0.0,  0.0, -1.0 );
      else            ret = m_view->move_selection( 0.0, -1.0 );
      break;
    case GDK_KEY_Left: case GDK_KEY_KP_Left:
      if (rotate)     ret = m_view->rotate_selection(Vector3d(0.,0.,1.), tendeg);
      else            ret = m_view->move_selection( -1.0, 0.0 );
      break;
    case GDK_KEY_Right: case GDK_KEY_KP_Right:
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
  switch (event->keyval)
    {
    case GDK_KEY_Up: case GDK_KEY_KP_Up:
    case GDK_KEY_Down: case GDK_KEY_KP_Down:
    case GDK_KEY_Left: case GDK_KEY_KP_Left:
    case GDK_KEY_Right: case GDK_KEY_KP_Right:
      return false;
    }
  return true;
}

bool Render::on_button_press_event(GdkEventButton* event) {
  grab_focus();
  // real mouse-down:
  m_downPoint = Vector2f(event->x, event->y);
  // "moving" mouse-down, updated with dragpoint on mouse move:
  m_dragStart = Vector2f(event->x, event->y);
  m_arcBall->click (event->x, event->y, &m_transform);
  // on button 1 with shift/ctrl, if there is an object, select it (for dragging)
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
  //dragging = false;
  if (event->state & GDK_SHIFT_MASK || event->state & GDK_CONTROL_MASK)  {
    // move/rotate object
    get_model()->ModelChanged();
    queue_draw();
  }
  else {
    if (m_downPoint.x() == event->x && m_downPoint.y() == event->y) {// click only
      if (event-> button == 3) { // right button -> popup menu?
	//cerr << "menu" << endl;
      } else {
	guint index = find_object_at(event->x, event->y);
	if (index) {
	  Gtk::TreeModel::iterator iter = get_model()->objtree.find_stl_by_index(index);
	  if (iter) {
	    if (event->button == 1)  {
	      m_selection->unselect_all();
	    }
	    if (m_selection->is_selected(iter))
	      m_selection->unselect(iter);
	    else
	      m_selection->select(iter);
	  }
	}
	// click on no object -> clear the selection:
	else if (event->button == 1)  {
	  m_selection->unselect_all();
	}
      }
    }
  }
  return true;
}

bool Render::on_scroll_event(GdkEventScroll* event) {
  cout << "Render::on_scroll_event" << endl;
  double factor = 110.0/100.0;
  if (event->direction == GDK_SCROLL_UP)
    m_zoom /= factor;
  else
    m_zoom *= factor;
  queue_draw();
  return true;
}

const double drag_factor = 0.3;

bool Render::on_motion_notify_event(GdkEventMotion* event) {
  bool redraw=true;
  const Vector2f dragp(event->x, event->y);
  const Vector2f delta = dragp - m_dragStart;
  const Vector3d delta3f(delta.x()*drag_factor, -delta.y()*drag_factor, 0);

  const Vector3d mouse_preview = mouse_on_plane(event->x, event->y,
						get_model()->get_preview_Z());
  get_model()->setMeasuresPoint(mouse_preview);

  if (event->state & GDK_BUTTON1_MASK) { // move or rotate
    if (event->state & GDK_SHIFT_MASK) { // move object XY
      vector<Shape*> shapes;
      vector<TreeObject*>objects;
      if (!m_view->get_selected_objects(objects, shapes))
	return true;
      const Vector3d mouse_down_plat = mouse_on_plane(m_dragStart.x(), m_dragStart.y());
      const Vector3d mousePlat  = mouse_on_plane(event->x, event->y);
      const Vector2d mouse_xy   = Vector2d(mousePlat.x(), mousePlat.y());
      const Vector2d deltamouse = mouse_xy - Vector2d(mouse_down_plat.x(), mouse_down_plat.y());
      const Vector3d movevec(deltamouse.x(), deltamouse.y(), 0.);
      if (shapes.size()>0)
	for (uint s=0; s<shapes.size(); s++) {
	  shapes[s]->transform3D.move(movevec);
	}
      else
	for (uint o=0; o<objects.size(); o++) {
	  objects[o]->transform3D.move(movevec);
	}
      m_dragStart = dragp;
    }
    else if (event->state & GDK_CONTROL_MASK) { // move object Z wise
      const Vector3d delta3fz(0, 0, -delta.y()*drag_factor);
      vector<Shape*> shapes;
      vector<TreeObject*>objects;
      if (!m_view->get_selected_objects(objects, shapes))
	return true;
      if (shapes.size()>0)
	for (uint s=0; s<shapes.size(); s++) {
	  Transform3D &transf = shapes[s]->transform3D;
	  double scale = transf.transform[3][3];
	  Vector3d movevec = delta3fz*scale;
	  transf.move(movevec);
	}
      else
	for (uint o=0; o<objects.size(); o++) {
	  Transform3D &transf = objects[o]->transform3D;
	  const double scale = transf.transform[3][3];
	  const Vector3d movevec = delta3fz*scale;
	  transf.move(movevec);
	}
      m_dragStart = dragp;
    }
    else { // rotate view
      m_arcBall->dragAccumulate(event->x, event->y, &m_transform);
    }
    if (redraw) queue_draw();
    return true;
  } // BUTTON 1
  else {
    if (event->state & GDK_BUTTON2_MASK) { // zoom
      const double factor = 1.0 + 0.01 * (delta.y() - delta.x());
      if (event->state & GDK_SHIFT_MASK) { // scale shape
	vector<Shape*> shapes;
	vector<TreeObject*>objects;
	if (!m_view->get_selected_objects(objects, shapes))
	  return true;
	if (shapes.size()>0) {
	  for (uint s=0; s<shapes.size(); s++) {
	    shapes[s]->Scale(shapes[s]->getScaleFactor()/factor, false);
	  }
	  m_view->update_scale_value();
	}
      } else { // zoom view
	m_zoom *= factor;
      }
      m_dragStart = dragp;
    } // BUTTON 2
    else if (event->state & GDK_BUTTON3_MASK) {
      if (event->state & GDK_SHIFT_MASK ||
	  event->state & GDK_CONTROL_MASK ) {  // rotate shape
	vector<Shape*> shapes;
	vector<TreeObject*>objects;
	Vector3d axis;
	if (event->state & GDK_CONTROL_MASK)  // rotate  z wise
	  axis = Vector3d(0,0,delta.x());
	else
	  axis = Vector3d(delta.y(), delta.x(), 0); // rotate strange ...
	m_view->rotate_selection(axis, delta.length()/100.);
	m_dragStart = dragp;
      } else {  // move view XY  / pan
	moveArcballTrans(m_transform, delta3f);
	m_dragStart = dragp;
      }
    } // BUTTON 3
    if (redraw) queue_draw();
    return true;
  }
  if (redraw) queue_draw();
  return Gtk::Widget::on_motion_notify_event (event);
}

void Render::CenterView() {
  /* FIXME */
  /* Vector3d c = get_model()->GetViewCenter(); */
}

guint Render::find_object_at(gdouble x, gdouble y) {
  /* FIXME */
  
  return 0;
}

Vector3d Render::mouse_on_plane(double x, double y, double plane_z) const {
  /* FIXME */

  return {0, 0, 0};
}
