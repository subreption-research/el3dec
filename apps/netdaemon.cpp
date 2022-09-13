/*
 * Copyright (c) 2022 Subreption LLC. All rights reserved.
 * Author: 12dc8242df1be0d6f4b73d68166552197936a27074c004c8af95b27194ec584d
 *
 * Dual-licensed under the Subreption Ukraine Defense License (SUDL, version 1) and the  Server Side
 * Public License (SSPL, version 3). Both licenses are provided with this software distribution.
 */

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/algorithm/hex.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <el3dec/lib.hpp>
#include <el3dec/telemetry.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace logging = boost::log;
namespace po = boost::program_options;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using namespace logging::trivial;
src::severity_logger< severity_level > lg;

//------------------------------------------------------------------------------

// Report a failure
void
fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
    BOOST_LOG_SEV(lg, error) << ec.message();
}

void log_incoming_telemetry(El3Telemetry *el3tele)
{
    BOOST_LOG_SEV(lg, info) << boost::format(
        "UAV ID:%d Type:%d Time:%s Lat:%f Lon:%f Alt:%u Speed:%g VideoFreq:%d Rem:%d "
        "Camera: A:%g Z:%g P:%g"
        ) % el3tele->ID()
            % el3tele->Type()
            % el3tele->Timestamp()
            % el3tele->Latitude()
            % el3tele->Longitude()
            % el3tele->Altitude()
            % el3tele->Groundspeed()
            % el3tele->VideoFreq()
            % el3tele->RemainingFlightMinutes()
            % el3tele->CameraAngle()
            % el3tele->CameraAzimuth()
            % el3tele->CameraPosition();
}

class session : public std::enable_shared_from_this<session>
{
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;

public:
    // Take ownership of the socket
    explicit
    session(tcp::socket&& socket)
        : ws_(std::move(socket))
    {
    }

    // Get on the correct executor
    void
    run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        net::dispatch(ws_.get_executor(),
            beast::bind_front_handler(
                &session::on_run,
                shared_from_this()));
    }

    // Start the asynchronous operation
    void
    on_run()
    {
        // Set suggested timeout settings for the websocket
        ws_.set_option(
            websocket::stream_base::timeout::suggested(
                beast::role_type::server));

        // Set a decorator to change the Server of the handshake
        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res)
            {
                res.set(http::field::server, "el3dec_websocket_netdaemon");
            }));
        // Accept the websocket handshake
        ws_.async_accept(
            beast::bind_front_handler(
                &session::on_accept,
                shared_from_this()));
    }

    void
    on_accept(beast::error_code ec)
    {
        if (ec)
            return fail(ec, "accept");

        // Read a message
        do_read();
    }

    void
    do_read()
    {
        ws_.async_read(
            buffer_,
            beast::bind_front_handler(
                &session::on_read,
                shared_from_this()));
    }

    void
    on_read(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec == websocket::error::closed)
            return;

        if (ec)
            return fail(ec, "read");

        std::string instr(boost::asio::buffer_cast<const char*>(buffer_.data()), buffer_.size());

        unsigned char bytes[256];

        if (buffer_.size() < sizeof(bytes))
        {
            memset(bytes, 0, sizeof(bytes));

            std::string hexdata = boost::algorithm::unhex(instr); 
            std::copy(hexdata.begin(), hexdata.end(), bytes);

            BOOST_LOG_SEV(lg, debug) <<  boost::format("Recvd %u hex encoded bytes...") % buffer_.size();

            El3Telemetry *telemetry = el3Decode(bytes, buffer_.size(), FAULT_TOLERANT);
            log_incoming_telemetry(telemetry);

            ws_.text(ws_.got_text());
            ws_.async_write(
            boost::asio::buffer(telemetry->toJson(false)),
            beast::bind_front_handler(
                &session::on_write,
                shared_from_this()));

            delete telemetry;
        }
    }

    void
    on_write(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "write");

        // Clear the buffer
        buffer_.consume(buffer_.size());

        // Do another read
        do_read();
    }
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
    net::io_context& ioc_;
    tcp::acceptor acceptor_;

public:
    listener(
        net::io_context& ioc,
        tcp::endpoint endpoint)
        : ioc_(ioc)
        , acceptor_(ioc)
    {
        beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if(ec)
        {
            fail(ec, "open");
            return;
        }

        // Allow address reuse
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if(ec)
        {
            fail(ec, "set_option");
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if(ec)
        {
            fail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor_.listen(
            net::socket_base::max_listen_connections, ec);
        if(ec)
        {
            fail(ec, "listen");
            return;
        }
    }

    // Start accepting incoming connections
    void
    run()
    {
        do_accept();
    }

private:
    void
    do_accept()
    {
        // The new connection gets its own strand
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(
                &listener::on_accept,
                shared_from_this()));
    }

    void
    on_accept(beast::error_code ec, tcp::socket socket)
    {
        if(ec)
        {
            fail(ec, "accept");
        }
        else
        {
            BOOST_LOG_SEV(lg, info) << "Connection from " << socket.remote_endpoint().address().to_string();
            // Create the session and run it
            std::make_shared<session>(std::move(socket))->run();
        }

        // Accept another connection
        do_accept();
    }
};

//------------------------------------------------------------------------------

static void init_logging(void)
{
    logging::add_file_log
    (
        keywords::file_name = "el3dec_netdaemon_%N.log",
        keywords::rotation_size = 10 * 1024 * 1024,
        keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0),
        keywords::format = "[%TimeStamp%] EL3: %Message%"
    );

    logging::add_console_log(std::cout, boost::log::keywords::format = "[%TimeStamp%] EL3: %Message%");

    logging::core::get()->set_filter
    (
        logging::trivial::severity >= logging::trivial::info
    );
}

int main(int argc, char* argv[])
{
    po::options_description general_opts("General options");
    general_opts.add_options()
        ("help", "produce a help message")
        ;

    po::options_description server_opts("Server options");
    server_opts.add_options()
        ("address", po::value<std::string>(), "address (to listen for connections)")
        ("port", po::value<int>(), "port")
        ;

    po::options_description extra_opts("Backend options");
    extra_opts.add_options()
        ("num-threads", po::value<int>(), "the initial number of threads")
        ;

    po::options_description all_opts("Allowed options");
    all_opts.add(general_opts).add(server_opts).add(extra_opts);

    po::variables_map vm;
    po::store(parse_command_line(argc, argv, all_opts), vm);

    if (vm.count("help"))
    {
        std::cerr << all_opts;
        return EXIT_SUCCESS;
    }

    if (!vm.count("num-threads"))
    {
        std::cerr << "Please specify a maximum amount of threads";
        return EXIT_SUCCESS;
    }

    init_logging();
    logging::add_common_attributes();

    auto const address = net::ip::make_address(vm["address"].as<std::string>());
    auto const port = static_cast<unsigned short>(vm["port"].as<int>());
    auto const threads = std::max<int>(1, vm["num-threads"].as<int>());

    // The io_context is required for all I/O
    net::io_context ioc{threads};

    // Create and launch a listening port
    std::make_shared<listener>(ioc, tcp::endpoint{address, port})->run();

    net::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait(
        [&](beast::error_code const&, int)
        {
            BOOST_LOG_SEV(lg, info) << "Exiting...";
            // Stop the `io_context`. This will cause `run()`
            // to return immediately, eventually destroying the
            // `io_context` and all of the sockets in it.
            ioc.stop();
        });

    BOOST_LOG_SEV(lg, info) <<  boost::format("Listening (%d threads)") % threads;

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for(auto i = threads - 1; i > 0; --i)
        v.emplace_back(
        [&ioc]
        {
            ioc.run();
        });

    ioc.run();

    for(auto& t : v)
        t.join();

    return EXIT_SUCCESS;
}