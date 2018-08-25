/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2010  Kulitorum
    Copyright (C) 2011  Michael Meeks <michael.meeks@novell.com>
    Copyright (C) 2012  martin.dieringer@gmx.de
    Copyright (C) 2018  Paul Maurer

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

#include <stdlib.h>

#include <ctype.h>
#include <math.h>
#include <string.h>

#include "files.h"
#include "gcode.h"
#include "geometry.h"
#include "model.h"

GCode::GCode(void) {
  buffer = Gtk::TextBuffer::create();
}

void GCode::clear(void) {
  cmds.clear();
  layers.clear();
}

static double len3(double x, double y, double z) {
  return sqrt(x * x + y * y + z * z);
}

static double len2(double x, double y) {
  return sqrt(x * x + y * y);
}

static double norm2(Vector2d &v) {
  return len2(v.x(), v.y());
}

void GCode::ParseCmd(const char *str, GCodeCmd &cmd, printer_state &state, double &max_feedrate, double &home_feedrate) {
  double codes[256];
  char letter, *stop;
  double num;
  Vector3d pos = state.pos;
  Vector3d dest = state.pos;
  Vector3d center = state.pos;
  double radius = 0.0;
  double angle;
  double ext_end = state.ext;
  
  memset(codes, 0xFF, sizeof(codes)); /* All NaN */

  while (*str != '\0') {
    while (isspace(*str)) {
      str++;
    }
    
    letter = *str++;
    if (letter == ';')
      break;
    num = strtod(str, &stop);
    str = stop;
    codes[(unsigned char) letter] = num;
  }
  
  if (isfinite(codes['F']))
    // Feedrate is specified per minute: convert to per second
    state.feedrate = codes['F'] * state.scale / 60;

  cmd.spec_xy = 0;
  if (isfinite(codes['X'])) {
    if (state.is_rel)
      dest[0] += codes['X'] * state.scale;
    else
      dest[0] = codes['X'] * state.scale + state.offset[0];
    cmd.spec_xy = 1;
  }
  
  if (isfinite(codes['Y'])) {
    if (state.is_rel)
      dest[1] += codes['Y'] * state.scale;
    else
      dest[1] = codes['Y'] * state.scale + state.offset[1];
    cmd.spec_xy = 1;
  }

  cmd.spec_z = 0;
  if (isfinite(codes['Z'])) {
    if (state.is_rel)
      dest[2] += codes['Z'] * state.scale;
    else
      dest[2] = codes['Z'] * state.scale + state.offset[2];
    cmd.spec_z = 1;
  }
  
  if (isfinite(codes['I']))
    center[0] += codes['I'] * state.scale;
  
  if (isfinite(codes['J']))
    center[1] += codes['J'] * state.scale;
  
  if (isfinite(codes['K']))
    center[2] += codes['K'] * state.scale;
  
  if (isfinite(codes['R']))
    radius = codes['R'] * state.scale;
  
  cmd.spec_e = 0;
  if (isfinite(codes['E'])) {
    if (state.is_e_rel)
      ext_end += codes['E'] * state.scale;
    else
      ext_end = codes['E'] * state.scale;
    cmd.spec_e = 1;
  }
  
  if (isfinite(codes['T']))
    state.e_no = codes['T'];
  
  cmd.type = other;
  cmd.e_no = state.e_no;
  cmd.start = pos;
  cmd.stop = pos;
  cmd.center = {0, 0, 0};
  cmd.angle = 0;
  cmd.ccw = 0;
  cmd.length = 0;
  cmd.feedrate = state.feedrate;
  cmd.e_start = state.ext + state.ext_offset;
  cmd.e_stop = state.ext + state.ext_offset;
  cmd.layerno = state.layerno;
  
  if (isfinite('G')) {
    if (codes['G'] == 0.0) {
      // Rapid positioning
      cmd.type = line;
      cmd.stop = dest;
      cmd.e_stop = ext_end + state.ext_offset;
      state.pos = dest;
      state.ext = ext_end;
      cmd.feedrate = max_feedrate;
      cmd.length = len3(dest[0] - pos[0], dest[1] - pos[1], dest[2] - pos[2]);
    } else if (codes['G'] == 1.0) {
      // Linear
      cmd.type = line;
      cmd.stop = dest;
      cmd.e_stop = ext_end + state.ext_offset;
      state.pos = dest;
      state.ext = ext_end;
      cmd.length = len3(dest[0] - pos[0], dest[1] - pos[1], dest[2] - pos[2]);
    } else if (codes['G'] == 2.0 || codes['G'] == 3.0) {
      // Arc: 2 = CW, 3 = CCW
      cmd.type = arc;
      cmd.stop = dest;
      cmd.e_stop = ext_end + state.ext_offset;
      state.pos = dest;
      state.ext = ext_end;
      if (codes['G'] == 3.0)
	cmd.ccw = 1;
      
      if (isfinite(codes['R'])) {
	radius = codes['R'];
	Vector3d mid = (pos + dest) / 2.0;
	double dx = mid.x() - pos.x();
	double dy = mid.y() - pos.y();
	double dist = len2(dx, dy);
	if (dist >= radius) {
	  center = mid;
	  radius = dist;
	} else {
	  double len = sqrt(radius*radius - dist*dist);
	  double px = dy / dist;
	  double py = -dx / dist;
	  if (cmd.ccw) {
	    px = -px;
	    py = -py;
	  }
	  center[0] = mid[0] + px * len;
	  center[1] = mid[1] + py * len;
	  center[2] = mid[2];
	}
      } else {
	radius = len2(dest[0] - center[0], dest[1] - center[1]);
      }

      cmd.center = center;
      angle = CcwAngleBetween({dest[0]-center[0], dest[1]-center[1]},
			      {pos[0]-center[0], pos[1]-center[1]});

      if (!cmd.ccw)
	angle = -angle;
      
      if (angle <= 0) {
	angle += 2 * M_PI;
      }

      cmd.angle = angle;
      
      cmd.length = len2(radius * angle, dest[2] - pos[2]);
    } else if (codes['G'] == 4.0) {
      // Dwell
      double dtime = 0;
      if (isfinite(codes['P']))
	dtime += codes['P'] / 1000.0;

      if (isfinite(codes['S']))
	dtime += codes['S'];

      cmd.type = dwell;
      cmd.t_dwell = dtime;
    } else if (codes['G'] == 20.0) {
      state.scale = 25.4;
    } else if (codes['G'] == 21.0) {
      state.scale = 1.0;
    } else if (codes['G'] == 28.0) {
      // Home
      cmd.length = len3(dest[0] - pos[0], dest[1] - pos[1], dest[2] - pos[2]);
      cmd.feedrate = home_feedrate;
    } else if (codes['G'] == 90.0) {
      state.is_rel = 0;
      state.is_e_rel = 0; /* FIXME: Use firmware flavor */
    } else if (codes['G'] == 91.0) {
      state.is_rel = 1;
      state.is_e_rel = 1;
    } else if (codes['G'] == 92.0) {
      if (isfinite(codes['E'])) {
	state.ext_offset += state.ext - codes['E'];
	state.ext = codes['E'];
	cout << "Extruder reset, new offset = " << state.ext_offset << endl;
      }
    }
  } else if (isfinite(codes['M'])) {
    if (codes['M'] == 82.0) {
      state.is_e_rel = 0;
    } else if (codes['M'] == 83.0) {
      state.is_e_rel = 1;
    } else if (codes['M'] == 204.0) {
      if (isfinite(codes['P']))
	state.accel = codes['P'] * state.scale;
      if (isfinite(codes['S']))
	state.accel = codes['S'] * state.scale;
    }
  }
  
  cmd.jerk = state.jerk;
  cmd.accel = state.accel;
  cmd.center = center;
}

static void max_factors(Vector2d &max, double a, double b, double j) {
  if (fabs(a * max[0] - b * max[1]) < j) {
    // Max is valid
    return;
  }
  
  if (a * b <= 0) {
    // Reversing
    a = fabs(a);
    b = fabs(b);
    
    if (max[0] * a < j/2) {
      max[1] = (j - 2 * max[0] * a) / b;
      return;
    }

    if (max[1] * b < j/2) {
      max[0] = (j - 2 * max[1] * b) / a;
    }
    
    max[0] = j / (2 * a);
    max[1] = j / (2 * b);
    return;
  }
  
  // Same direction
  a = fabs(a);
  b = fabs(b);
  
  if (max[0] * a > max[1] * b) {
    max[0] = (max[1] * b + j) / a;
    return;
  }
  
  max[1] = (max[0] * a + j) / b;
}

static void junction_speed(Vector2d &ret, Vector2d &a, Vector2d &b, double jerk) {
  double jx = fabs(b.x() - a.x());
  double jy = fabs(b.y() - a.y());
  Vector2d max = {1.0, 1.0};
  
  if (jx > jy) {
    max_factors(max, a.x(), b.x(), jerk);
    max_factors(max, a.y(), b.y(), jerk);
    max_factors(max, a.x(), b.x(), jerk);
  } else {
    max_factors(max, a.y(), b.y(), jerk);
    max_factors(max, a.x(), b.x(), jerk);
    max_factors(max, a.y(), b.y(), jerk);
  }
  
  ret = {norm2(a)*max[0], norm2(b) * max[1]};
}

static void delta_vec(Vector2d &ret, Vector3d &start, Vector3d &stop) {
  ret = {stop.x() - start.x(), stop.y() - start.y()};
}

static void rotate_right90(Vector2d &vec) {
  vec = {vec.y(), -vec.x()};
}

void GCode::Velocities(GCodeCmd *cmd, Vector2d &start, Vector2d &stop) {
  if (cmd->type == line) {
    Vector2d vel;
    delta_vec(vel, cmd->start, cmd->stop);
    if (vel.x() != 0 || vel.y() != 0) {
      vel /= norm2(vel);
      vel *= cmd->feedrate;
    }
    
    start = stop = vel;
    return;
  }

  if (cmd->type == arc) {
    Vector2d vel;

    delta_vec(vel, cmd->center, cmd->start);
    rotate_right90(vel);
    if (vel.x() != 0 || vel.y() != 0) {
      vel /= norm2(vel);
      vel *= cmd->feedrate;
    }
    if (cmd->ccw)
      vel = -vel;
    start = vel;

    delta_vec(vel, cmd->center, cmd->stop);
    rotate_right90(vel);
    if (vel.x() != 0 || vel.y() != 0) {
      vel /= norm2(vel);
      vel *= cmd->feedrate;
    }
    if (cmd->ccw)
      vel = -vel;
    stop = vel;
    return;
  }

  if (cmd->type == dwell) {
    start = {0, 0};
    stop = {0, 0};
    return;
  }
}

void GCode::CalcTime(void) {
  Vector2d vstart2, vstop2, vstop1, j1, j2;
  
  if (cmds.size() == 0)
    return;

  GCodeCmd *cmd = &cmds[0];
  cmd->t_start = 0;
  Velocities(cmd, vstart2, vstop2);
  vstop1 = {0,0};
  junction_speed(j1, vstop1, vstart2, cmd->jerk);
  size_t prev = 0;
  size_t count = 1;
  vstop1 = vstop2;
  while (count <= cmds.size()) {
    while (count < cmds.size() && cmds[count].type == other)
      count++;
    
    if (count < cmds.size())
      Velocities(&cmds[count], vstart2, vstop2);
    else
      vstart2 = {0,0};
    junction_speed(j2, vstop1, vstart2, cmd->jerk);

    double time = 0;
    if (cmd->type == dwell)
      time = cmd->t_dwell;
    else {
      double start_speed = j1[1];
      double stop_speed = j2[0];
    
      double start_accel_time = fmax(0, (cmd->feedrate - start_speed)) / cmd->accel;
      double start_accel_len = (0.5 * cmd->accel * start_accel_time + start_speed) * start_accel_time;
      double stop_accel_time = fmax(0, (cmd->feedrate - stop_speed)) / cmd->accel;
      double stop_accel_len = (0.5 * cmd->accel * stop_accel_time + stop_speed) * stop_accel_time;
      double tot_accel_len = start_accel_len + stop_accel_len;
      if (cmd->length > tot_accel_len) {
	time = start_accel_time + stop_accel_time + (cmd->length - tot_accel_len) / cmd->feedrate;
      } else {
	double ds = start_speed - stop_speed;
	double aa = cmd->accel;
	double bb = 2 * (start_speed + stop_speed);
	double cc = -ds * ds / cmd->accel - 4 * cmd->length;
	double ss = sqrt(bb * bb - 4 * aa * cc);
	double t1 = (-bb - ss) / (2 * aa);
	double t2 = (-bb + ss) / (2 * aa);
	
	if (t1 >= 0 && t2 >= 0) {
	  if (t1 < t2)
	    time = t1;
	  else
	    time = t2;
	} else if (t1 >= 0) {
	  time = t1;
	} else if (t2 >= 0) {
	  time = t2;
	}
      }
    }
    
    cmd->t_stop = cmd->t_start + time;

    while (++prev < count)
      cmds[prev].t_start = cmds[prev].t_stop = cmd->t_stop;
    
    if (count < cmds.size()) {
      cmds[count].t_start = cmd->t_stop;
      cmd = &cmds[count];
    }
    
    vstop1 = vstop2;
    j1 = j2;
    prev = count;
    count++;
  }
}

void GCode::Parse(Model *model, ViewProgress *progress, istream &is) {
  printer_state state;
  GCodeCmd cmd;
  string s;
  stringstream alltext;
  
  /* Fixme: offset for machine_center_is_zero setting */
  clear();
  
  set_locales("C");
  
  double h = model->settings.get_double("Slicing", "LayerHeight");
  double home_feedrate = PS_AsFloat(model->settings.GetPrinter()->Get("repsnapper", "#global", "home_feedrate_x"));
  const Psv *dflt = model->settings.GetDflt();
  
  memset(&state, 0, sizeof(state));
  state.scale = 1.0;
  state.e_no = 0;
  
  double max_feedrate;
  try {
    max_feedrate = min(PS_AsFloat(dflt->GetThrow("#global", "machine_max_feedrate_x")),
		       PS_AsFloat(dflt->GetThrow("#global", "machine_max_feedrate_y"))); // mm/s
  } catch (exception &e) {
    cout << "Cannot determine printer max feedrate" << endl;
    max_feedrate = 200;
  }
  
  state.feedrate = max_feedrate;

  try {
    state.accel = min(PS_AsFloat(dflt->GetThrow("#global", "machine_max_acceleration_x")),
		      PS_AsFloat(dflt->GetThrow("#global", "machine_max_acceleration_y")));
  } catch (exception &e) {
    cout << "Cannot determine printer max acceleration" << endl;
    state.accel = 9000;
  }
  
  // jerk = min speed change that requires acceleration
  try {
    state.jerk = PS_AsFloat(dflt->GetThrow("#global", "machine_max_jerk_xy")); // mm/s
  } catch (exception &e) {
    cout << "Cannot determine printer max jerk" << endl;
    state.jerk = 10;
  }
  
  state.offset = {0, 0, 0};
  if (PS_AsBoolean(dflt->Get("#global", "machine_center_is_zero"))) {
    Vector3d vol = model->settings.getPrintVolume();
    state.offset.x() = vol.x() / 2;
    state.offset.y() = vol.y() / 2;
  }
  
  cmds.clear();
  while (getline(is,s)) {
    alltext << s << endl;
    
    ParseCmd(s.c_str(), cmd, state, max_feedrate, home_feedrate);
    if (fabs(cmd.stop.z() - state.layer_z) > 0.99 * h && cmd.spec_e && cmd.spec_xy) {
      // New layer
      state.layer_z = cmd.stop.z();
      if (layers.empty()) {
	layers.push_back(0);
      } else {
	layers.push_back(cmds.size());
	cmd.layerno = ++state.layerno;
      }
    }
    cmds.push_back(cmd);
  }
  reset_locales();
  
  CalcTime();
  
  buffer->set_text(alltext.str());
  model->m_signal_gcode_changed.emit();
  
  if (!cmds.empty()) {
    double tot = cmds.back().t_stop;
    int hr = floor(tot / 3600);
    int min = fmod(floor(tot / 60), 60);
    double sec = fmod(tot, 60);
    cout << "Print time: " << hr << "h " << min << "m " << sec << "s" << endl;
    cout << "Filament used: " << cmds.back().e_stop / 1000.0 << " m" << endl;
  } else {
    cout << "Gcode was empty" << endl;
  }
}

void GCode::Read(Model *model, ViewProgress *progress, string filename) {
  ifstream file;
  file.open(filename.c_str());		//open a file
  Parse(model, progress, file);
  file.close();
}

void GCode::draw(Render &render, const Settings &settings,
		 int layer, bool liveprinting,
		 int linewidth) {
  if (layer < 0)
    layer = 0;

  if (cmds.empty())
    return;
  
  drawCommands(render, settings,
	       getLayerStart(layer), getLayerEnd(layer) + 1,
	       liveprinting, linewidth);
}

void GCode::addSeg(RenderVert &vert, const GCodeCmd *cmd) {
  if (cmd->type == line) {
    vert.add(cmd->start);
    vert.add(cmd->stop);
    return;
  }

  if (cmd->type != arc)
    return;
  
  // Arc
  Vector3d lastPos = cmd->start;
  Vector3d center = cmd->center;
  double angle = cmd->angle;
  double dz = cmd->stop.z() - cmd->start.z();
  int ccw = cmd->ccw;
  
  Vector3d arcpoint;
  Vector3d radiusv = lastPos-center;
  radiusv.z() = 0;
  double astep = angle/radiusv.length()/30.;
  if (angle/astep > 10000) astep = angle/10000;
  if (angle<0) ccw=!ccw;
  Vector3d axis(0.,0.,ccw?1.:-1.);
  double startZ = lastPos.z();
  for (long double a = 0; abs(a) < abs(angle); a+=astep) {
    arcpoint = center + radiusv.rotate(a, axis);
    if (dz!=0 && angle!=0) arcpoint.z() = startZ + dz*a/angle;
    vert.add(lastPos);
    vert.add(arcpoint);
    lastPos = arcpoint;
  }
}

void GCode::drawCommands(Render &render,
			 const Settings &settings, uint start, uint end,
			 bool liveprinting, int linewidth) {
  size_t count;
  GCodeCmd *cmd;
  RenderVert vert;
  
  //cout << "Drawing gcode from " << start << " to " << end << endl;
  
  if (end > cmds.size())
    end = cmds.size();
  
  for (count = start; count < end; count++) {
    cmd = &cmds[count];
    if (cmd->spec_e)
      continue;

    addSeg(vert, cmd);
  }
  render.draw_lines(settings.get_colour("Display","GCodeMoveColor"), vert, 1.0);

  vert.clear();
  float *color;
  if (liveprinting) {
    for (count = start; count < end; count++) {
      cmd = &cmds[count];
      if (!cmd->spec_e)
	continue;
      
      addSeg(vert, cmd);
    }
    
    color = settings.get_colour("Display","GCodePrintingColor");
    render.draw_lines(color, vert, linewidth);
  } else {
    int num = settings.getNumExtruders();
    for (int e_no = 0; e_no < num; e_no++) {
      for (count = start; count < end; count++) {
	cmd = &cmds[count];
	if (!cmd->spec_e)
	  continue;
	if (cmd->e_no != e_no)
	  continue;

	addSeg(vert, cmd);
      }
      
      string colorname = "E" + to_string(e_no % 5) + "Color";
      color = settings.get_colour("Display", colorname);
      render.draw_lines(color, vert, linewidth);
    }
  }
}

double GCode::GetTotalExtruded(void) const {
  if (cmds.empty())
    return 0.0;
  
  return cmds.back().e_stop;
}

double GCode::GetTimeEstimation(void) const {
  if (cmds.empty())
    return 0.0;
    
  return cmds.back().t_stop;
}

int GCode::getLayerNo(const double z) const {
  size_t i;
  
  if (layers.empty())
    return 0;
  
  for (i = 0; i < layers.size(); i++) {
    if (cmds[layers[i]].start.z() >= z)
      break;
  }

  // cout << "Searching for layer at z = " << z << ", found " << i << endl;
  // cout << "Length layers = " << layers.size() << endl;
  
  return i;
}

int GCode::getLayerNo(const unsigned long commandno) const {
  if (cmds.empty())
    return 0;
  
  if (commandno >= cmds.size())
    return layers.size();
  
  return cmds[commandno].layerno;
}

size_t GCode::getLayerStart(const uint layerno) const {
  if (layers.empty())
    return 0;

  if (layerno >= layers.size())
    return cmds.size();

  return layers[layerno];
}

size_t GCode::getLayerEnd(const uint layerno) const {
  return getLayerStart(layerno + 1);
}

double GCode::timeLeft(const size_t commandno) const {
  if (cmds.empty() || commandno >= cmds.size())
    return 0.0;
  
  return cmds.back().t_stop - cmds[commandno].t_start;
}

double GCode::percentDone(const size_t commandno) const {
  if (cmds.empty() || commandno >= cmds.size())
    return 100.0;
  
  return cmds[commandno].t_start / cmds.back().t_stop * 100.0;
}

double GCode::max_z(void) const {
  ssize_t count;

  for (count = cmds.size() - 1; count >= 0; count--) {
    if (cmds[count].spec_e)
      return cmds[count].stop.z();
  }
  
  return 0.0;
}
