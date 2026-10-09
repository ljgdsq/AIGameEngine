#pragma once

namespace Aster
{
    struct Vector3
    {
        float X = 0.0F;
        float Y = 0.0F;
        float Z = 0.0F;

        friend constexpr bool operator==(const Vector3&, const Vector3&) = default;
    };
}