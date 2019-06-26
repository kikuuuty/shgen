#include "spherical_harmonics.h"

#include <future>

constexpr auto PI     = 3.1415926535897932384626433832795;
constexpr auto INV_PI = 1.0 / PI;

namespace
{
    double factorial(size_t n, size_t d = 1)
    {
        d = std::max(size_t(1), d);
        n = std::max(size_t(1), n);
        double r = 1.0;
        if (n == d) {
            // intentionally left blank
        } else if (n > d) {
            for ( ; n > d; n--) {
                r *= n;
            }
        } else {
            for ( ; d > n; d--) {
                r *= d;
            }
            r = 1.0 / r;
        }
        return r;
    }

    double computeTruncatedCosSh(size_t l)
    {
        if (l == 0) {
            return PI;
        } else if (l == 1) {
            return 2 * PI / 3;
        } else if (l & 1) {
            return 0;
        }
        const size_t l_2 = l / 2;
        double A0 = ((l_2 & 1) ? 1.0 : -1.0) / ((l + 2) * (l - 1));
        double A1 = factorial(l, l_2) / (factorial(l_2) * (1 << l));
        return 2 * PI * A0 * A1;
    }

    inline double sphereQuadrantArea(double x, double y)
    {
        return std::atan2(x * y, std::sqrt(x * x + y * y + 1));
    }

    double solidAngle(size_t dim, size_t u, size_t v)
    {
        const double iDim = 1.0f / dim;
        double s = ((u + 0.5) * 2 * iDim) - 1;
        double t = ((v + 0.5) * 2 * iDim) - 1;
        const double x0 = s - iDim;
        const double y0 = t - iDim;
        const double x1 = s + iDim;
        const double y1 = t + iDim;
        double solidAngle = sphereQuadrantArea(x0, y0)
                          - sphereQuadrantArea(x0, y1)
                          - sphereQuadrantArea(x1, y0)
                          + sphereQuadrantArea(x1, y1);
        return solidAngle;
    }
}

namespace ibl
{
    std::unique_ptr<math::double3[]> computeIrradianceSH3Bands(const Cubemap& cm)
    {
        const size_t numCoefs = 9;

        std::unique_ptr<math::double3[]> SH(new math::double3[numCoefs]{});
        std::unique_ptr<double[]> A(new double[numCoefs]{});

        const double c0 = computeTruncatedCosSh(0);
        const double c1 = computeTruncatedCosSh(1);
        const double c2 = computeTruncatedCosSh(2);
        A[0] = (INV_PI * INV_PI / 4)       * c0;
        A[1] = (INV_PI * INV_PI / 4) * 3   * c1;
        A[2] = (INV_PI * INV_PI / 4) * 3   * c1;
        A[3] = (INV_PI * INV_PI / 4) * 3   * c1;
        A[4] = (INV_PI * INV_PI / 4) * 15  * c2;
        A[5] = (INV_PI * INV_PI / 4) * 15  * c2;
        A[6] = (INV_PI * INV_PI /16) * 5   * c2;
        A[7] = (INV_PI * INV_PI / 4) * 15  * c2;
        A[8] = (INV_PI * INV_PI /16) * 15  * c2;

        struct State {
            math::double3 SH[9] = {};
        };

        auto proc = [&](State& state, size_t y, Cubemap::Face f, const Cubemap::Texel* data, size_t dim)
        {
            for (size_t x = 0 ; x < dim; ++x, ++data)
            {
                math::double3 s(cm.getDirectionFor(f, x, y));

                // sample a color
                math::double3 color(Cubemap::sampleAt(data));

                // take solid angle into account
                color *= solidAngle(dim, x, y);

                state.SH[0] += color * A[0];
                state.SH[1] += color * A[1] * s.y;
                state.SH[2] += color * A[2] * s.z;
                state.SH[3] += color * A[3] * s.x;
                state.SH[4] += color * A[4] * s.y * s.x;
                state.SH[5] += color * A[5] * s.y * s.z;
                state.SH[6] += color * A[6] * (3 * s.z * s.z - 1);
                state.SH[7] += color * A[7] * s.z * s.x;
                state.SH[8] += color * A[8] * (s.x * s.x - s.y * s.y);
            }
        };

        const size_t dim = cm.getDimensions();

        State states[6];

        std::future<void> results[6];

        for (size_t faceIndex = 0; faceIndex < 6; faceIndex++) {
            const Cubemap::Face f = (Cubemap::Face)faceIndex;

            results[faceIndex] = std::async(std::launch::async, [&cm, &states, f, &dim, &proc] {
                State& s = states[size_t(f)];
                Image& image(const_cast<Cubemap&>(cm).getImageForFace(f));

                for (size_t y = 0; y < dim; y++) {
                    Cubemap::Texel* data = static_cast<Cubemap::Texel*>(image.getPixelRef(0, y));
                    proc(s, y, f, data, dim);
                }
            });
        }

        // wait for all our threads to finish
        for (auto& ret : results) {
            ret.wait();
        }

        for (State& s : states) {
            for (size_t i = 0 ; i < numCoefs ; i++) {
                SH[i] += s.SH[i];
            }
        }
        return SH;
    }

    void renderPreScaledSH3Bands(Cubemap& cm, const std::unique_ptr<math::double3[]>& sh)
    {
        auto proc = [&](size_t y, Cubemap::Face f, Cubemap::Texel* data, size_t dim)
        {
            for (size_t x = 0 ; x < dim ; ++x, ++data) {
                math::double3 s(cm.getDirectionFor(f, x, y));
                math::double3 c = 0;
                c += sh[0];
                c += sh[1] * s.y;
                c += sh[2] * s.z;
                c += sh[3] * s.x;
                c += sh[4] * s.y * s.x;
                c += sh[5] * s.y * s.z;
                c += sh[6] * (3 * s.z * s.z - 1);
                c += sh[7] * s.z * s.x;
                c += sh[8] * (s.x * s.x - s.y * s.y);
                Cubemap::writeAt(data, Cubemap::Texel(c));
            }
        };

        const size_t dim = cm.getDimensions();

        std::future<void> results[6];

        for (size_t faceIndex = 0; faceIndex < 6; faceIndex++) {
            const Cubemap::Face f = (Cubemap::Face)faceIndex;

            results[faceIndex] = std::async(std::launch::async, [&cm, f, &dim, &proc] {
                Image& image(const_cast<Cubemap&>(cm).getImageForFace(f));

                for (size_t y = 0; y < dim; y++) {
                    Cubemap::Texel* data = static_cast<Cubemap::Texel*>(image.getPixelRef(0, y));
                    proc(y, f, data, dim);
                }
            });
        }

        // wait for all our threads to finish
        for (auto& ret : results) {
            ret.wait();
        }
    }
}
