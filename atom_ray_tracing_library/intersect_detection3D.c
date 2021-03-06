/*
 * Copyright (c) 2018-20, Sam Lambrick.
 * All rights reserved.
 * This file is part of the Sub-beam Ray Tracing simulation, subject to the
 * GNU/GPL-3.0-or-later.
 *
 * Intersect the ray with a particular surface. combinations of surfaces are
 * used to create a single interaction of the ray path.
 */
#include "intersect_detection3D.h"
#include "ray_tracing_core3D.h"
#include "distributions3D.h"
#include "mtwister.h"
#include <math.h>
#include <stdbool.h>


/*
 * Finds the distance to, the normal to, and the position of a rays intersection
 * with an analytically defined sphere. Returns 0 if the ray does not intersect
 * the sphere. The inclusion of the analytic sphere is not very general and can
 * only really be used for the specific probable it was written for without
 * alteration--a single sphere places just touching a flat surface in the centre
 * of the scan region.
 *
 * INPUTS:
 *  the_ray    - pointer to a ray3D struct, contains information on the ray
 *  normal     - double array, a variable to store the normal to the point
 *               of intersection in
 *  t          - double, a variable to store the distance to the
 *               intersection point in
 *  the_sphere - AnalytSphere, contains all the important information about the
 *               analytic sphere
 *
 * OUTPUTS:
 *  intersect - int, 1 or 0 depending on if the ray intersects the sphere.
 *
 */
void scatterSphere(Ray3D * the_ray, AnalytSphere the_sphere, double * const min_dist,
        double nearest_inter[3], double nearest_n[3], int * const tri_hit,
        int * const which_surface, int * const meets_sphere) {
    double a,b,c;
    double beta, gamma;
    double distance;
    double *e;
    double *d;

    /* Get the present position and direction of the ray */
    e = the_ray->position;
    d = the_ray->direction;

    /* Centre of the sphere */
    a = the_sphere.sphere_c[0];
    b = the_sphere.sphere_c[1];
    c = the_sphere.sphere_c[2];

    /* Coefficients of the quadratic equation */
    beta = 2*(d[0]*(e[0] - a) + d[1]*(e[1] - b) + d[2]*(e[2] - c));
    gamma = -the_sphere.sphere_r*the_sphere.sphere_r - 2*e[0]*a - 2*e[1]*b -
        2*e[2]*c + e[0]*e[0] + e[1]*e[1] + e[2]*e[2] + a*a + b*b + c*c;

    /* Do we hit the sphere */
    if (beta*beta - 4*gamma < 0) {
        *meets_sphere = 0;
        return;
    }

    /* Solve the quadratic equation. Take the smaller root */
    distance = (-beta - sqrt(beta*beta - 4*gamma))/2;

    if ((distance*distance < *min_dist) && (distance > 0)) {
        /* Shortest intersection yet found */

        /* Normal to the sphere at that point */
        nearest_n[0] = (e[0] + distance*d[0] - a)/the_sphere.sphere_r;
        nearest_n[1] = (e[1] + distance*d[1] - b)/the_sphere.sphere_r;
        nearest_n[2] = (e[2] + distance*d[2] - c)/the_sphere.sphere_r;

        /* Intersection with the sphere */
        nearest_inter[0] = e[0] + d[0]*distance;
        nearest_inter[1] = e[1] + d[1]*distance;
        nearest_inter[2] = e[2] + d[2]*distance;

        *min_dist = distance*distance;

        normalise(nearest_n);

        /* We are not on a triangle */
        *tri_hit = -1;

        /* We are now on the sphere */
        *which_surface = the_sphere.surf_index;

        /* We do hit the sphere and it is the closest interesction*/
        *meets_sphere = 1;
        return;
    }

    /* The sphere is not the closest intersection */
    *meets_sphere = 0;
    return;
}

/*
 * Finds the distance to, the normal to, and the position of a ray's intersection
 * with an triangulated surface.
 *
 * INPUTS:
 *
 *  current_tri     - int, the index of the triangle the current ray is on, -1
 *                    indicates that the ray is not on any triangle
 *  current_surface - int, an index stating which surface the ray is on,
 *                    0=sample, 1=pinhole plate, -1=none, -2=sphere
 *  min_dist        - double pointer, to store the minimum distance to a surface
 *                    in
 *  nearest_inter   - double array, to store the location of the nearest
 *                    intersection in
 *  nearest_n       - double array, to store the normal to the surface at the
 *                    nearest intersection
 *  meets           - int, 1 or 0 to store if the ray hits any surfaces
 *  tri_hit         - int pointer, to store the index of the triangle that the
 *                    ray hits (if it hits any)
 *  which_surface   - int pointer, which surface does the ray intersect (if any)
 *
 * NOTE: this function is messy as attempts (mostly successful) have been made
 *       to improve the speed of the simulation as this is the section of code
 *       called the highest number of times, hence the rather low level looking
 *       code.
 */
void scatterTriag(Ray3D * the_ray, Surface3D sample, double * const min_dist,
        double nearest_inter[3], double nearest_n[3], int * const meets, int * const tri_hit,
        int * const which_surface) {
    int j;
    double normal[3];
    double *e, *d;

    /* Position and direction of the ray */
    e = the_ray->position;
    d = the_ray->direction;

    /* Loop through all triangles in the surface */
    for (j = 0; j < sample.n_faces; j++) {
        double a[3];
        double b[3];
        double c[3];
        double AA[3][3];
        double v[3];
        double u[3] = {0, 0, 0};
        double epsilon;

        /* Skip this triangle if the ray is already on it */
        if ((the_ray->on_element == j) && (the_ray->on_surface == sample.surf_index)) {
            continue;
        }

        /*
         * Specify which triangle and get its normal.
         */
        get_element3D(&sample, j, a, b, c, normal);

        /* If the triangle is 'back-facing' then the ray cannot hit it */
        double test;
        dot(normal, d, &test);
        if (test > 0) {
            continue;
        }

        /*
         * If the triangle is behind the current ray position then the ray
         * cannot hit it. To do this we have to test each of the three vertices
         * to find if they are `behind' the ray. If any one of the vertices is
         * in-fornt of the ray we have to consider it. Re-use variable v.
         */
        v[0] = a[0] - e[0];
        v[1] = a[1] - e[1];
        v[2] = a[2] - e[2];
        if (v[0]*d[0] + v[1]*d[1] + v[2]*d[2] < 0) {
            v[0] = b[0] - e[0];
            v[1] = b[1] - e[1];
            v[2] = b[2] - e[2];
            if (v[0]*d[0] + v[1]*d[1] + v[2]*d[2] < 0) {
                v[0] = c[0] - e[0];
                v[1] = c[1] - e[1];
                v[2] = c[2] - e[2];
                if (v[0]*d[0] + v[1]*d[1] + v[2]*d[2] < 0) {
                    continue;
                }
            }
        }

        /*
         * Construct the linear equation
         * AA u = v, where u contains (alpha, beta, t) for the propagation
         * equation:
         * e + td = a + beta(b - a) + gamma(c - a)
         */
        //propagate3D(a, e, -1, v); // <- simpler to write, heavier computation
        v[0] = a[0] - e[0];
        v[1] = a[1] - e[1];
        v[2] = a[2] - e[2];

        /* This could be pre-calculated and stored, however it would involve an
         * array of matrices
         */
        AA[0][0] = a[0] - b[0];
        AA[0][1] = a[0] - c[0];
        AA[0][2] = d[0];
        AA[1][0] = a[1] - b[1];
        AA[1][1] = a[1] - c[1];
        AA[1][2] = d[1];
        AA[2][0] = a[2] - b[2];
        AA[2][1] = a[2] - c[2];
        AA[2][2] = d[2];

        /*
         * Tests to see if this triangle is parallel to the ray, if it is the
         * determinant of the matrix AA will be zero, we must set a tolerance for
         * size of determinant we will allow.
         */
        epsilon = 0.0000000001;
        int success = 0; // Default to no success, this was causing problems some how...
        solve3x3(AA, u, v, epsilon, &success); // <- NOTE: this is the biggest computation
        if (!success) {
            continue;
        }

        /* Find if the point of intersection is inside the triangle */
        /* Must also find if the ray is propagating forwards */
        if ((u[0] >= 0) && (u[1] >= 0) && ((u[0] + u[1]) <= 1) && (u[2] > 0)) {
            double new_loc[3];
            double movment[3];
            double dist;

            /* We have hit a triangle */
            *meets = 1; // <- I think I've found the problem....

            /* Store the location and normal of the nearest intersection */
            new_loc[0] = e[0] + (u[2]*d[0]);
            new_loc[1] = e[1] + (u[2]*d[1]);
            new_loc[2] = e[2] + (u[2]*d[2]);

            /* Movement is the vector from the current location to the possible
             * new location */
            movment[0] = new_loc[0] - e[0];
            movment[1] = new_loc[1] - e[1];
            movment[2] = new_loc[2] - e[2];

            /* NOTE: we are comparing the square of the distance */
            dist = movment[0]*movment[0] + movment[1]*movment[1] +
                movment[2]*movment[2];
            // Not good here!! :'(

            if (dist < *min_dist) {
                /* This is the smallest intersection found so far */
                *min_dist = dist;

                *tri_hit = j;
                nearest_n[0] = normal[0];
                nearest_n[1] = normal[1];
                nearest_n[2] = normal[2];
                nearest_inter[0] = new_loc[0];
                nearest_inter[1] = new_loc[1];
                nearest_inter[2] = new_loc[2];

                *which_surface = sample.surf_index;
            }
        }
    }
}

/*
 * Scatter off a back wall with n detectors. Returns 0 if the ray is not
 * detected or the index of the detector is has entered.
 */
void multiBackWall(Ray3D * the_ray, NBackWall wallPlate, double * const min_dist,
        double nearest_inter[3], double nearest_n[3], int * meets, int * const tri_hit,
        int * const which_surface, int * const which_aperture) {
    double *d;
    d = the_ray->direction;

    /*
     * If the ray is travelling in the positive y-direction then it may hit
     * the 'back wall'.
     */
    *meets = 0;
    if (d[1] > 0) {
        double alpha, x;
        double wall_hit[3];
        double backNormal[3];
        double test;
        double r, x_disp, y_disp;
        double *e;
        *which_aperture = 0;
        int i;

        e = the_ray->position;

        backNormal[0] = 0;
        backNormal[1] = -1;
        backNormal[2] = 0;

        /*
         * Find where the ray hits the back wall, the back wall is defined to be
         * in the plane y = 0
         */
        alpha = (0 - e[1])/d[1];

        /*
         * If the distance to the back wall is longer than a previous
         * intersection then the ray does not hit the back wall.
         */
        if (alpha*alpha > *min_dist) {
            *which_aperture = 0;
            return;
        }

        /* propagate the ray to that position */
        propagate(e, d, alpha, wall_hit);

        test = 10; /* Number greater than 1 */
        for (i = 0; i < wallPlate.n_detect; i++) {
            BackWall plate;
            get_nth_aperture(i, &wallPlate, &plate);
            /*
             * See if it goes into the aperture. The aperture is defined by a centre,
             * along with two axes. Uses the formula for an ellipse:
             *     (x-h)^2/a^2 + (z-k)^2/b^2 = 1
             * where h and k are the x and z positions of the centre of the ellipse.
             */
            x_disp = wall_hit[0] - plate.aperture_c[0];
            y_disp = wall_hit[2] - plate.aperture_c[1];
            test = x_disp*x_disp/
                (0.25*plate.aperture_axes[0]*plate.aperture_axes[0]) +
                y_disp*y_disp/
                (0.25*plate.aperture_axes[1]*plate.aperture_axes[1]);

            if (test < 1) {
                *which_aperture = i + 1;
                break;
            }
        }

        if (test < 1) {
            /* Goes into detector aperture */
            /* Check that the distance to the aperture is the smallest so far */
            if (alpha*alpha < *min_dist) {
                *min_dist = alpha*alpha;

                /* -1 is not being on a triangle */
                *tri_hit = -1;
                nearest_n[0] = backNormal[0];
                nearest_n[1] = backNormal[1];
                nearest_n[2] = backNormal[2];
                nearest_inter[0] = wall_hit[0];
                nearest_inter[1] = wall_hit[1];
                nearest_inter[2] = wall_hit[2];

                *which_surface = wallPlate.surf_index;

                /*
                 * If we have gone into the aperture then the ray is both dead and
                 * should be counted.
                 */
                return;
            }
        }

        /* If the position is not then we can scatter of the back wall if that is
         * what is wanted */
        x = wall_hit[0]*wall_hit[0] + wall_hit[2]*wall_hit[2];
        r = wallPlate.circle_plate_r;
        if ((x <=  r*r) && wallPlate.plate_represent) {
            /* We have met a surface */
            *meets = 1;

            if (alpha*alpha < *min_dist) {
                *min_dist = alpha*alpha;

                /* -1 is not being on a triangle */
                *tri_hit = -1;
                nearest_n[0] = backNormal[0];
                nearest_n[1] = backNormal[1];
                nearest_n[2] = backNormal[2];
                nearest_inter[0] = wall_hit[0];
                nearest_inter[1] = wall_hit[1];
                nearest_inter[2] = wall_hit[2];

                *which_surface = wallPlate.surf_index;
            }
        }
    }

    /* The ray has not been detected. */
    *which_aperture = 0;
    return;
}

/* Scatter off an abstract hemisphere */
/* TODO: fix this!! */
//void abstractScatter(Ray3D * the_ray, AbstractHemi const * detector, double * min_dist,
//        double nearest_inter[3], bool * meets, int *tri_hit, int * which_surface,
//		int * which_detector) {
//
//    /* No detection, do nothing, maybe code this eventually */
//    return;
//}


/*
 * Scatters the ray off a corrugated surface.
 *
 * TODO: make this
 */
