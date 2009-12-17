#include "renderer.h"
#include "surfaces.h"

boost::shared_ptr<Geometry> createPatch(const Options& opts,
                                       const Vec3& a, const Vec3& b,
                                       const Vec3& c, const Vec3& d,
                                       const Mat4& trans = Mat4())
{
    boost::shared_ptr<Geometry> patch(new Patch(opts, a,b,c,d));
    patch->transform(trans);
    return patch;
}

void addCube(const Options& opts, Renderer& r, const Mat4& otow)
{
    r.add(createPatch(opts, Vec3(-1,-1,-1), Vec3(-1,-1,1),
                            Vec3(-1,1,-1), Vec3(-1,1,1), otow) );
    r.add(createPatch(opts, Vec3(1,-1,-1), Vec3(1,-1,1),
                            Vec3(1,1,-1), Vec3(1,1,1), otow) );

    r.add(createPatch(opts, Vec3(-1,-1,-1), Vec3(-1,-1,1),
                            Vec3(1,-1,-1), Vec3(1,-1,1), otow) );
    r.add(createPatch(opts, Vec3(-1,1,-1), Vec3(-1,1,1),
                            Vec3(1,1,-1), Vec3(1,1,1), otow) );

    r.add(createPatch(opts, Vec3(-1,-1,-1), Vec3(-1,1,-1),
                            Vec3(1,-1,-1), Vec3(1,1,-1), otow) );
    r.add(createPatch(opts, Vec3(-1,-1,1), Vec3(-1,1,1),
                            Vec3(1,-1,1), Vec3(1,1,1), otow) );
}

int main()
{
    Options opts;
    opts.xRes = 1024;
    opts.yRes = 1024;
    opts.gridSize = 8;
    opts.shadingRate = 1;
    opts.smoothShading = true;
    opts.clipNear = 0.1;

    Mat4 camToScreen =
//        Mat4();
//        perspectiveProjection(90, opts.clipNear, opts.clipFar);
        screenWindow(0,0.5, 0,0.5);

    Renderer r(opts, camToScreen);

//    r.add(createPatch(opts, Vec3(-0.5,-0.5,0), Vec3(0.5,-0.5,0),
//                            Vec3(-0.5,0.5,0), Vec3(0.5,0.5,0),
//                            Mat4().setAxisAngle(Vec3(1,0,0), deg2rad(50))
//                            * Mat4().setTranslation(Vec3(0,0,1))));

    r.add(createPatch(opts, Vec3(0.2,0.2,5), Vec3(0.5,-0.5,1),
                            Vec3(-0.5,0.5,1), Vec3(0.5,0.5,5)) );

    // Cube geometry
//    addCube(opts, r,
//        Mat4().setAxisAngle(Vec3(0,1,0), deg2rad(45)) *
////        Mat4().setAxisAngle(Vec3(0,1,0), deg2rad(45)) *
//        Mat4().setTranslation(Vec3(0,0,1.4))
//    );

    r.render();

    return 0;
}