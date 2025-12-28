#pragma once
#include "sdk.h"
// boost.beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <chrono>
#include <iostream>

namespace http_server {

    namespace net = boost::asio;
    using tcp = net::ip::tcp;
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace json = boost::json;

    // Вспомогательная функция для генерации timestamp
    inline std::string GetCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &time_t);
#else
        localtime_r(&time_t, &tm);
#endif

        char buffer[100];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tm);

        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::string result = buffer;
        result += "." + std::to_string(ms.count());
        return result;
    }

    class SessionBase {
    public:
        // Запрещаем копирование и присваивание объектов SessionBase и его наследников
        SessionBase(const SessionBase&) = delete;
        SessionBase& operator=(const SessionBase&) = delete;
        void Run();

        template <typename Body, typename Fields>
        void Write(http::response<Body, Fields>&& response) {
            // Запись выполняется асинхронно, поэтому response перемещаем в область кучи
            auto safe_response = std::make_shared<http::response<Body, Fields>>(std::move(response));

            auto self = GetSharedThis();
            http::async_write(stream_, *safe_response,
                [safe_response, self](beast::error_code ec, std::size_t bytes_written) {
                    self->OnWrite(safe_response->need_eof(), ec, bytes_written);
                });
        }

        std::string GetRemoteIP() const {
            try {
                return stream_.socket().remote_endpoint().address().to_string();
            }
            catch (...) {
                return "unknown";
            }
        }

    protected:
        explicit SessionBase(tcp::socket&& socket)
            : stream_(std::move(socket)) {
        }
        using HttpRequest = http::request<http::string_body>;

        ~SessionBase() = default;
    private:
        void Read();
        void OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read);
        void OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written);
        void Close();
        virtual void HandleRequest(HttpRequest&& request) = 0;
        virtual std::shared_ptr<SessionBase> GetSharedThis() = 0;

        void ReportError(beast::error_code ec, std::string_view where) {
            // Логируем ошибку в формате JSON
            json::value log_entry = {
                {"timestamp", GetCurrentTimestamp()},
                {"message", "error"},
                {"data", {
                    {"code", ec.value()},
                    {"text", ec.message()},
                    {"where", std::string(where)}
                }}
            };
            std::cout << json::serialize(log_entry) << std::endl;

            std::cerr << where << ": " << ec.message() << std::endl;
        }

        // tcp_stream содержит внутри себя сокет и добавляет поддержку таймаутов
        beast::tcp_stream stream_;
        beast::flat_buffer buffer_;
        HttpRequest request_;
    };

    template <typename RequestHandler>
    class Session : public SessionBase, public std::enable_shared_from_this<Session<RequestHandler>> {
    public:
        template <typename Handler>
        Session(tcp::socket&& socket, Handler&& request_handler)
            : SessionBase(std::move(socket))
            , request_handler_(std::forward<Handler>(request_handler)) {
        }
    private:
        std::shared_ptr<SessionBase> GetSharedThis() override;
        void HandleRequest(HttpRequest&& request) override;

        RequestHandler request_handler_;
    };

    template <typename RequestHandler>
    class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
    public:
        template <typename Handler>
        Listener(net::io_context& ioc, const tcp::endpoint& endpoint, Handler&& request_handler)
            : ioc_(ioc)
            // Обработчики асинхронных операций acceptor_ будут вызываться в своём strand
            , acceptor_(net::make_strand(ioc))
            , request_handler_(std::forward<Handler>(request_handler)) {
            // Открываем acceptor, используя протокол (IPv4 или IPv6), указанный в endpoint
            acceptor_.open(endpoint.protocol());

            // После закрытия TCP-соединения сокет некоторое время может считаться занятым,
            // чтобы компьютеры могли обменяться завершающими пакетами данных.
            // Однако это может помешать повторно открыть сокет в полузакрытом состоянии.
            // Флаг reuse_address разрешает открыть сокет, когда он "наполовину закрыт"
            acceptor_.set_option(net::socket_base::reuse_address(true));
            // Привязываем acceptor к адресу и порту endpoint
            acceptor_.bind(endpoint);
            // Переводим acceptor в состояние, в котором он способен принимать новые соединения
            // Благодаря этому новые подключения будут помещаться в очередь ожидающих соединений
            acceptor_.listen(net::socket_base::max_listen_connections);
        }

        void Run() {
            DoAccept();
        }
    private:
        void DoAccept() {
            acceptor_.async_accept(
                net::make_strand(ioc_),
                beast::bind_front_handler(&Listener::OnAccept, this->shared_from_this()));
        }
        void AsyncRunSession(tcp::socket&& socket);

        void OnAccept(beast::error_code ec, tcp::socket socket) {
            if (ec) {
                // Обработка ошибки
                return;
            }
            else {
                AsyncRunSession(std::move(socket));
            }
            DoAccept();
        }


        net::io_context& ioc_;
        tcp::acceptor acceptor_;
        RequestHandler request_handler_;
    };

    template <typename RequestHandler>
    void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler) {
        // При помощи decay_t исключим ссылки из типа RequestHandler,
        // чтобы Listener хранил RequestHandler по значению
        using MyListener = Listener<std::decay_t<RequestHandler>>;

        std::make_shared<MyListener>(ioc, endpoint, std::forward<RequestHandler>(handler))->Run();
    }

    template<typename RequestHandler>
    inline std::shared_ptr<SessionBase> Session<RequestHandler>::GetSharedThis() {
        return this->shared_from_this();
    }

    template<typename RequestHandler>
    inline void Session<RequestHandler>::HandleRequest(HttpRequest&& request) {
        // Захватываем умный указатель на текущий объект Session в лямбде,
        // чтобы продлить время жизни сессии до вызова лямбды.
        // Используется generic-лямбда функция, способная принять response произвольного типа
        request_handler_(std::move(request), [self = this->shared_from_this()](auto&& response) {
            self->Write(std::move(response));
            });
    }

    template<typename RequestHandler>
    inline void Listener<RequestHandler>::AsyncRunSession(tcp::socket&& socket) {
        std::make_shared<Session<RequestHandler>>(std::move(socket), request_handler_)->Run();
    }

}  // namespace http_server