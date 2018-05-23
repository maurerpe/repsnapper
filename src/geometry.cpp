/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2010 Kulitorum
    Copyright (C) 2013 martin.dieringer@gmx.de

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

#include "geometry.h"

void move(const Vector3f &delta, Matrix4f &mat){
  Vector3f trans;
  mat.get_translation(trans);
  mat.set_translation(trans+delta);
}

Vector3d normalized(const Vector3d &v){
  Vector3d n(v); n.normalize(); return n;
}

Vector2d normalized(const Vector2d &v){
  Vector2d n(v); n.normalize(); return n;
}

// is B left of A wrt center?
bool isleftof(const Vector2d &center, const Vector2d &A, const Vector2d &B)
{
  double position = (B.x()-A.x())*(center.y()-A.y()) - (B.y()-A.y())*(center.x()-A.x());
  return (position >= 0);
}

// long double planeAngleBetween(const Vector2d &V1, const Vector2d &V2)
// {
//     long double dotproduct =  V1.dot(V2);
//     long double length = V1.length() * V2.length();
//     long double quot = dotproduct / length;
//     if (quot > 1  && quot < 1.0001) quot = 1; // strange case where acos => NaN
//     if (quot < -1 && quot > -1.0001) quot = -1;
//     long double result = acosl( quot ); // 0 .. pi
//     if (isleftof(Vector2d(0,0), V2, V1))
//         result = -result;
//     return result;
// }

double planeAngleBetween(const Vector2d &V1, const Vector2d &V2) {
  Vector2d a = normalized(V1);
  Vector2d b = normalized(V2);
  double bx = a[0] * b[0] + a[1] * b[1];
  double by = a[0] * b[1] - a[1] * b[0];

  return atan2(by, bx);
}

void moveArcballTrans(Matrix4fT &matfT, const Vector3d &delta) {
  Matrix4f matf;
  typedef Matrix4f::iterator mIt;
  for (mIt it = matf.begin(); it!=matf.end(); ++it){
    uint i = it - matf.begin();
    *it = matfT.M[i];
  }
  move(delta,matf);
  for (mIt it = matf.begin(); it!=matf.end(); ++it){
    uint i = it - matf.begin();
    matfT.M[i] = *it;
  }
}

void setArcballTrans(Matrix4fT &matfT, const Vector3d &trans) {
  Matrix4f matf;
  typedef Matrix4f::iterator  mIt;
  for (mIt it = matf.begin(); it!=matf.end(); ++it){
    uint i = it - matf.begin();
    *it = matfT.M[i];
  }
  matf.set_translation(trans);
  for (mIt it = matf.begin(); it!=matf.end(); ++it){
    uint i = it - matf.begin();
    matfT.M[i] = *it;
  }
}

void rotArcballTrans(Matrix4fT &matfT,  const Vector3d &axis, double angle)
{
  Matrix4f rot = Matrix4f::IDENTITY;
  Vector3d naxis(axis); naxis.normalize();
  rot.rotate(angle, naxis);
  Matrix4f matf;
  typedef Matrix4f::iterator  mIt;
  for (mIt it = matf.begin(); it!=matf.end(); ++it){
    uint i = it - matf.begin();
    *it = matfT.M[i];
  }
  matf = rot * matf;
  for (mIt it = matf.begin(); it!=matf.end(); ++it){
    uint i = it - matf.begin();
    matfT.M[i] = *it;
  }
}

// The following functions
// (inSegment, intersect2D_Segments and dist3D_Segment_to_Segment)
// are licensed as:
//
// Copyright 2001 softSurfer, 2012-13 Dan Sunday
// This code may be freely used, distributed and modified for any
// purpose providing that this copyright notice is included with it.
// SoftSurfer makes no warranty for this code, and cannot be held
// liable for any real or imagined damage resulting from its use.
// Users of this code must verify correctness for their application.
//
// inSegment(): determine if a point is inside a segment
//    Input:  a point P, and a collinear segment p1--p2
//    Return: true  = P is inside p1--p2
//            fasle = P is not inside p1--p2
bool inSegment( const Vector2d &P, const Vector2d &p1, const Vector2d &p2)
{
  if (p1.x() != p2.x()) {    // S is not vertical
    if (p1.x() <= P.x() && P.x() <= p2.x())
      return true;
    if (p1.x() >= P.x() && P.x() >= p2.x())
      return true;
  }
  else {    // S is vertical, so test y coordinate
    if (p1.y() <= P.y() && P.y() <= p2.y())
      return true;
    if (p1.y() >= P.y() && P.y() >= p2.y())
      return true;
  }
  return false;
}

// intersect2D_2Segments(): the intersection of 2 finite 2D segments
//    Input:  two finite segments p1-p2 and p3-p4
//    Output: *I0 = intersect point (when it exists)
//            *I1 = endpoint of intersect segment [I0,I1] (when it exists)
//    Return: 0=disjoint (no intersect)
//            1=intersect in unique point I0
//            2=overlap in segment from I0 to I1
//            3=lines intersect outside the segments
#define perp(u,v)  ((u).x() * (v).y() - (u).y() * (v).x())  // perp product (2D)
int intersect2D_Segments( const Vector2d &p1, const Vector2d &p2,
			  const Vector2d &p3, const Vector2d &p4,
			  Vector2d &I0, Vector2d &I1,
			  double maxerr)
{
  Vector2d    u = p2 - p1;
  Vector2d    v = p4 - p3;
  Vector2d    w = p1 - p3;
  double    D = perp(u,v);
  double t0, t1; // Temp vars for parametric checks

  // test if they are parallel (includes either being a point)
  if (abs(D) < maxerr) {          // S1 and S2 are parallel
    if (perp(u,w) != 0 || perp(v,w) != 0) {
      return 0;                   // they are NOT collinear
    }
    // they are collinear or degenerate
    // check if they are degenerate points
    double du = u.dot(u);
    double dv = v.dot(v);
    if (du==0 && dv==0) {           // both segments are points
      if (p1 != p3)         // they are distinct points
	return 0;
      I0 = p1;                // they are the same point
      return 1;
    }
    if (du==0) {                    // S1 is a single point
      if (!inSegment(p1, p3, p4))  // but is not in S2
	return 0;
      I0 = p1;
      return 1;
    }
    if (dv==0) {                    // S2 a single point
      if (!inSegment(p3, p1,p2))  // but is not in S1
	return 0;
      I0 = p3;
      return 1;
    }
    // they are collinear segments - get overlap (or not)
    Vector2d w2 = p2 - p3;
    if (v.x() != 0) {
      t0 = w.x() / v.x();
      t1 = w2.x() / v.x();
    }
    else {
      t0 = w.y() / v.y();
      t1 = w2.y() / v.y();
    }
    if (t0 > t1) {                  // must have t0 smaller than t1
      double t=t0; t0=t1; t1=t;    // swap if not
    }
    if (t0 > 1 || t1 < 0) {
      return 0;     // NO overlap
    }
    t0 = t0<0? 0 : t0;              // clip to min 0
    t1 = t1>1? 1 : t1;              // clip to max 1
    if (t0 == t1) {                 // intersect is a point
      I0 = p3 + v*t0;
      return 1;
    }

    // they overlap in a valid subsegment
    I0 = p3 + v*t0;
    I1 = p3 + v*t1;
    return 2;
  } // end parallel

  bool outside = false;
  // the segments are skew and may intersect in a point
  // get the intersect parameter for S1
  t0 = perp(v,w) / D;
  if (t0 < 0 || t0 > 1)               // no intersect in S1
    outside = true;
  //return 0;

  // get the intersect parameter for S2
  t1 = perp(u,w) / D;
  if (t1 < 0 || t1 > 1)               // no intersect in S2
    outside = true;
  //    return 0;

  I0 = p1 + u * t0;               // compute S1 intersect point
  if (outside) return 3;
  return 1;
}


// dist3D_Segment_to_Segment():
//    Input:  two 3D line segments S1 and S2
//    Return: the shortest distance between S1 and S2
double dist3D_Segment_to_Segment(const Vector3d &S1P0, const Vector3d &S1P1,
				 const Vector3d &S2P0, const Vector3d &S2P1, double SMALL_NUM)
{
     Vector3d   u = S1P1 - S1P0;
     Vector3d   v = S2P1 - S2P0;
     Vector3d   w = S1P0 - S2P0;
     double    a = u.dot(u);        // always >= 0
     double    b = u.dot(v);
     double    c = v.dot(v);        // always >= 0
     double    d = u.dot(w);
     double    e = v.dot(w);
     double    D = a*c - b*b;       // always >= 0
     double    sc, sN, sD = D;      // sc = sN / sD, default sD = D >= 0
     double    tc, tN, tD = D;      // tc = tN / tD, default tD = D >= 0

     // compute the line parameters of the two closest points
     if (D < SMALL_NUM) { // the lines are almost parallel
         sN = 0.0;        // force using point P0 on segment S1
         sD = 1.0;        // to prevent possible division by 0.0 later
         tN = e;
         tD = c;
     }
     else {                // get the closest points on the infinite lines
         sN = (b*e - c*d);
         tN = (a*e - b*d);
         if (sN < 0.0) {       // sc < 0 => the s=0 edge is visible
             sN = 0.0;
             tN = e;
             tD = c;
         }
         else if (sN > sD) {  // sc > 1 => the s=1 edge is visible
             sN = sD;
             tN = e + b;
             tD = c;
         }
     }

     if (tN < 0.0) {           // tc < 0 => the t=0 edge is visible
         tN = 0.0;
         // recompute sc for this edge
         if (-d < 0.0)
             sN = 0.0;
         else if (-d > a)
             sN = sD;
         else {
             sN = -d;
             sD = a;
         }
     }
     else if (tN > tD) {      // tc > 1 => the t=1 edge is visible
         tN = tD;
         // recompute sc for this edge
         if ((-d + b) < 0.0)
             sN = 0;
         else if ((-d + b) > a)
             sN = sD;
         else {
             sN = (-d + b);
             sD = a;
         }
     }
     // finally do the division to get sc and tc
     sc = (abs(sN) < SMALL_NUM ? 0.0 : sN / sD);
     tc = (abs(tN) < SMALL_NUM ? 0.0 : tN / tD);

     // get the difference of the two closest points
     Vector3d dP = w + (u * sc ) - (v * tc);  // = S1(sc) - S2(tc)

     return dP.length();   // return the closest distance
 }
