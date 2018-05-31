/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2010 Kulitorum

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

#include "stdafx.h"

Vector3d normalized(const Vector3d &v);
Vector2d normalized(const Vector2d &v);

/* (-pi, pi] */
double CcwAngleBetween(const Vector2d &dest, const Vector2d &ref);

double dist3D_Segment_to_Segment(const Vector3d &S1P0, const Vector3d &S1P1,
				 const Vector3d &S2P0, const Vector3d &S2P1, double SMALL_NUM);
