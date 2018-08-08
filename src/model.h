/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2010  Kulitorum
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

#include <math.h>

#include <giomm/file.h>

#include "stdafx.h"

#include "files.h"


#include "objtree.h"
#include "gcode.h"
#include "settings.h"
#include "ui/render.h"

#ifdef _MSC_VER // Visual C++ compiler
#  pragma warning( disable : 4244 4267)
#endif

class Model {
  ViewProgress *m_progress;
  
 public:
  Gtk::Statusbar *statusbar;
  // Something in the rfo changed
  sigc::signal< void > m_signal_gcode_changed;
  
  Model();
  ~Model();
  
  void SaveConfig(Glib::RefPtr<Gio::File> file);
  void LoadConfig() { LoadConfig(Gio::File::create_for_path("repsnapper3.conf")); }
  void LoadConfig(Glib::RefPtr<Gio::File> file);
  
  // STL Functions
  void ReadStl(Glib::RefPtr<Gio::File> file, int extruder = -1);
  vector<Shape*> ReadShapes(Glib::RefPtr<Gio::File> file,
			    uint max_triangles = 0,
			    int extruder = -1);
  void SaveStl(Glib::RefPtr<Gio::File> file);
  
  int AddShape(TreeObject *parent, Shape * shape, string filename,
	       bool autoplace = true);
  int SplitShape(TreeObject *parent, Shape *shape, string filename);
  int MergeShapes(TreeObject *parent, const vector<Shape*> shapes);
  Shape GetCombinedShape(int extruder = -1) const;
  
  sigc::signal< void, Gtk::TreePath & > m_signal_stl_added;

  void Read(Glib::RefPtr<Gio::File> file, int extruder = -1);
  void SetViewProgress (ViewProgress *progress);

  void DeleteObjTree(vector<Gtk::TreeModel::Path> &iter);
  vector<Gtk::TreeModel::Path> m_current_selectionpath;

  void ScaleObject(Shape *shape, TreeObject *object, double scale);
  void ScaleObjectX(Shape *shape, TreeObject *object, double scale);
  void ScaleObjectY(Shape *shape, TreeObject *object, double scale);
  void ScaleObjectZ(Shape *shape, TreeObject *object, double scale);
  void RotateObject(Shape *shape, TreeObject *object, Vector4d rotate);
  void PlaceOnPlatform(Shape *shape, TreeObject *object);
  bool updateStatusBar(GdkEventCrossing *event, Glib::ustring = "");
  void Mirror(Shape *shape, TreeObject *object);

  vector<Shape*> preview_shapes;
  double get_preview_Z() {return 0.0;};

  // GCode Functions
  void init();
  void ReadGCode(Glib::RefPtr<Gio::File> file);
  void ConvertToGCode();

  void MakeRaft(GCodeState &state, double &z);
  void WriteGCode(Glib::RefPtr<Gio::File> file);
  void ClearGCode();
  Glib::RefPtr<Gtk::TextBuffer> GetGCodeBuffer();
  void GlDrawGCode(Render &render, int layer=-1); // should be in the view
  void GlDrawGCode(Render &render, double z);
  void setCurrentPrintingLine(long line){ currentprintingline = line; }
  unsigned long currentprintingline;

  Matrix4f &SelectedNodeMatrix(guint objectNr = 1);
  void SelectedNodeMatrices(vector<Matrix4d *> &result );
  void newObject();

  Settings settings;

  // Model derived: Bounding box info
  Vector3d Center;
  Vector3d Min;
  Vector3d Max;

  void CalcBoundingBoxAndCenter(bool selected_only = false);
  Vector3d GetViewCenter();
  Vector3d FindEmptyLocation(const vector<Shape*> &shapes,
			     const vector<Matrix4d> &transforms,
			     const Shape *shape);
  bool FindEmptyLocation(Vector3d &result, const Shape *stl);

  sigc::signal< void > m_model_changed;
  void ModelChanged();
  bool m_inhibit_modelchange;
  // Truly the model
  ObjectsTree objtree;
  Glib::RefPtr<Gtk::TextBuffer> errlog, echolog;

  int drawShapes(Render &render, vector<Gtk::TreeModel::Path> &selected);
  int drawBBoxes(Render &render);

  sigc::signal< void, Gtk::MessageType, const char *, const char * > signal_alert;
  void alert (const char *message);
  void error (const char *message, const char *secondary);

  void ClearLogs();

  GCode gcode;

  void SetIsPrinting(bool printing) { is_printing = printing; };

  bool isCalculating() const { return is_calculating; };
 private:
  bool is_calculating;
  bool is_printing;
};
