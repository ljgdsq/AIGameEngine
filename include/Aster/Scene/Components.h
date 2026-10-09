#pragma once

#include <Aster/Math/Vector3.h>

namespace Aster
{
    struct TransformComponent
    {
        Vector3 Translation{};
        Vector3 Rotation{};
        Vector3 Scale{ 1.0F, 1.0F, 1.0F };
    };
}