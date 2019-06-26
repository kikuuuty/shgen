#include "cubemap.h"

namespace ibl
{
    Cubemap::Cubemap(size_t dim)
    {
        resetDimensions(dim);
    }

    Cubemap::~Cubemap() = default;

    void Cubemap::resetDimensions(size_t dim)
    {
        mDimensions = dim;
        mScale = 2.0 / dim;
        mUpperBound = std::nextafter(mDimensions, 0);
        for (auto& mFace : mFaces) {
            mFace.reset();
        }
    }

    void Cubemap::setImageForFace(Face face, const Image& image)
    {
        mFaces[size_t(face)].set(image);
    }

    Cubemap::Address Cubemap::getAddressFor(const math::double3& r)
    {
        Cubemap::Address addr;
        double sc, tc, ma;
        const double rx = std::abs(r.x);
        const double ry = std::abs(r.y);
        const double rz = std::abs(r.z);
        if (rx >= ry && rx >= rz) {
            ma = rx;
            if (r.x >= 0) {
                addr.face = Face::PX;
                sc = -r.z;
                tc = -r.y;
            } else {
                addr.face = Face::NX;
                sc =  r.z;
                tc = -r.y;
            }
        } else if (ry >= rx && ry >= rz) {
            ma = ry;
            if (r.y >= 0) {
                addr.face = Face::PY;
                sc =  r.x;
                tc =  r.z;
            } else {
                addr.face = Face::NY;
                sc =  r.x;
                tc = -r.z;
            }
        } else {
            ma = rz;
            if (r.z >= 0) {
                addr.face = Face::PZ;
                sc =  r.x;
                tc = -r.y;
            } else {
                addr.face = Face::NZ;
                sc = -r.x;
                tc = -r.y;
            }
        }
        // ma is guaranteed to be >= sc and tc
        addr.s = (sc / ma + 1) * 0.5f;
        addr.t = (tc / ma + 1) * 0.5f;
        return addr;
    }
}
