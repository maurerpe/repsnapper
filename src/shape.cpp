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

#include "shape.h"
#include "files.h"
#include "ui/progress.h"
#include "settings.h"
#include "render.h"
#include "geometry.h"

#ifdef _OPENMP
#include <omp.h>
#endif

// Constructor
Shape::Shape() {
  Min.set(0,0,0);
  Max.set(200,200,200);
  CalcBBox();
}

void Shape::clear() {
  triangles.clear();
};

void Shape::setTriangles(const vector<Triangle> &triangles_) {
  triangles = triangles_;

  CalcBBox();
  double vol = volume();
  if (vol < 0) {
    invertNormals();
    vol = -vol;
  }

  //PlaceOnPlatform();
  cerr << _("Shape has volume ") << volume() << _(" mm^3 and ")
       << triangles.size() << _(" triangles") << endl;
}


int Shape::saveBinarySTL(Glib::ustring filename) const {
  if (!File::saveBinarySTL(filename, triangles, transform3D.getTransform()))
    return -1;
  return 0;

}

// recursively build a list of triangles for a shape
void addtoshape(uint i, const vector< vector<uint> > &adj,
		vector<uint> &tr, vector<bool> &done) {
  if (!done[i]) {
    // add this index to tr
    tr.push_back(i);
    done[i] = true;
    for (uint j = 0; j < adj[i].size(); j++) {
      if (adj[i][j]!=i)
     	addtoshape(adj[i][j], adj, tr, done);
    }
  }
  // we have a complete list of adjacent triangles indices
}

void Shape::splitshapes(vector<Shape*> &shapes, ViewProgress *progress) {
  int n_tr = (int)triangles.size();
  if (progress) progress->start(_("Split Shapes"), n_tr);
  int progress_steps = max(1,(int)(n_tr/100));
  vector<bool> done(n_tr);
  bool cont = true;
  // make list of adjacent triangles for each triangle
  vector< vector<uint> > adj(n_tr);
  if (progress) progress->set_label(_("Split: Sorting Triangles ..."));
#ifdef _OPENMP
  omp_lock_t progress_lock;
  omp_init_lock(&progress_lock);
#pragma omp parallel for schedule(dynamic)
#endif
  for (int i = 0; i < n_tr; i++) {
    if (progress && i%progress_steps==0) {
#ifdef _OPENMP
      omp_set_lock(&progress_lock);
#endif
      cont = progress->update(i);
#ifdef _OPENMP
      omp_unset_lock(&progress_lock);
#endif
    }
    vector<uint> trv;
    for (int j = 0; j < n_tr; j++) {
      if (i!=j) {
	bool add = false;
	if (j<i) // maybe(!) we have it already
	  for (uint k = 0; k<adj[j].size(); k++) {
	    if ((int)adj[j][k] == i) {
	      add = true; break;
	    }
	  }
	add |= (triangles[i].isConnectedTo(triangles[j], 0.01));
	if (add) trv.push_back(j);
      }
    }
    adj[i] = trv;
    if (!cont) i=n_tr;
  }

  if (progress) progress->set_label(_("Split: Building shapes ..."));


  // triangle indices of shapes
  vector< vector<uint> > shape_tri;

  for (int i = 0; i < n_tr; i++) done[i] = false;
  for (int i = 0; i < n_tr; i++) {
    if (progress && i%progress_steps==0)
      cont = progress->update(i);
    if (!done[i]){
      cerr << _("Shape ") << shapes.size()+1 << endl;
      vector<uint> current;
      addtoshape(i, adj, current, done);
      Shape *shape = new Shape();
      shapes.push_back(shape);
      shapes.back()->triangles.resize(current.size());
      for (uint i = 0; i < current.size(); i++)
	shapes.back()->triangles[i] = triangles[current[i]];
      shapes.back()->CalcBBox();
    }
    if (!cont) i=n_tr;
  }

  if (progress) progress->stop("_(Done)");
}

void Shape::invertNormals() {
  for (uint i = 0; i < triangles.size(); i++)
    triangles[i].invertNormal();
}

// doesn't work
void Shape::repairNormals(double sqdistance) {
  for (uint i = 0; i < triangles.size(); i++) {
    vector<uint> adjacent;
    uint numadj=0, numwrong=0;
    for (uint j = i+1; j < triangles.size(); j++) {
      if (i!=j) {
	if (triangles[i].isConnectedTo(triangles[j], sqdistance)) {
	  numadj++;
	  if (triangles[i].wrongOrientationWith(triangles[j], sqdistance)) {
	    numwrong++;
	    triangles[j].invertNormal();
	  }
	}
      }
    }
    //cerr << i<< ": " << numadj << " - " << numwrong  << endl;
    //if (numwrong > numadj/2) triangles[i].invertNormal();
  }
}

void Shape::mirror() {
  const Vector3d mCenter = transform3D.getInverse() * Center;
  for (uint i = 0; i < triangles.size(); i++)
    triangles[i].mirrorX(mCenter);
  CalcBBox();
}

double Shape::volume() const {
  double vol=0;
  
  for (uint i = 0; i < triangles.size(); i++)
    vol+=triangles[i].projectedvolume(transform3D.getTransform());
  
  return vol;
}

string Shape::getSTLsolid() const {
  stringstream sstr;
  sstr << "solid " << filename <<endl;
  for (uint i = 0; i < triangles.size(); i++)
    sstr << triangles[i].getSTLfacet(transform3D.getTransform());
  sstr << "endsolid " << filename <<endl;
  return sstr.str();
}

void Shape::addTriangles(const vector<Triangle> &tr) {
  triangles.insert(triangles.end(), tr.begin(), tr.end());
  CalcBBox();
}

vector<Triangle> Shape::getTriangles(const Matrix4d &T) const {
  vector<Triangle> tr(triangles.size());
  for (uint i = 0; i < triangles.size(); i++) {
    tr[i] = triangles[i].transformed(T*transform3D.getTransform());
  }
  return tr;
}

vector<Triangle> Shape::trianglesSteeperThan(double angle) const {
  vector<Triangle> tr;
  for (uint i = 0; i < triangles.size(); i++) {
    // negative angles are triangles facing downwards
    const double tangle = -triangles[i].slopeAngle(transform3D.getTransform());
    if (tangle >= angle)
      tr.push_back(triangles[i]);
  }
  return tr;
}

void Shape::FitToVolume(const Vector3d &vol) {
  Vector3d diag = Max-Min;
  const double sc_x = diag.x() / vol.x();
  const double sc_y = diag.y() / vol.y();
  const double sc_z = diag.z() / vol.z();
  double max_sc = max(max(sc_x, sc_y),sc_z);
  if (max_sc > 1.)
    Scale(1./max_sc, true);
}

void Shape::Scale(double in_scale_factor, bool calcbbox) {
  transform3D.scale(in_scale_factor);
  if (calcbbox)
    CalcBBox();
}

void Shape::ScaleX(double x) {
  transform3D.scale_x(x);
}

void Shape::ScaleY(double x) {
  transform3D.scale_y(x);
}

void Shape::ScaleZ(double x) {
  transform3D.scale_z(x);
}

void Shape::CalcBBox() {
  Min.set(INFTY,INFTY,INFTY);
  Max.set(-INFTY,-INFTY,-INFTY);
  Matrix4d trans = transform3D.getTransform();
  for(size_t i = 0; i < triangles.size(); i++) {
    triangles[i].AccumulateMinMax (Min, Max, trans);
  }
  Center = (Max + Min) / 2;
}

struct SNorm {
  Vector3d normal;
  double area;
  bool operator<(const SNorm &other) const {return (area<other.area);};
};

void Shape::PlaceOnPlatform() {
  transform3D.move(Vector3d(0,0,-Min.z()));
}

// Rotate shape about center
void Shape::Rotate(const Vector3d & axis, const double & angle) {
  transform3D.rotate(Center, axis, angle);
  CalcBBox();
}

int find_vertex(const vector<Vector2d> &vertices,
		const Vector2d &v, double delta = 0.0001) {
  int found = -1;
  int count = (int)vertices.size();
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic)
#endif
  for (int i=0; i<count; i++) {
    if (found != -1) continue;
    if ( (v-vertices[i]).squared_length() < delta ) {
      found = i;
#ifndef _OPENMP
      break;
#endif
    }
  }
  return found;
}

// called from Model::draw
void Shape::draw(Render &render, const Settings &settings, bool highlight, uint max_triangles) {
  //cerr << "Shape::draw" <<  endl;

  if (settings.get_boolean("Display","DisplayPolygons")) {
    draw_geometry(render, settings, max_triangles);
  }
  
  if (settings.get_boolean("Display","DisplayBBox")) {
    drawBBox(render);
  }
}

// the bounding box is in real coordinates (not transformed)
void Shape::drawBBox(Render &render) const {
  const double minz = max(0.,Min.z()); // draw above zero plane only

  // Draw bbox
  RenderVert vert;
  vert.add(Min.x(), Min.y(), minz);
  vert.add(Min.x(), Max.y(), minz);

  vert.add(Min.x(), Max.y(), minz);
  vert.add(Max.x(), Max.y(), minz);
  
  vert.add(Max.x(), Max.y(), minz);
  vert.add(Max.x(), Min.y(), minz);
  
  vert.add(Max.x(), Min.y(), minz);
  vert.add(Min.x(), Min.y(), minz);
  
  vert.add(Min.x(), Min.y(), Max.z());
  vert.add(Min.x(), Max.y(), Max.z());
  
  vert.add(Min.x(), Max.y(), Max.z());
  vert.add(Max.x(), Max.y(), Max.z());
  
  vert.add(Max.x(), Max.y(), Max.z());
  vert.add(Max.x(), Min.y(), Max.z());
  
  vert.add(Max.x(), Min.y(), Max.z());
  vert.add(Min.x(), Min.y(), Max.z());

  vert.add(Min.x(), Min.y(), minz);
  vert.add(Min.x(), Min.y(), Max.z());
  
  vert.add(Min.x(), Max.y(), minz);
  vert.add(Min.x(), Max.y(), Max.z());
  
  vert.add(Max.x(), Max.y(), minz);
  vert.add(Max.x(), Max.y(), Max.z());
  
  vert.add(Max.x(), Min.y(), minz);
  vert.add(Max.x(), Min.y(), Max.z());
  
  //RenderModelTrans mt(render, transform3D.getTransform());
  float color[4] = {1, 0.2, 0.2, 1};
  render.draw_lines(color, vert, 1.0);

  ostringstream val;
  val.precision(1);
  Vector3d pos;
  val << fixed << (Max.x()-Min.x());
  pos = Vector3d((Max.x()+Min.x())/2.,Min.y(),Max.z());
  render.draw_string(color, pos, val.str(), 12);
  val.str("");
  val << fixed << (Max.y()-Min.y());
  pos = Vector3d(Min.x(),(Max.y()+Min.y())/2.,Max.z());
  render.draw_string(color, pos, val.str(), 12);
  val.str("");
  val << fixed << (Max.z()-minz);
  pos = Vector3d(Min.x(),Min.y(),(Max.z()+minz)/2.);
  render.draw_string(color, pos, val.str(), 12);
}

void Shape::draw_geometry(Render &render, const Settings &settings, uint max_triangles) {
  RenderVert vert;
  float color[4] = {1.0, 1.0, 1.0, 0};
  color[3] = settings.get_double("Display","PolygonOpacity");
  
  for(size_t i = 0; i < triangles.size(); i++) {
    Triangle *tri = &triangles[i];
    //Vector3d norm = normalized((tri->C - tri->A).cross(tri->B - tri->A));
    Vector3d norm = tri->Normal;
    vert.add(tri->A);
    vert.add(norm);
    vert.add(tri->B);
    vert.add(norm);
    vert.add(tri->C);
    vert.add(norm);
  }

  RenderModelTrans mt(render, transform3D.getTransform());
  render.draw_triangles(color, vert);
}

string Shape::info() const {
  ostringstream ostr;
  ostr <<"Shape with "<<triangles.size() << " triangles "
       << "min/max/center: "<<Min<<Max <<Center ;
  return ostr.str();
}
