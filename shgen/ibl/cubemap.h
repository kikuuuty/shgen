#ifndef CUBEMAP_H__
#define CUBEMAP_H__

#include <cstdint>

#include <algorithm>

#include "image.h"
#include "vec3.h"

namespace ibl
{
    class Cubemap
    {
    public:
        explicit Cubemap(size_t dim);

        Cubemap(Cubemap&&) = default;
        Cubemap& operator=(Cubemap&&) = default;

        ~Cubemap();

        enum class Face : uint8_t
        {
            NX = 0,     // left            +----+
            PX,         // right           | PY |
            NY,         // bottom     +----+----+----+----+
            PY,         // top        | NX | PZ | PX | NZ |
            NZ,         // back       +----+----+----+----+
            PZ          // front           | NY |
                        //                 +----+
        };

        using Texel = math::float3;

        void resetDimensions(size_t dim);

        void setImageForFace(Face face, const Image& image);

        Image& getImageForFace(Face face) { return mFaces[int(face)]; }
        const Image& getImageForFace(Face face) const { return mFaces[int(face)]; }

        math::double3 getDirectionFor(Face face, size_t x, size_t y) const { return getDirectionFor(face, x + 0.5, y + 0.5); }

        math::double3 getDirectionFor(Face face, double x, double y) const;

        const Texel& sampleAt(const math::double3& direction) const;

        static const Texel& sampleAt(const void* data) { return *static_cast<const Texel*>(data); }

        static void writeAt(void* data, const Texel& texel) { *static_cast<Texel*>(data) = texel; }
        
        size_t getDimensions() const { return mDimensions; }

        struct Address
        {
            Face face;
            double s = 0;
            double t = 0;
        };

        static Address getAddressFor(const math::double3& direction);

    private:
        size_t mDimensions = 0;
        double mScale = 1;
        double mUpperBound = 0;
        Image mFaces[6];
    };

    inline math::double3 Cubemap::getDirectionFor(Face face, double x, double y) const
    {
        // map [0, dim] to [-1,1] with (-1,-1) at bottom left
        double cx = (x * mScale) - 1;
        double cy = 1 - (y * mScale);

        math::double3 dir;
        const double l = std::sqrt(cx * cx + cy * cy + 1);
        switch (face) {
        case Face::PX: dir = {  1, cy, -cx}; break;
        case Face::NX: dir = { -1, cy,  cx}; break;
        case Face::PY: dir = { cx,  1, -cy}; break;
        case Face::NY: dir = { cx, -1,  cy}; break;
        case Face::PZ: dir = { cx, cy,   1}; break;
        case Face::NZ: dir = {-cx, cy,  -1}; break;
        }
        return dir * (1 / l);
    }

    inline const Cubemap::Texel& Cubemap::sampleAt(const math::double3& direction) const
    {
        Cubemap::Address addr(getAddressFor(direction));
        const size_t x = std::min(size_t(addr.s * mDimensions), mDimensions - 1);
        const size_t y = std::min(size_t(addr.t * mDimensions), mDimensions - 1);
        return sampleAt(getImageForFace(addr.face).getPixelRef(x, y));
    }
}

#endif
