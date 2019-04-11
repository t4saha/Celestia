// frustum.h
//
// Copyright (C) 2000-2008, Chris Laurel <claurel@shatters.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELMATH_FRUSTUM_H_
#define _CELMATH_FRUSTUM_H_

#include <Eigen/Core>
#include <Eigen/Geometry>

//#include <celmath/capsule.h>


class Frustum
{
 public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW

    typedef Eigen::Hyperplane<double, 3> PlaneType;

    Frustum(double fov, float aspectRatio, double nearDist);
    Frustum(double fov, float aspectRatio, double nearDist, double farDist);

    inline Eigen::Hyperplane<double, 3> plane(unsigned int which) const
    {
        return planes[which];
    }

    void transform(const Eigen::Matrix3d& m);
    void transform(const Eigen::Matrix4d& m);

    enum {
        Bottom    = 0,
        Top       = 1,
        Left      = 2,
        Right     = 3,
        Near      = 4,
        Far       = 5,
    };

    enum Aspect {
        Outside   = 0,
        Inside    = 1,
        Intersect = 2,
    };

    Aspect test(const Eigen::Vector3f& point) const;
    Aspect testSphere(const Eigen::Vector3f& center, float radius) const;
    Aspect testSphere(const Eigen::Vector3d& center, double radius) const;
//    Aspect testCapsule(const Capsulef&) const;

 private:
    void init(double, float, double, double);

    Eigen::Hyperplane<double, 3> planes[6];
    bool infinite;
};

#endif // _CELMATH_FRUSTUM_H_
