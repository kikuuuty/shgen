#ifndef SPHERICALHARMONICS_H__
#define SPHERICALHARMONICS_H__

#include <cstdint>
#include <memory>

#include "cubemap.h"

namespace ibl
{
    std::unique_ptr<math::double3[]> computeIrradianceSH3Bands(const Cubemap& cm);

    void renderPreScaledSH3Bands(Cubemap& cm, const std::unique_ptr<math::double3[]>& sh);
}

#endif
