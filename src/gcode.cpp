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

double len3(double x, double y, double z) {
  return sqrt(x * x + y * y + z * z);
}

double len2(double x, double y) {
  return sqrt(x * x + y * y);
}

void GCode::ParseCmd(const char *str, GCodeCmd &cmd, printer_state &state, double &max_feedrate, double &home_feedrate) {
  double codes[256];
  char letter, *stop;
  double num;
  Vector3d pos = state.pos;
  Vector3d dest = state.pos;
  Vector3d center = state.pos;
  double radius = 0.0;
  double length = 0.0;
  double this_feed;
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
      dest[0] = codes['X'] * state.scale;
    cmd.spec_xy = 1;
  }
  
  if (isfinite(codes['Y'])) {
    if (state.is_rel)
      dest[1] += codes['Y'] * state.scale;
    else
      dest[1] = codes['Y'] * state.scale;
    cmd.spec_xy = 1;
  }

  cmd.spec_z = 0;
  if (isfinite(codes['Z'])) {
    if (state.is_rel)
      dest[2] += codes['Z'] * state.scale;
    else
      dest[2] = codes['Z'] * state.scale;
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
    state.e_no = codes['T'] + 1;
  
  this_feed = state.feedrate;
  cmd.type = other;
  cmd.e_no = state.e_no;
  cmd.start = pos;
  cmd.stop = pos;
  cmd.center = {0, 0, 0};
  cmd.angle = 0;
  cmd.ccw = 0;
  cmd.e_start = state.ext + state.ext_offset;
  cmd.e_stop = state.ext + state.ext_offset;
  cmd.t_start = state.time;
  cmd.layerno = state.layerno;
  
  if (isfinite('G')) {
    if (codes['G'] == 0.0) {
      // Rapid positioning
      cmd.type = line;
      cmd.stop = dest;
      cmd.e_stop = ext_end + state.ext_offset;
      state.pos = dest;
      state.ext = ext_end;
      this_feed = max_feedrate;
      length = len3(dest[0] - pos[0], dest[1] - pos[1], dest[2] - pos[2]);
    } else if (codes['G'] == 1.0) {
      // Linear
      cmd.type = line;
      cmd.stop = dest;
      cmd.e_stop = ext_end + state.ext_offset;
      state.pos = dest;
      state.ext = ext_end;
      length = len3(dest[0] - pos[0], dest[1] - pos[1], dest[2] - pos[2]);
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
      
      length = len2(radius * angle, dest[2] - pos[2]);
    } else if (codes['G'] == 20.0) {
      state.scale = 25.4;
    } else if (codes['G'] == 21.0) {
      state.scale = 1.0;
    } else if (codes['G'] == 28.0) {
      // Home
      length = len3(dest[0] - pos[0], dest[1] - pos[1], dest[2] - pos[2]);
      this_feed = home_feedrate;
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

  /* FIXME: Factor jerk into time */
  double accel_time = this_feed / state.accel;
  double accel_len = 0.5 * state.accel * accel_time * accel_time;
  double time = 0;
  if (length > 2 * accel_len) {
    time = 2 * accel_time + (length - 2 * accel_len) / this_feed;
  } else {
    time = 2 * sqrt(length / state.accel);
  }
  
  state.time += time;
  cmd.center = center;
  cmd.t_stop = state.time;
}

void GCode::Parse(Model *model, ViewProgress *progress, istream &is) {
  printer_state state;
  GCodeCmd cmd;
  string s;
  stringstream alltext;
  
  clear();
  
  set_locales("C");
  
  double h = model->settings.get_double("Slicing", "LayerHeight");
  double home_feedrate = model->settings.get_double("Hardware", "HomeSpeed.XY"); // mm/s
  const Psv *dflt = model->settings.GetDflt();
  
  memset(&state, 0, sizeof(state));
  state.scale = 1.0;
  state.e_no = 1;
  
  double max_feedrate;
  try {
    max_feedrate = min(PS_AsFloat(dflt->Get("#global", "machine_max_feedrate_x")),
		       PS_AsFloat(dflt->Get("#global", "machine_max_feedrate_y"))); // mm/s
  } catch (exception &e) {
    cout << "Cannot determine printer max feedrate" << endl;
    max_feedrate = 200;
  }
  
  state.feedrate = max_feedrate;

  try {
    state.accel = min(PS_AsFloat(dflt->Get("#global", "machine_max_acceleration_x")),
		      PS_AsFloat(dflt->Get("#global", "machine_max_acceleration_y")));
  } catch (exception &e) {
    cout << "Cannot determine printer max acceleration" << endl;
    state.accel = 9000;
  }
  
  // jerk = min speed change that requires acceleration
  try {
    state.jerk = PS_AsFloat(dflt->Get("#global", "machine_max_jerk_xy")); // mm/s
  } catch (exception &e) {
    cout << "Cannot determine printer max jerk" << endl;
    state.jerk = 10;
  }
  
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
  
  buffer->set_text(alltext.str());
  model->m_signal_gcode_changed.emit();

  if (!cmds.empty()) {
    double tot = cmds.back().t_stop;
    int hr = floor(tot / 3600);
    int min = fmod(floor(tot / 60), 60);
    double sec = fmod(tot, 60);
    cout << "Print time: " << hr << "h " << min << "m " << sec << "s" << endl;
    cout << "Filament used: " << cmds.back().e_stop / 1000.0 << " m" << endl;
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
    for (int e_no = 1; e_no <= num; e_no++) {
      for (count = start; count < end; count++) {
	cmd = &cmds[count];
	if (!cmd->spec_e)
	  continue;
	if (cmd->e_no != e_no)
	  continue;

	addSeg(vert, cmd);
      }
      
      string colorname = "E" + to_string(((e_no - 1) % 5) + 1) + "Color";
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
