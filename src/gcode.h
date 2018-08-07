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

#pragma once

#include <gtkmm.h>

#include <sstream>
#include <vector>

#include "stdafx.h"
#include "ui/render.h"

class GCode {
 private:
  enum CmdType {
    line,
    arc,
    dwell,
    other
  };
  
  class GCodeCmd {
  public:
    enum CmdType type;
    int e_no;
    int spec_e;
    int spec_xy;
    int spec_z;
    Vector3d start;
    Vector3d stop;
    Vector3d center;
    double angle;
    int ccw;
    double e_start;
    double e_stop;
    double t_start;
    double t_stop;
    size_t layerno;
  };

  class printer_state {
  public:
    Vector3d pos;
    double layer_z;
    double ext;
    double ext_offset;
    double feedrate;
    double accel;
    double jerk;
    double scale;
    double time;
    int e_no;
    int is_rel;
    int is_e_rel;
    size_t layerno;
  };

  vector<GCodeCmd> cmds;
  vector<size_t> layers; // First command at each layer
  
  static void ParseCmd(const char *str, GCodeCmd &cmd, printer_state &state, double &max_feedrate, double &home_feedrate);
  static void addSeg(RenderVert &vert, const GCodeCmd *cmd);
  
  Glib::RefPtr<Gtk::TextBuffer> buffer;
 public:
  GCode(void);

  bool empty(void) const {return cmds.empty();};
  void clear(void);
  
  void Parse(Model *model, ViewProgress *progress, istream &is);
  
  void Read(Model *model, ViewProgress *progress, string filename);
  
  void draw(Render &render,
	    const Settings &settings,
	    int layer=-1, bool liveprinting=false,
	    int linewidth=3);
  
  void drawCommands(Render &render,
		    const Settings &settings, uint start, uint end,
		    bool liveprinting, int linewidth);
  
  double GetTotalExtruded(void) const;
  double GetTimeEstimation(void) const;

  int getLayerNo(const double z) const;
  int getLayerNo(const unsigned long commandno) const;
  size_t getLayerStart(const uint layerno) const;
  size_t getLayerEnd(const uint layerno) const;

  double timeLeft(const size_t commandno) const;
  double percentDone(const size_t commandno) const;
  
  double max_z(void) const;
  
  std::string get_text() const {return buffer->get_text();};
  Glib::RefPtr<Gtk::TextBuffer> get_buffer() const {return buffer;};
};
