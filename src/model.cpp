/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2011 Michael Meeks
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

#define  MODEL_IMPLEMENTATION
#include <vector>
#include <string>
#include <cerrno>
#include <functional>
#include <numeric>

#include "stdafx.h"
#include "model.h"
#include "objtree.h"
#include "settings.h"
#include "ui/progress.h"
#include "ui/render.h"
#include "shape.h"

Model::Model() :
  currentprintingline(0),
  settings(),
  Min(), Max(),
  m_inhibit_modelchange(false),
  errlog (Gtk::TextBuffer::create()),
  echolog (Gtk::TextBuffer::create()),
  is_calculating(false),
  is_printing(false) {
  // Variable defaults
  Center.set(100.,100.,0.);
  preview_shapes.clear();
}

Model::~Model() {
  ClearGCode();
  preview_shapes.clear();
}

void Model::alert (const char *message) {
  signal_alert.emit(Gtk::MESSAGE_INFO, message, NULL);
}

void Model::error (const char *message, const char *secondary) {
  signal_alert.emit(Gtk::MESSAGE_ERROR, message, secondary);
}

void Model::SaveConfig(Glib::RefPtr<Gio::File> file) {
  settings.save_settings(file);
}

void Model::LoadConfig(Glib::RefPtr<Gio::File> file) {
  settings.load_settings(file);
  ModelChanged();
}

void Model::SetViewProgress (ViewProgress *progress) {
  m_progress = progress;
}

void Model::ClearGCode() {
  gcode.clear();
}

Glib::RefPtr<Gtk::TextBuffer> Model::GetGCodeBuffer() {
  return gcode.get_buffer();
}

void Model::GlDrawGCode(Render &render, int layerno) {
  if (settings.get_boolean("Display","DisplayGCode"))  {
    gcode.draw(render, settings, layerno, false);
  }
  // assume that the real printing line is the one at the start of the buffer
  if (currentprintingline > 0) {
    int currentlayer = gcode.getLayerNo(currentprintingline);
    if (currentlayer>=0) {
      int start = gcode.getLayerStart(currentlayer);
      int end   = gcode.getLayerEnd(currentlayer);
      gcode.drawCommands(render, settings, start, currentprintingline, true, 4);
      gcode.drawCommands(render, settings, currentprintingline,  end,  true, 1);
    }
  }
}

void Model::GlDrawGCode(Render &render, double layerz) {
  if (!settings.get_boolean("Display","DisplayGCode")) return;
  int layer = gcode.getLayerNo(layerz);
  if (layer>=0)
    GlDrawGCode(render, layer);
}

void Model::init() {
}

void Model::WriteGCode(Glib::RefPtr<Gio::File> file) {
  Glib::ustring contents = gcode.get_text();
  Glib::file_set_contents (file->get_path(), contents);
  settings.GCodePath = file->get_parent()->get_path();
}

vector<Shape*> Model::ReadShapes(Glib::RefPtr<Gio::File> file,
				 uint max_triangles) {
  vector<Shape*> shapes;
  if (!file) return shapes;
  File sfile(file);
  vector< vector<Triangle> > triangles;
  vector<ustring> shapenames;
  sfile.loadTriangles(triangles, shapenames, max_triangles);
  for (uint i = 0; i < triangles.size(); i++) {
    if (triangles[i].size() > 0) {
      Shape *shape = new Shape();
      shape->setTriangles(triangles[i]);
      shape->filename = shapenames[i];
      shape->FitToVolume(settings.getPrintVolume() - 2.*settings.getPrintMargin());
      shapes.push_back(shape);
    }
  }
  return shapes;
}

void Model::ReadStl(Glib::RefPtr<Gio::File> file) {
  bool autoplace = true;
  vector<Shape*> shapes = ReadShapes(file, 0);
  // do not autoplace in multishape files
  if (shapes.size() > 1)  autoplace = false;
  for (uint i = 0; i < shapes.size(); i++){
    AddShape(NULL, shapes[i], shapes[i]->filename, autoplace);
  }
  shapes.clear();
  ModelChanged();
}

void Model::SaveStl(Glib::RefPtr<Gio::File> file) {
  vector<Shape*> shapes;
  vector<Matrix4d> transforms;
  objtree.get_all_shapes(shapes,transforms);

  if(shapes.size() == 1) {
    shapes[0]->saveBinarySTL(file->get_path());
  }
  else {
    Shape single = GetCombinedShape();
    single.saveBinarySTL(file->get_path());
  }
  
  settings.STLPath = file->get_parent()->get_path();
}

// everything in one shape
Shape Model::GetCombinedShape() const {
  Shape shape;
  for (uint o = 0; o<objtree.Objects.size(); o++) {
    for (uint s = 0; s<objtree.Objects[o]->shapes.size(); s++) {
      vector<Triangle> tr =
	objtree.Objects[o]->shapes[s]->getTriangles(objtree.Objects[o]->transform3D.getTransform());
      shape.addTriangles(tr);
    }
  }
  
  return shape;
}

void Model::Read(Glib::RefPtr<Gio::File> file) {
  std::string basename = file->get_basename();
  size_t pos = basename.rfind('.');
  cerr << "reading " << basename<< endl;
  string directory_path = file->get_parent()->get_path();
  if (pos != std::string::npos) {
    std::string extn = basename.substr(pos);
    if (extn == ".conf") {
      LoadConfig(file);
      settings.SettingsPath = directory_path;
      return;
    } else if (extn == ".gcode") {
      ReadGCode(file);
      settings.GCodePath = directory_path;
      return;
    }
  }
  
  ReadStl(file);
  settings.STLPath = directory_path;
}

void Model::ReadGCode(Glib::RefPtr<Gio::File> file) {
  if (is_calculating) return;
  if (is_printing) return;
  is_calculating=true;
  settings.set_boolean("Display","DisplayGCode",true);
  m_progress->start (_("Reading GCode"), 100.0);
  gcode.Read (this, settings.get_extruder_letters(), m_progress, file->get_path());
  m_progress->stop (_("Done"));
  is_calculating=false;
}

void Model::ModelChanged() {
  if (m_inhibit_modelchange) return;
  if (objtree.empty()) return;
  //printer.update_temp_poll_interval(); // necessary?
  if (!is_printing) {
    CalcBoundingBoxAndCenter();
    setCurrentPrintingLine(0);
    m_model_changed.emit();
  }
}

Vector3d Model::FindEmptyLocation(const vector<Shape*> &shapes,
				  const vector<Matrix4d> &transforms,
				  const Shape *shape) {
  // Offset that puts object in center of bed, on platform
  double d = 5.0; // Minimum spacing between objects
  double bedx = settings.get_double("Hardware", "Volume.X");
  double bedy = settings.get_double("Hardware", "Volume.Y");
  Vector3d offset = {bedx / 2 - shape->Center.x(),
		     bedy / 2 - shape->Center.y(),
		     -shape->Min.z()};
  Vector3d smin = shape->Min - d;
  Vector3d smax = shape->Max + d;
  
  // Get all object bounding boxes
  vector<Vector3d> maxpos;
  vector<Vector3d> minpos;
  for (size_t s = 0; s < shapes.size(); s++) {
    minpos.push_back(shapes[s]->Min);
    maxpos.push_back(shapes[s]->Max);
  }
  
  // Search for empty spot
  double grid = d/5;
  double max_x = max(fabs(smin.x()), fabs(smax.x()));
  double max_y = max(fabs(smin.y()), fabs(smax.y()));
  double max_r = max(bedx / 2 + max_x, bedy / 2 + max_y) + d;
  Vector3d result;
  
  for (double radius = 0; radius < max_r; radius += grid) {
    double num = ceil(2 * M_PI * radius / grid + 1e-6);

    for (double count = 0; count < num; count++) {
      result.set(radius * cos(2 * M_PI * count / num) + offset.x(),
		 radius * sin(2 * M_PI * count / num) + offset.y(),
		 offset.z());
      Vector3d ssmin = smin + result;
      Vector3d ssmax = smax + result;

      // Check if on bed
      if (ssmin.x() < 0 || ssmax.x() > bedx ||
	  ssmin.y() < 0 || ssmax.y() > bedy)
	continue;
      
      // Check if contacts shapes
      bool valid = true;
      for (size_t s = 0; s < minpos.size(); s++) {
	Vector3d *pmin = &minpos[s];
	Vector3d *pmax = &maxpos[s];
	
	if (ssmin.x() > pmax->x() || ssmax.x() < pmin->x() ||
	    ssmin.y() > pmax->y() || ssmax.y() < pmin->y())
	  continue;
	
	valid = false;
	break;
      }
      
      if (valid)
	return result;
    }
  }
  
  // no empty spots
  return offset;
}

bool Model::FindEmptyLocation(Vector3d &result, const Shape *shape) {
  // Get all object positions
  std::vector<Vector3d> maxpos;
  std::vector<Vector3d> minpos;

  vector<Shape*>   allshapes;
  vector<Matrix4d> transforms;
  objtree.get_all_shapes(allshapes, transforms);
  result = FindEmptyLocation(allshapes, transforms, shape);
  return true;
}

int Model::AddShape(TreeObject *parent, Shape *shape, string filename, bool autoplace) {
  //Shape *retshape;
  bool found_location=false;


  if (!parent) {
    if (objtree.Objects.size() <= 0)
      objtree.newObject();
    parent = objtree.Objects.back();
  }
  g_assert (parent != NULL);

  // Decide where it's going
  Vector3d trans = Vector3d(0,0,0);
  if (autoplace) found_location = FindEmptyLocation(trans, shape);
  // Add it to the set
  size_t found = filename.find_last_of("/\\");
  Gtk::TreePath path = objtree.addShape(parent, shape, filename.substr(found+1));
  Shape *retshape = parent->shapes.back();

  // Move it, if we found a suitable place
  if (found_location) {
    retshape->transform3D.move(trans);
  }

  //if (autoplace) retshape->PlaceOnPlatform();

  // Update the view to include the new object
  ModelChanged();
    // Tell everyone
  m_signal_stl_added.emit (path);

  return 0;
}

int Model::SplitShape(TreeObject *parent, Shape *shape, string filename) {
  vector<Shape*> splitshapes;
  shape->splitshapes(splitshapes, m_progress);
  if (splitshapes.size()<2) return splitshapes.size();
  for (uint s = 0; s <  splitshapes.size(); s++) {
    ostringstream sfn;
    sfn << filename << "_" << (s+1) ;
    AddShape(parent, splitshapes[s], sfn.str(), false);
  }
  return splitshapes.size();
}

int Model::MergeShapes(TreeObject *parent, const vector<Shape*> shapes) {
  Shape * shape = new Shape();
  for (uint s = 0; s <  shapes.size(); s++) {
    vector<Triangle> str = shapes[s]->getTriangles();
    shape->addTriangles(str);
  }
  AddShape(parent, shape, "merged", true);
  return 1;
}

void Model::newObject() {
  objtree.newObject();
}

/* Scales the object on changes of the scale slider */
void Model::ScaleObject(Shape *shape, TreeObject *object, double scale) {
  if (shape)
    shape->Scale(scale);
  else if(object)
    //    for (uint s = 0;s<object->shapes.size(); s++) {
      //double fact = object->shapes[s].getScaleFactor();
      object->transform3D.scale(scale);
  //}
  else return;
  ModelChanged();
}

void Model::ScaleObjectX(Shape *shape, TreeObject *object, double scale) {
  if (shape)
    shape->ScaleX(scale);
  else if(object)
    for (uint s = 0;s<object->shapes.size(); s++) {
      //double fact = object->shapes[s].getScaleFactor();
      object->shapes[s]->ScaleX(scale);
    }
  else return;
  ModelChanged();
}

void Model::ScaleObjectY(Shape *shape, TreeObject *object, double scale) {
  if (shape)
    shape->ScaleY(scale);
  else if(object)
    for (uint s = 0;s<object->shapes.size(); s++) {
      //double fact = object->shapes[s].getScaleFactor();
      object->shapes[s]->ScaleY(scale);
    }
  else return;
  ModelChanged();
}

void Model::ScaleObjectZ(Shape *shape, TreeObject *object, double scale) {
  if (shape)
    shape->ScaleZ(scale);
  else if(object)
    for (uint s = 0;s<object->shapes.size(); s++) {
      //      double fact = object->shapes[s].getScaleFactorZ();
      object->shapes[s]->ScaleZ(scale);
    }
  else return;
  ModelChanged();
}

void Model::RotateObject(Shape* shape, TreeObject* object, Vector4d rotate) {
  if (!shape)
    return;
  Vector3d rot(rotate.x(), rotate.y(), rotate.z());
  shape->Rotate(rot, rotate.w());
  ModelChanged();
}

void Model::Mirror(Shape *shape, TreeObject *object) {
  if (shape)
    shape->mirror();
  else
    return;
  ModelChanged();
}

void Model::PlaceOnPlatform(Shape *shape, TreeObject *object) {
  if (shape)
    shape->PlaceOnPlatform();
  else if(object) {
    Transform3D * transf = &object->transform3D;
    transf->move(Vector3f(0, 0, -transf->getTranslation().z()));
    for (uint s = 0;s<object->shapes.size(); s++) {
      object->shapes[s]->PlaceOnPlatform();
    }
  }
  else return;
  ModelChanged();
}

void Model::DeleteObjTree(vector<Gtk::TreeModel::Path> &iter) {
  objtree.DeleteSelected (iter);
  ClearGCode();
  ModelChanged();
}


void Model::ClearLogs() {
  errlog->set_text("");
  echolog->set_text("");
}

void Model::CalcBoundingBoxAndCenter(bool selected_only) {
  Vector3d newMax = Vector3d(G_MINDOUBLE, G_MINDOUBLE, G_MINDOUBLE);
  Vector3d newMin = Vector3d(G_MAXDOUBLE, G_MAXDOUBLE, G_MAXDOUBLE);

  vector<Shape*> shapes;
  vector<Matrix4d> transforms;
  if (selected_only)
    objtree.get_selected_shapes(m_current_selectionpath, shapes, transforms);
  else
    objtree.get_all_shapes(shapes, transforms);

  for (uint s = 0 ; s < shapes.size(); s++) {
    shapes[s]->CalcBBox();
    Vector3d stlMin = transforms[s] * shapes[s]->Min;
    Vector3d stlMax = transforms[s] * shapes[s]->Max;
    for (uint k = 0; k < 3; k++) {
      newMin[k] = MIN(stlMin[k], newMin[k]);
      newMax[k] = MAX(stlMax[k], newMax[k]);
    }
  }

  if (newMin.x() > newMax.x()) {
    // Show the whole platform if there's no objects
    Min = Vector3d(0,0,0);
    Vector3d pM = settings.getPrintMargin();
    Max = settings.getPrintVolume() - pM - pM;
    Max.z() = 0;
  }
  else {
    Max = newMax;
    Min = newMin;
  }

  Center = (Max + Min) / 2.0;
}

Vector3d Model::GetViewCenter() {
  Vector3d printOffset = settings.getPrintMargin();
  if(settings.get_boolean("Raft","Enable")){
    const double rsize = settings.get_double("Raft","Size");
    printOffset += Vector3d(rsize, rsize, 0);
  }
  return printOffset + Center;
}

int Model::drawShapes(Render &render, vector<Gtk::TreeModel::Path> &iter) {
  vector<Shape*> sel_shapes;
  vector<Matrix4d> transforms;
  objtree.get_selected_shapes(iter, sel_shapes, transforms);

  // draw preview shapes and nothing else
  if (settings.get_boolean("Display","PreviewLoad") && preview_shapes.size() > 0) {
    for (uint i = 0; i < preview_shapes.size(); i++)
      preview_shapes[i]->draw_geometry(render, 0, false, settings, 2000000);
    
    return 0;
  }
  
  size_t index = 1; // pick/select index. matches computation in update_model()
  for (uint i = 0; i < objtree.Objects.size(); i++) {
    TreeObject *object = objtree.Objects[i];
    index++;

    for (uint j = 0; j < object->shapes.size(); j++) {
      Shape *shape = object->shapes[j];

      bool is_selected = false;
      for (uint s = 0; s < sel_shapes.size(); s++)
	if (sel_shapes[s] == shape)
	  is_selected = true;

      shape->draw_geometry(render, index, is_selected, settings);
      index++;
    }
  }
  
  return -1;
}

int Model::drawBBoxes(Render &render) {
  // draw preview shapes and nothing else
  if (settings.get_boolean("Display","PreviewLoad") && preview_shapes.size() > 0) {
    for (uint i = 0; i < preview_shapes.size(); i++)
      preview_shapes[i]->drawBBox(render, settings);
    
    return 0;
  }
  
  for (uint i = 0; i < objtree.Objects.size(); i++) {
    TreeObject *object = objtree.Objects[i];
    for (uint j = 0; j < object->shapes.size(); j++) {
      Shape *shape = object->shapes[j];

      shape->drawBBox(render, settings);
    }
  }
  
  return -1;
}
