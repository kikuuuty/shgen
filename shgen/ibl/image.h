#ifndef IMAGE_H__
#define IMAGE_H__

#include <cstdint>

#include <memory>

#include "vec3.h"

namespace ibl
{
    class Image
    {
    public:
        Image();
        Image(size_t w, size_t h, size_t stride = 0);
        Image(void* data, size_t w, size_t h);

        void reset();

        void set(Image const& image);

        void subset(Image const& image, size_t x, size_t y, size_t w, size_t h);

        bool isValid() const { return mData != nullptr; }

        size_t getWidth() const { return mWidth; }

        size_t getHeight() const { return mHeight; }

        size_t getBytesPerRow() const { return mBpr; }

        size_t getBytesPerPixel() const { return sizeof(math::float3); }

        void* getData() const { return mData; }

        void* getPixelRef(size_t x, size_t y) const { return static_cast<uint8_t*>(mData) + y * getBytesPerRow() + x * getBytesPerPixel(); }

    private:
        size_t mBpr;
        size_t mWidth;
        size_t mHeight;
        std::unique_ptr<uint8_t[]> mOwnedData;
        void* mData;
    };
}

#endif
