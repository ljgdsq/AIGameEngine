#include "Aster/Network/NetworkLua.h"

#include "Aster/Core/Log.h"
#include "Aster/Network/Network.h"

#include <algorithm>
#include <memory>
#include <new>
#include <string>
#include <vector>

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace Aster
{
    namespace
    {
        constexpr const char* kSocketMetatable = "Aster.TcpSocket";
        constexpr int kDefaultTimeoutMs = 5000;
        constexpr size_t kMaxReceiveBytes = 1024u * 1024u;

        struct LuaTcpSocket
        {
            TcpSocket* Socket = nullptr;
        };

        void PushResponse(lua_State* state, const HttpResponse& response)
        {
            lua_createtable(state, 0, 4);
            lua_pushboolean(state, response.Ok ? 1 : 0);
            lua_setfield(state, -2, "ok");
            lua_pushinteger(state, static_cast<lua_Integer>(response.Status));
            lua_setfield(state, -2, "status");
            lua_pushlstring(state, response.Body.data(), response.Body.size());
            lua_setfield(state, -2, "body");
            lua_pushlstring(state, response.Error.data(), response.Error.size());
            lua_setfield(state, -2, "error");
        }

        int LuaHttpGet(lua_State* state)
        {
            const char* url = luaL_checkstring(state, 1);
            const int timeout = static_cast<int>(luaL_optinteger(state, 2, kDefaultTimeoutMs));
            PushResponse(state, Network::HttpGet(url, timeout));
            return 1;
        }

        int LuaHttpPost(lua_State* state)
        {
            const char* url = luaL_checkstring(state, 1);
            size_t bodySize = 0;
            const char* body = luaL_checklstring(state, 2, &bodySize);
            const char* contentType = luaL_optstring(state, 3, "application/json");
            const int timeout = static_cast<int>(luaL_optinteger(state, 4, kDefaultTimeoutMs));
            PushResponse(state,
                Network::HttpPost(url, std::string(body, bodySize), contentType, timeout));
            return 1;
        }

        TcpSocket* CheckSocket(lua_State* state)
        {
            auto* wrapper = static_cast<LuaTcpSocket*>(luaL_checkudata(state, 1, kSocketMetatable));
            if (wrapper == nullptr || wrapper->Socket == nullptr)
            {
                luaL_error(state, "TCP socket is closed");
            }
            return wrapper->Socket;
        }

        int LuaTcpConnect(lua_State* state)
        {
            const char* host = luaL_checkstring(state, 1);
            const lua_Integer portValue = luaL_checkinteger(state, 2);
            const int timeout = static_cast<int>(luaL_optinteger(state, 3, kDefaultTimeoutMs));
            if (portValue < 1 || portValue > 65535)
            {
                lua_pushnil(state);
                lua_pushliteral(state, "Port must be between 1 and 65535");
                return 2;
            }

            auto socket = std::make_unique<TcpSocket>();
            std::string error;
            if (!socket->Connect(host, static_cast<uint16_t>(portValue), timeout, &error))
            {
                lua_pushnil(state);
                lua_pushlstring(state, error.data(), error.size());
                return 2;
            }

            void* memory = lua_newuserdatauv(state, sizeof(LuaTcpSocket), 0);
            auto* wrapper = new (memory) LuaTcpSocket{ socket.release() };
            (void)wrapper;
            luaL_getmetatable(state, kSocketMetatable);
            lua_setmetatable(state, -2);
            return 1;
        }

        int LuaSocketSend(lua_State* state)
        {
            TcpSocket* socket = CheckSocket(state);
            size_t size = 0;
            const char* data = luaL_checklstring(state, 2, &size);
            const int timeout = static_cast<int>(luaL_optinteger(state, 3, kDefaultTimeoutMs));
            std::string error;
            if (!socket->Send(data, size, timeout, &error))
            {
                lua_pushnil(state);
                lua_pushlstring(state, error.data(), error.size());
                return 2;
            }
            lua_pushboolean(state, 1);
            return 1;
        }

        int LuaSocketReceive(lua_State* state)
        {
            TcpSocket* socket = CheckSocket(state);
            const lua_Integer requested = luaL_optinteger(state, 2, 4096);
            const size_t capacity = static_cast<size_t>(
                std::clamp<lua_Integer>(requested, 1, static_cast<lua_Integer>(kMaxReceiveBytes)));
            const int timeout = static_cast<int>(luaL_optinteger(state, 3, kDefaultTimeoutMs));

            std::vector<char> buffer(capacity);
            size_t received = 0;
            std::string error;
            if (!socket->Receive(buffer.data(), buffer.size(), received, timeout, &error))
            {
                lua_pushnil(state);
                lua_pushlstring(state, error.data(), error.size());
                return 2;
            }
            lua_pushlstring(state, buffer.data(), received);
            return 1;
        }

        int LuaSocketClose(lua_State* state)
        {
            if (auto* wrapper = static_cast<LuaTcpSocket*>(luaL_checkudata(state, 1, kSocketMetatable)))
            {
                if (wrapper->Socket != nullptr)
                {
                    wrapper->Socket->Close();
                }
            }
            return 0;
        }

        int LuaSocketIsOpen(lua_State* state)
        {
            auto* wrapper = static_cast<LuaTcpSocket*>(luaL_checkudata(state, 1, kSocketMetatable));
            lua_pushboolean(state, wrapper != nullptr && wrapper->Socket != nullptr &&
                    wrapper->Socket->IsOpen() ? 1 : 0);
            return 1;
        }

        int LuaSocketGc(lua_State* state)
        {
            auto* wrapper = static_cast<LuaTcpSocket*>(luaL_checkudata(state, 1, kSocketMetatable));
            if (wrapper != nullptr && wrapper->Socket != nullptr)
            {
                delete wrapper->Socket;
                wrapper->Socket = nullptr;
            }
            return 0;
        }
    }

    void RegisterNetworkBindings(lua_State* state)
    {
        luaL_newmetatable(state, kSocketMetatable);
        lua_pushcfunction(state, &LuaSocketGc);
        lua_setfield(state, -2, "__gc");
        lua_newtable(state);
        lua_pushcfunction(state, &LuaSocketSend);
        lua_setfield(state, -2, "Send");
        lua_pushcfunction(state, &LuaSocketReceive);
        lua_setfield(state, -2, "Receive");
        lua_pushcfunction(state, &LuaSocketClose);
        lua_setfield(state, -2, "Close");
        lua_pushcfunction(state, &LuaSocketIsOpen);
        lua_setfield(state, -2, "IsOpen");
        lua_setfield(state, -2, "__index");
        lua_pop(state, 1);

        lua_newtable(state);
        lua_pushcfunction(state, &LuaHttpGet);
        lua_setfield(state, -2, "HttpGet");
        lua_pushcfunction(state, &LuaHttpPost);
        lua_setfield(state, -2, "HttpPost");
        lua_pushcfunction(state, &LuaTcpConnect);
        lua_setfield(state, -2, "TcpConnect");
        lua_setglobal(state, "Network");

        ASTER_LOG_DEBUG("Script", "Registered Network HTTP/TCP Lua bindings");
    }
}
