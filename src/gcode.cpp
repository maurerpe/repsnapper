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
  
  if (isnormal(codes['F']))
    state.feedrate = codes['F'] * state.scale;
  
  if (isnormal(codes['X'])) {
    if (state.is_rel)
      dest[0] += codes['X'] * state.scale;
    else
      dest[0] = codes['X'] * state.scale;
  }
  
  if (isnormal(codes['Y'])) {
    if (state.is_rel)
      dest[1] += codes['Y'] * state.scale;
    else
      dest[1] = codes['Y'] * state.scale;
  }

  cmd.spec_z = 0;
  if (isnormal(codes['Z'])) {
    if (state.is_rel)
      dest[2] += codes['Z'] * state.scale;
    else
      dest[2] = codes['Z'] * state.scale;
    cmd.spec_z = 1;
    state.layerno++;
  }
  
  if (isnormal(codes['I']))
    center[0] += codes['I'] * state.scale;
  
  if (isnormal(codes['J']))
    center[1] += codes['J'] * state.scale;
  
  if (isnormal(codes['K']))
    center[2] += codes['K'] * state.scale;
  
  if (isnormal(codes['R']))
    radius = codes['R'] * state.scale;
  
  cmd.spec_e = 0;
  if (isnormal(codes['E'])) {
    if (state.is_e_rel)
      ext_end += codes['E'] * state.scale;
    else
      ext_end = codes['E'] * state.scale;
    cmd.spec_e = 1;
  }
  
  if (isnormal(codes['T']))
    state.e_no = codes['T'];
  
  this_feed = state.feedrate;
  cmd.type = other;
  cmd.e_no = state.e_no;
  cmd.start = pos;
  cmd.stop = pos;
  cmd.ccw = 0;
  cmd.e_start = state.ext;
  cmd.e_stop = state.ext;
  cmd.t_start = state.time;
  cmd.layerno = state.layerno;
  
  if (isnormal('G')) {
    if (codes['G'] == 0.0) {
      // Rapid positioning
      cmd.type = line;
      cmd.stop = dest;
      cmd.e_stop = ext_end;
      state.pos = dest;
      state.ext = ext_end;
      this_feed = max_feedrate;
      length = len3(dest[0] - pos[0], dest[1] - pos[1], dest[2] - pos[2]);
    } else if (codes['G'] == 1.0) {
      // Linear
      cmd.type = line;
      cmd.stop = dest;
      cmd.e_stop = ext_end;
      state.pos = dest;
      state.ext = ext_end;
      length = len3(dest[0] - pos[0], dest[1] - pos[1], dest[2] - pos[2]);
    } else if (codes['G'] == 2.0) {
      // CW Arc
      cmd.type = arc;
      cmd.stop = dest;
      cmd.e_stop = ext_end;
      state.pos = dest;
      state.ext = ext_end;
      if (isnormal(codes['R'])) {
	/* FIXME: Allow calculation of center from radius */
      } else {
	radius = len2(dest[0] - center[0], dest[1] - center[1]);
      }

      angle = planeAngleBetween({pos[0]-center[0], pos[1]-center[1]},
				{dest[0]-center[0], dest[1]-center[1]});
      if (angle == 0) {
	angle = 2 * M_PI;
      }
      
      length = len2(radius * angle, dest[2] - pos[2]);
    } else if (codes['G'] == 3.0) {
      // CCW Arc
      cmd.type = arc;
      cmd.stop = dest;
      cmd.e_stop = ext_end;
      cmd.ccw = 1;
      state.pos = dest;
      state.ext = ext_end;
      if (isnormal(codes['R'])) {
	/* FIXME: Allow calculation of center from radius */
      } else {
	radius = len2(dest[0] - center[0], dest[1] - center[1]);
      }

      angle = planeAngleBetween({dest[0]-center[0], dest[1]-center[1]},
				{pos[0]-center[0], pos[1]-center[1]});
      if (angle == 0) {
	angle = 2 * M_PI;
      }
      
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
    }
  } else if (isnormal(codes['M'])) {
    if (codes['M'] == 82.0) {
      state.is_e_rel = 0;
    } else if (codes['M'] == 83.0) {
      state.is_e_rel = 1;
    } else if (codes['M'] == 204.0) {
      if (isnormal(codes['P']))
	state.accel = codes['P'] * state.scale;
      if (isnormal(codes['S']))
	state.accel = codes['S'] * state.scale;
    }
  }

  /* FIXME: Factor acceleration into time */
  state.time += length / this_feed * 60;
  
  cmd.center = center;
  cmd.t_stop = state.time;
}

void GCode::Parse(Model *model, const vector<char> E_letters,
		  ViewProgress *progress, istream &is) {
  printer_state state;
  GCodeCmd cmd;
  string s;
  stringstream alltext;
  
  clear();

  set_locales("C");

  memset(&state, 0, sizeof(state));
  /* FIXME: Get initial values from printer defaults */
  state.feedrate = 3600;
  state.accel = 1000;
  state.scale = 1.0;
  double max_feedrate = 6000;
  double home_feedrate = 1200;
  
  while(getline(is,s)) {
    alltext << s << endl;
    
    ParseCmd(s.c_str(), cmd, state, max_feedrate, home_feedrate);
    if (cmd.spec_z) {
      layers.push_back(cmds.size());
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

void GCode::Read(Model *model, const vector<char> E_letters,
		 ViewProgress *progress, string filename) {
  ifstream file;
  file.open(filename.c_str());		//open a file
  Parse(model, E_letters, progress, file);
  file.close();
}

void GCode::draw(const Settings &settings,
		 int layer, bool liveprinting,
		 int linewidth) {
  if (layer < 0)
    layer = 0;

  if (cmds.empty())
    return;
  
  drawCommands(settings,
	       getLayerStart(layer), getLayerEnd(layer) + 1,
	       liveprinting, linewidth);
}

void GCode::drawCommands(const Settings &settings, uint start, uint end,
			 bool liveprinting, int linewidth) {
  size_t count;
  GCodeCmd *cmd;

  //cout << "Drawing gcode from " << start << " to " << end << endl;
  
  if (end > cmds.size())
    end = cmds.size();
  
  glEnable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glDisable(GL_LIGHTING);

  if (start < cmds.size()) {
    glPointSize(20);
    glBegin(GL_POINTS);
    glVertex3dv((GLdouble*)&cmds[start].start);
    glEnd();
  }
  
  /* FIXME: Interpolate arcs */
  glLineWidth(1);
  glColor4fv(settings.get_colour("Display","GCodeMoveColour"));
  glBegin(GL_LINES);
  for (count = start; count < end; count++) {
    cmd = &cmds[count];
    if (cmd->spec_e)
      continue;
    
    glVertex3dv((GLdouble*)&cmd->start);
    glVertex3dv((GLdouble*)&cmd->stop);
    
  }
  glEnd();

  /* FIXME: More colors for multiple extruders */
  /* FIXME: Interpolate arcs */
  glLineWidth(linewidth);
  if (liveprinting) {
    glColor4fv(settings.get_colour("Display","GCodePrintingColour"));
  } else {
    string extrudername =
      settings.numberedExtruder("Extruder", 0);
    glColor4fv(settings.get_colour(extrudername,"DisplayColour"));
  }
  glBegin(GL_LINES);
  for (count = start; count < end; count++) {
    cmd = &cmds[count];
    if (!cmd->spec_e)
      continue;
    
    glVertex3dv((GLdouble*)&cmd->start);
    glVertex3dv((GLdouble*)&cmd->stop);
  }
  glEnd();
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
  
  if (cmds.empty())
    return 0;
  
  for (i = 0; i < cmds.size(); i++) {
    if (cmds[i].start.z() >= z)
      break;
  }
  
  //cout << "Searching for layer at z = " << z << ", found " << i << endl;
  
  return cmds[i].layerno;
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
