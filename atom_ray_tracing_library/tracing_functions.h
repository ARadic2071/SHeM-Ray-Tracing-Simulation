/*
 *  Copyright (c) 2018-20, Sam Lambrick.
 *  All rights reserved.
 *  This file is part of the SHeM ray tracing simulation, subject to the
 *  GNU/GPL-3.0-or-later.
 *
 *
 *  Functions that update the new position and direction of a ray
 *  on each individual collision event.
 */

#ifndef _tracing_functions_h
#define _tracing_functions_h

#include "mtwister.h"
#include "ray_tracing_core3D.h"

/*
 *  Finds the intersection, normal at the point of intersection and distance to
 *  the intersection between the ray and an analytic sphere. returns 0 if the
 *  ray does not intersect the sphere.
 */
void scatterOffSurface(Ray3D * the_ray, Surface3D sample, AnalytSphere the_sphere,
        MTRand * const myrng);

/*
 *  Finds the intersection, normal at the point of intersection and distance to
 *  the intersection between the ray and a triangulated surface.
 */
void scatterPinholeSurface(Ray3D * the_ray, Surface3D plate, const double backWall[],
        MTRand * const myrng);

/*
 *  Scatters a ray off two triangulared surfaces, and an analytic sphere if
 *  desired.
 */
void scatterSurfaces(Ray3D * the_ray, Surface3D sample, Surface3D plate,
		AnalytSphere the_sphere, const double backWall[], MTRand * const myrng);

/*
 *  Scatters a ray off a triangulated surface, and a simple model of the pinhole plate
 *  with multiple detector apertures.
 */
void scatterSimpleMulti(Ray3D * the_ray, Surface3D sample, NBackWall plate,
		AnalytSphere the_sphere, int * detector, MTRand * const myrng);

/*
 *  Scatters the ray off a sample and a attempts dtection on a hemisphere with abstract
 *  detector apertures placed on it.
 */
//int scatterAbstractSurfaces(Ray3D * the_ray, Surface3D const  * sample, AbstractHemi const *  plate,
//		const AnalytSphere * the_sphere, MTRand * myrng, int * status) ;

#endif
