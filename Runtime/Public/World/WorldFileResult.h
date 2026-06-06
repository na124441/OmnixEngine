#pragma once

#include "Runtime/Public/World/WorldFileError.h"

namespace Omnix
{
    struct WorldFileResult
    {
        WorldFileError error = WorldFileError::None;

        bool Success() const { return error == WorldFileError::None; }
        
        static WorldFileResult Ok() { return { WorldFileError::None }; }
        static WorldFileResult Fail(WorldFileError err) { return { err }; }
    };
}
