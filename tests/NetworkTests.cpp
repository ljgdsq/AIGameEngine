#include "TestFramework.h"

#include "Aster/Network/Network.h"
#include "Aster/Scene/Scene.h"
#include "Aster/Scripting/ScriptEngine.h"

#include <asio.hpp>

#include <cctype>
#include <functional>
#include <memory>
#include <string>
#include <thread>

using namespace Aster;

namespace
{
    std::string MakeHttpResponse(int status, const std::string& reason, const std::string& body)
    {
        return "HTTP/1.1 " + std::to_string(status) + " " + reason + "\r\nContent-Length: " +
            std::to_string(body.size()) + "\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n" + body;
    }

    class OneShotHttpServer
    {
    public:
        using Handler = std::function<std::string(const std::string& request)>;

        explicit OneShotHttpServer(Handler handler)
            : m_Acceptor(m_Io)
            , m_Handler(std::move(handler))
        {
            const asio::ip::tcp::endpoint endpoint(asio::ip::address_v4::loopback(), 0);
            m_Acceptor.open(endpoint.protocol());
            m_Acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
            m_Acceptor.bind(endpoint);
            m_Acceptor.listen();
            m_Port = m_Acceptor.local_endpoint().port();
            m_Thread = std::thread([this] { Run(); });
        }

        ~OneShotHttpServer()
        {
            asio::error_code ignored;
            m_Acceptor.close(ignored);
            if (m_Thread.joinable())
            {
                m_Thread.join();
            }
        }

        [[nodiscard]] uint16_t Port() const { return m_Port; }

    private:
        void Run()
        {
            try
            {
                asio::ip::tcp::socket socket(m_Io);
                m_Acceptor.accept(socket);
                std::string request;
                asio::read_until(socket, asio::dynamic_buffer(request), "\r\n\r\n");
                const size_t headerEnd = request.find("\r\n\r\n");
                const std::string lower = [&request, headerEnd]
                {
                    std::string text = request.substr(0, headerEnd);
                    for (char& character : text)
                    {
                        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
                    }
                    return text;
                }();
                const size_t lengthPos = lower.find("content-length:");
                if (lengthPos != std::string::npos)
                {
                    const size_t valueStart = lengthPos + 15;
                    const size_t valueEnd = lower.find("\r\n", valueStart);
                    const size_t length = static_cast<size_t>(
                        std::stoul(lower.substr(valueStart, valueEnd - valueStart)));
                    const size_t bodyStart = headerEnd + 4;
                    if (request.size() < bodyStart + length)
                    {
                        const size_t missing = bodyStart + length - request.size();
                        const size_t oldSize = request.size();
                        request.resize(oldSize + missing);
                        asio::read(socket, asio::buffer(request.data() + oldSize, missing));
                    }
                }
                const std::string response = m_Handler(request);
                asio::write(socket, asio::buffer(response));
                asio::error_code ignored;
                socket.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
            }
            catch (...)
            {
            }
        }

        asio::io_context m_Io;
        asio::ip::tcp::acceptor m_Acceptor;
        Handler m_Handler;
        uint16_t m_Port = 0;
        std::thread m_Thread;
    };

    class EchoServer
    {
    public:
        EchoServer(size_t expectedBytes, std::string reply)
            : m_Acceptor(m_Io)
            , m_ExpectedBytes(expectedBytes)
            , m_Reply(std::move(reply))
        {
            const asio::ip::tcp::endpoint endpoint(asio::ip::address_v4::loopback(), 0);
            m_Acceptor.open(endpoint.protocol());
            m_Acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
            m_Acceptor.bind(endpoint);
            m_Acceptor.listen();
            m_Port = m_Acceptor.local_endpoint().port();
            m_Thread = std::thread([this] { Run(); });
        }

        ~EchoServer()
        {
            asio::error_code ignored;
            m_Acceptor.close(ignored);
            if (m_Thread.joinable())
            {
                m_Thread.join();
            }
        }

        [[nodiscard]] uint16_t Port() const { return m_Port; }

    private:
        void Run()
        {
            try
            {
                asio::ip::tcp::socket socket(m_Io);
                m_Acceptor.accept(socket);
                std::string request(m_ExpectedBytes, '\0');
                asio::read(socket, asio::buffer(request.data(), request.size()));
                asio::write(socket, asio::buffer(m_Reply));
                asio::error_code ignored;
                socket.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
            }
            catch (...)
            {
            }
        }

        asio::io_context m_Io;
        asio::ip::tcp::acceptor m_Acceptor;
        size_t m_ExpectedBytes = 0;
        std::string m_Reply;
        uint16_t m_Port = 0;
        std::thread m_Thread;
    };
}

ASTER_TEST(Network, HttpGetLoopback)
{
    OneShotHttpServer server([](const std::string&)
    {
        return MakeHttpResponse(200, "OK", "hello network");
    });
    const HttpResponse response =
        Network::HttpGet("http://127.0.0.1:" + std::to_string(server.Port()) + "/hello", 5000);
    ASTER_CHECK(response.Ok);
    ASTER_CHECK_EQ(response.Status, 200L);
    ASTER_CHECK_EQ(response.Body, std::string("hello network"));
    ASTER_CHECK(response.Error.empty());
}

ASTER_TEST(Network, HttpPostSendsBodyAndContentType)
{
    std::shared_ptr<std::string> captured = std::make_shared<std::string>();
    OneShotHttpServer server([captured](const std::string& request)
    {
        *captured = request;
        return MakeHttpResponse(201, "Created", "created");
    });
    const HttpResponse response = Network::HttpPost(
        "http://127.0.0.1:" + std::to_string(server.Port()) + "/submit", R"({"name":"aster"})",
        "application/json", 5000);
    ASTER_CHECK(response.Ok);
    ASTER_CHECK_EQ(response.Status, 201L);
    ASTER_CHECK_EQ(response.Body, std::string("created"));
    ASTER_CHECK(captured->find("POST /submit") != std::string::npos);
    ASTER_CHECK(captured->find("Content-Type: application/json") != std::string::npos);
    ASTER_CHECK(captured->find(R"({"name":"aster"})") != std::string::npos);
}

ASTER_TEST(Network, TcpSocketEcho)
{
    EchoServer server(4, "pong");
    TcpSocket socket;
    std::string error;
    ASTER_CHECK(socket.Connect("127.0.0.1", server.Port(), 3000, &error));
    ASTER_CHECK(socket.IsOpen());
    ASTER_CHECK(socket.Send("ping", 4, 3000, &error));

    char buffer[16]{};
    size_t received = 0;
    ASTER_CHECK(socket.Receive(buffer, sizeof(buffer), received, 3000, &error));
    ASTER_CHECK_EQ(received, 4u);
    ASTER_CHECK_EQ(std::string(buffer, received), std::string("pong"));
    socket.Close();
    ASTER_CHECK(!socket.IsOpen());
}

ASTER_TEST(Network, LuaHttpAndTcpBindings)
{
    OneShotHttpServer httpServer([](const std::string&)
    {
        return MakeHttpResponse(200, "OK", "lua-http");
    });
    EchoServer echoServer(4, "pong");

    Scene scene("Network Lua");
    ScriptEngine scripts;
    std::string error;
    ASTER_CHECK(scripts.Initialize(scene, &error));

    const std::string source =
        "local response = Network.HttpGet('http://127.0.0.1:" +
        std::to_string(httpServer.Port()) +
        "/lua', 3000)\n"
        "assert(response.ok, response.error)\n"
        "assert(response.status == 200)\n"
        "assert(response.body == 'lua-http')\n"
        "local socket, err = Network.TcpConnect('127.0.0.1', " +
        std::to_string(echoServer.Port()) +
        ", 3000)\n"
        "assert(socket, err)\n"
        "assert(socket:Send('lua!'))\n"
        "local data = socket:Receive(16, 3000)\n"
        "assert(data == 'pong', data)\n"
        "socket:Close()\n"
        "assert(not socket:IsOpen())\n"
        "local nilSocket, portError = Network.TcpConnect('127.0.0.1', 0)\n"
        "assert(nilSocket == nil)\n"
        "assert(type(portError) == 'string')\n";
    ASTER_CHECK(scripts.RunString(source, &error));
}

ASTER_TEST(Network, FailureCasesReportErrors)
{
    const HttpResponse invalid = Network::HttpGet("not-a-url", 200);
    ASTER_CHECK(!invalid.Ok);
    ASTER_CHECK(!invalid.Error.empty());

    const HttpResponse refused = Network::HttpGet("http://127.0.0.1:1/", 500);
    ASTER_CHECK(!refused.Ok);
    ASTER_CHECK(!refused.Error.empty());

    TcpSocket socket;
    std::string error;
    ASTER_CHECK(!socket.Connect("127.0.0.1", 1, 300, &error));
    ASTER_CHECK(!error.empty());
    ASTER_CHECK(!socket.IsOpen());
}
