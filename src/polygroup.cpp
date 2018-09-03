/*
    This file is a part of the RepSnapper project.
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

#include "polygroup.h"
#include "poly2tri.h"

PolyGroup::Poly::Poly(vect_ptr pts_) {
  pts = pts_;
}

PolyGroup::PolyGroup() {
}

bool PolyGroup::IsInside(vect_ptr poly, const Vector2d &pt) {
  bool in = false;
  Vector2d prev = poly->back();
  
  for (auto it = poly->begin(); it != poly->end(); prev = *it, ++it) {
    if (prev.x() <= pt.x() || it->x() > pt.x())
      continue;
    
    double yinter = (pt.x() - prev.x()) / (it->x() - prev.x()) * (it->y() - prev.y()) + prev.y();
    if (yinter > pt.y())
      in = !in;
  }
  
  return in;
}

void PolyGroup::Place(list<shared_ptr<Poly>> &polys, shared_ptr<Poly> pp) {
  // First check if any polys are inside the target pp
  for (auto it = polys.begin(); it != polys.end();) {
    if (IsInside(pp->pts, (*it)->pts->front()))
      pp->holes.splice(pp->holes.end(), polys, it++);
    else
      ++it;
  }
  
  if (pp->holes.size() > 0) {
    polys.push_back(pp);
    return;
  }
  
  // Next check if the target pp is inside any polys
  Vector2d front = pp->pts->front();
  for (auto it = polys.begin(); it != polys.end(); ++it) {
    if (IsInside((*it)->pts, front)) {
      Place((*it)->holes, pp);
      return;
    }
  }

  // All by itself
  polys.push_back(pp);
}

void PolyGroup::AddPoly(vect_ptr poly) {
  shared_ptr<Poly> pp(new Poly(poly));
  
  Place(polys, pp);
}

static void BuildVec(vector<p2t::Point *> &out,
		     vector<p2t::Point *> &all,
		     const PolyGroup::vect_ptr in) {
  out.clear();
  
  for (auto it = in->begin(); it != in->end(); ++it) {
    Vector2d *inpt = &(*it);
    p2t::Point *outpt = new p2t::Point(inpt->x(), inpt->y());
    out.push_back(outpt);
    all.push_back(outpt);
  }
}

static void FreeVec(vector<p2t::Point *> &all) {
  for (auto it = all.begin(); it != all.end(); ++it)
    delete (*it);
  
  all.clear();
}

static Vector3d Pt3(p2t::Point *pt) {
  return Vector3d(pt->x, pt->y, 0);
}

void PolyGroup::AddTriangles(shared_ptr<vector<Triangle>> tri,
			     list<shared_ptr<Poly>> &polys) {
  vector<p2t::Point *> vec;
  vector<p2t::Point *> all;
  
  for (auto it = polys.begin(); it != polys.end(); ++it) {
    auto holes = &(*it)->holes;
    BuildVec(vec, all, (*it)->pts);
    {
      p2t::CDT cdt(vec);
      
      for (auto ho = holes->begin(); ho != holes->end(); ++ho) {
	BuildVec(vec, all, (*ho)->pts);
	cdt.AddHole(vec);
      }
      
      cdt.Triangulate();
      
      vector<p2t::Triangle*> ret = cdt.GetTriangles();
      
      for (auto tt = ret.begin(); tt != ret.end(); ++tt) {
	Triangle triangle(Vector3d(0, 0, 1),
			  Pt3((*tt)->GetPoint(0)),
			  Pt3((*tt)->GetPoint(1)),
			  Pt3((*tt)->GetPoint(2)));
	tri->push_back(triangle);
      }
    }
    FreeVec(all);
    
    for (auto ho = holes->begin(); ho != holes->end(); ++ho)
      AddTriangles(tri, (*ho)->holes);
  }
}

shared_ptr<vector<Triangle>> PolyGroup::GetAsTriangles(void) {
  shared_ptr<vector<Triangle>> tri(new vector<Triangle>);
  
  AddTriangles(tri, polys);
  
  return tri;
}
