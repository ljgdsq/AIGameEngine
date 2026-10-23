#pragma once

#include "Aster/Core/Base.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace Aster
{
    struct HttpResponse
    {
        bool Ok = false;
        long Status = 0;
        std::string Body;
        std::string Error;
    };

    // Engine-owned HTTP client built on libcurl. Every call is synchronous and timeout-bounded so Lua
    // scripts cannot block the frame indefinitely. TLS uses the Windows Schannel backend.
    class Network
    {
    public:
        Network() = delete;

        static bool Initialize(std::string* error = nullptr);
        static HttpResponse HttpGet(const std::string& url, int timeoutMs = 5000);
        static HttpResponse HttpPost(const std::string& url, const std::string& body,
            const std::string& contentType = "application/json", int timeoutMs = 5000);
    };

    // Blocking TCP socket with per-operation timeouts, exposed to Lua as userdata. The implementation
    // (standalone Asio) is hidden so scripts and engine headers do not depend on Asio headers.
    class TcpSocket
    {
    public:
        TcpSocket();
        ~TcpSocket();

        TcpSocket(const TcpSocket&) = delete;
        TcpSocket& operator=(const TcpSocket&) = delete;

        [[nodiscard]] bool Connect(const std::string& host, uint16_t port, int timeoutMs,
            std::string* error = nullptr);
        [[nodiscard]] bool Send(const void* data, size_t size, int timeoutMs, std::string* error = nullptr);
        [[nodiscard]] bool Receive(void* data, size_t capacity, size_t& received, int timeoutMs,
            std::string* error = nullptr);
        void Close();
        [[nodiscard]] bool IsOpen() const;

    private:
        struct Impl;
        Scope<Impl> m_Impl;
    };
}
