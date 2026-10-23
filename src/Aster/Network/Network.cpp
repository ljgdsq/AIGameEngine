#include "Aster/Network/Network.h"

#include "Aster/Core/Log.h"

#include <asio.hpp>
#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <string>

namespace Aster
{
    namespace
    {
        constexpr size_t kMaxHttpBody = 16u * 1024u * 1024u;
        constexpr int kMinTimeoutMs = 1;
        constexpr int kMaxTimeoutMs = 600000;

        std::once_flag g_CurlInitOnce;
        CURLcode g_CurlInitResult = CURLE_OK;

        int ClampTimeout(int timeoutMs)
        {
            return std::clamp(timeoutMs, kMinTimeoutMs, kMaxTimeoutMs);
        }

        size_t WriteBodyCallback(char* data, size_t size, size_t count, void* userdata)
        {
            auto* body = static_cast<std::string*>(userdata);
            const size_t bytes = size * count;
            if (body->size() + bytes > kMaxHttpBody)
            {
                return 0;
            }
            body->append(data, bytes);
            return bytes;
        }

        HttpResponse PerformHttp(const std::string& url, const std::string* postBody,
            const std::string& contentType, int timeoutMs)
        {
            HttpResponse response;
            if (url.empty())
            {
                response.Error = "URL is empty";
                return response;
            }

            std::string initError;
            if (!Network::Initialize(&initError))
            {
                response.Error = initError;
                return response;
            }

            CURL* curl = curl_easy_init();
            if (curl == nullptr)
            {
                response.Error = "curl_easy_init failed";
                return response;
            }

            const int timeout = ClampTimeout(timeoutMs);
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(timeout));
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(timeout));
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &WriteBodyCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.Body);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "AsterEngine/0.2");
            curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");

            curl_slist* headers = nullptr;
            std::string contentTypeHeader;
            if (postBody != nullptr)
            {
                if (!contentType.empty())
                {
                    contentTypeHeader = "Content-Type: " + contentType;
                    headers = curl_slist_append(headers, contentTypeHeader.c_str());
                    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                }
                curl_easy_setopt(curl, CURLOPT_POST, 1L);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postBody->data());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE,
                    static_cast<curl_off_t>(postBody->size()));
            }

            ASTER_LOG_DEBUG("Network", "HTTP %s %s (timeout %d ms)", postBody != nullptr ? "POST" : "GET",
                url.c_str(), timeout);
            const CURLcode result = curl_easy_perform(curl);
            if (result != CURLE_OK)
            {
                response.Error = curl_easy_strerror(result);
                ASTER_LOG_DEBUG("Network", "HTTP request failed: %s", response.Error.c_str());
            }
            else
            {
                long status = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
                response.Status = status;
                response.Ok = status >= 200 && status < 300;
                if (!response.Ok)
                {
                    response.Error = "HTTP status " + std::to_string(status);
                }
                ASTER_LOG_DEBUG("Network", "HTTP response %ld (%zu bytes)", status, response.Body.size());
            }

            if (headers != nullptr)
            {
                curl_slist_free_all(headers);
            }
            curl_easy_cleanup(curl);
            return response;
        }
    }

    bool Network::Initialize(std::string* error)
    {
        std::call_once(g_CurlInitOnce, [] { g_CurlInitResult = curl_global_init(CURL_GLOBAL_DEFAULT); });
        if (g_CurlInitResult != CURLE_OK)
        {
            if (error != nullptr)
            {
                *error = curl_easy_strerror(g_CurlInitResult);
            }
            return false;
        }
        return true;
    }

    HttpResponse Network::HttpGet(const std::string& url, int timeoutMs)
    {
        return PerformHttp(url, nullptr, {}, timeoutMs);
    }

    HttpResponse Network::HttpPost(const std::string& url, const std::string& body,
        const std::string& contentType, int timeoutMs)
    {
        return PerformHttp(url, &body, contentType, timeoutMs);
    }

    struct TcpSocket::Impl
    {
        asio::io_context Io;
        asio::ip::tcp::socket Socket{ Io };
    };

    TcpSocket::TcpSocket()
        : m_Impl(CreateScope<Impl>())
    {
    }

    TcpSocket::~TcpSocket()
    {
        Close();
    }

    void TcpSocket::Close()
    {
        if (!m_Impl)
        {
            return;
        }
        asio::error_code ignored;
        if (m_Impl->Socket.is_open())
        {
            m_Impl->Socket.close(ignored);
        }
        // Drain cancellation handlers while the locals captured by them are still valid.
        m_Impl->Io.restart();
        m_Impl->Io.run_for(std::chrono::milliseconds(5));
        m_Impl->Io.restart();
    }

    bool TcpSocket::IsOpen() const
    {
        return m_Impl && m_Impl->Socket.is_open();
    }

    bool TcpSocket::Connect(const std::string& host, uint16_t port, int timeoutMs, std::string* error)
    {
        Close();
        if (host.empty() || port == 0)
        {
            if (error != nullptr)
            {
                *error = "Host or port is invalid";
            }
            return false;
        }

        const int timeout = ClampTimeout(timeoutMs);
        asio::error_code resolveError;
        asio::ip::tcp::resolver resolver(m_Impl->Io);
        const auto endpoints = resolver.resolve(host, std::to_string(port), resolveError);
        if (resolveError)
        {
            if (error != nullptr)
            {
                *error = "Could not resolve '" + host + "': " + resolveError.message();
            }
            return false;
        }

        bool completed = false;
        asio::error_code connectError;
        asio::async_connect(m_Impl->Socket, endpoints,
            [&completed, &connectError](const asio::error_code& ec, const asio::ip::tcp::endpoint&)
            {
                connectError = ec;
                completed = true;
            });
        m_Impl->Io.restart();
        m_Impl->Io.run_for(std::chrono::milliseconds(timeout));
        if (!completed)
        {
            Close();
            if (error != nullptr)
            {
                *error = "Timed out connecting to " + host + ":" + std::to_string(port);
            }
            ASTER_LOG_DEBUG("Network", "TCP connect timed out: %s:%u", host.c_str(), port);
            return false;
        }
        if (connectError)
        {
            Close();
            if (error != nullptr)
            {
                *error = "Connect failed: " + connectError.message();
            }
            ASTER_LOG_DEBUG("Network", "TCP connect failed: %s", connectError.message().c_str());
            return false;
        }

        ASTER_LOG_DEBUG("Network", "TCP connected to %s:%u", host.c_str(), port);
        return true;
    }

    bool TcpSocket::Send(const void* data, size_t size, int timeoutMs, std::string* error)
    {
        if (!IsOpen())
        {
            if (error != nullptr)
            {
                *error = "Socket is not connected";
            }
            return false;
        }
        if (data == nullptr && size != 0)
        {
            if (error != nullptr)
            {
                *error = "Data pointer is null";
            }
            return false;
        }
        if (size == 0)
        {
            return true;
        }

        bool completed = false;
        asio::error_code sendError;
        size_t sent = 0;
        asio::async_write(m_Impl->Socket, asio::buffer(data, size),
            [&completed, &sendError, &sent](const asio::error_code& ec, size_t bytes)
            {
                sendError = ec;
                sent = bytes;
                completed = true;
            });
        m_Impl->Io.restart();
        m_Impl->Io.run_for(std::chrono::milliseconds(ClampTimeout(timeoutMs)));
        if (!completed)
        {
            Close();
            if (error != nullptr)
            {
                *error = "Timed out sending data";
            }
            return false;
        }
        if (sendError)
        {
            Close();
            if (error != nullptr)
            {
                *error = "Send failed: " + sendError.message();
            }
            return false;
        }

        ASTER_LOG_DEBUG("Network", "TCP sent %zu bytes", sent);
        return true;
    }

    bool TcpSocket::Receive(void* data, size_t capacity, size_t& received, int timeoutMs,
        std::string* error)
    {
        received = 0;
        if (!IsOpen())
        {
            if (error != nullptr)
            {
                *error = "Socket is not connected";
            }
            return false;
        }
        if (data == nullptr || capacity == 0)
        {
            if (error != nullptr)
            {
                *error = "Receive buffer is empty";
            }
            return false;
        }

        bool completed = false;
        asio::error_code receiveError;
        size_t bytes = 0;
        m_Impl->Socket.async_read_some(asio::buffer(data, capacity),
            [&completed, &receiveError, &bytes](const asio::error_code& ec, size_t count)
            {
                receiveError = ec;
                bytes = count;
                completed = true;
            });        m_Impl->Io.restart();
        m_Impl->Io.run_for(std::chrono::milliseconds(ClampTimeout(timeoutMs)));
        if (!completed)
        {
            Close();
            if (error != nullptr)
            {
                *error = "Timed out receiving data";
            }
            return false;
        }
        if (receiveError)
        {
            Close();
            if (error != nullptr)
            {
                *error = receiveError == asio::error::eof ? "Connection closed by peer"
                                                         : "Receive failed: " + receiveError.message();
            }
            return false;
        }

        received = bytes;
        ASTER_LOG_DEBUG("Network", "TCP received %zu bytes", bytes);
        return true;
    }
}
