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

#pragma once

#include <list>
#include <vector>
#include <memory>

#include "stdafx.h"
#include "triangle.h"

class PolyGroup {
 public:
  typedef shared_ptr<vector<Vector2d>> vect_ptr;  
  
 private:
  class Poly {
  public:
    vect_ptr pts;
    list<shared_ptr<Poly>> holes;

    Poly(vect_ptr pts_);
    bool IsInside(const Vector2d &pt);
  };

  list<shared_ptr<Poly>> polys;
  
  static bool IsInside(vect_ptr poly, const Vector2d &pt);
  static void Place(list<shared_ptr<Poly>> &polys, shared_ptr<Poly> pp);
  static void AddTriangles(shared_ptr<vector<Triangle>> tri,
			   list<shared_ptr<Poly>> &polys);
  
 public:
  PolyGroup();

  void AddPoly(vect_ptr poly);
  shared_ptr<vector<Triangle>> GetAsTriangles(void);
};
