/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2010  Kulitorum
    Copyright (C) 2011-12  martin.dieringer@gmx.de

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

#include <vector>
#include <list>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <limits>
#include <algorithm>
#include "stdafx.h"
#include "transform3d.h"
#include "triangle.h"
#include "ui/render.h"

class Shape {
 public:
  virtual short dimensions(){return 3;};

  Shape();
  virtual ~Shape(){};
  Glib::ustring filename;
  int idx;

  Transform3D transform3D;

  virtual void clear();
  
  virtual void CalcBBox();

  virtual void Rotate(const Vector3d &center, const Vector3d &axis, const double &angle);
  virtual void Rotate(const Vector3d &axis, const double &angle) {Rotate(Center, axis, angle);};
  virtual void move(Vector3d delta){ transform3D.move(delta); CalcBBox();};

  void Scale(double scale_factor, bool calcbbox = true);
  void ScaleX(double scale_factor);
  void ScaleY(double scale_factor);
  virtual void ScaleZ(double scale_factor);
  double getScaleFactor() { return transform3D.get_scale(); };
  double getScaleFactorX(){ return transform3D.get_scale_x(); };
  double getScaleFactorY(){ return transform3D.get_scale_y(); };
  virtual double getScaleFactorZ(){ return transform3D.get_scale_z(); };

  void FitToVolume(const Vector3d &vol);

  void PlaceOnPlatform();

  Vector3d Min, Max, Center;

  string getSTLsolid() const;
  double volume() const;

  void invertNormals();
  virtual void mirror();
  virtual void splitshapes(vector<Shape*> &shapes, ViewProgress *progress=NULL);

  int saveBinarySTL(Glib::ustring filename) const;

  virtual string info() const;

  vector<Triangle> getTriangles(const Matrix4d &T=Matrix4d::IDENTITY) const;
  void addTriangles(const vector<Triangle> &tr);

  void setTriangles(const vector<Triangle> &triangles_);

  uint size() const {return triangles.size();}

  virtual void draw_geometry(Render &render, size_t index, bool highlight, const Settings &settings, uint max_triangles=0);  
  void drawBBox(Render &render, Settings &settings) const;
  
 private:
  vector<Triangle> triangles;
  void calcPolygons();
  bool vert_valid;
  RenderVert vert;
};
