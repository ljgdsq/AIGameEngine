#pragma once

struct lua_State;

namespace Aster
{
    // Registers the global `Network` table and the TCP socket userdata metatable on a Lua state.
    void RegisterNetworkBindings(lua_State* state);
}
