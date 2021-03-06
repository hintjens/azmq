/*
    Copyright (c) 2013-2014 Contributors as noted in the AUTHORS file

    This file is part of azmq

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef AZMQ_DETAIL_SOCKET_OPS_HPP__
#define AZMQ_DETAIL_SOCKET_OPS_HPP__

#include "../error.hpp"
#include "../message.hpp"
#include "context_ops.hpp"

#include <boost/assert.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/socket_base.hpp>
#if ! defined BOOST_ASIO_WINDOWS
    #include <boost/asio/posix/stream_descriptor.hpp>
#else
    #include <boost/asio/ip/tcp.hpp>
#endif
#include <boost/system/error_code.hpp>

#include <zmq.h>

#include <iterator>
#include <memory>
#include <string>
#include <sstream>
#include <type_traits>

namespace azmq {
namespace detail {
    struct socket_ops {
        using endpoint_type = std::string;

        struct socket_close {
            void operator()(void* socket) {
                int v = 0;
                auto rc = zmq_setsockopt(socket, ZMQ_LINGER, &v, sizeof(int));
                BOOST_ASSERT_MSG(rc == 0, "set linger=0 on shutdown");
                zmq_close(socket);
            }
        };

        using raw_socket_type = void*;
        using socket_type = std::unique_ptr<void, socket_close>;
#if ! defined BOOST_ASIO_WINDOWS
        using native_handle_type = boost::asio::posix::stream_descriptor::native_handle_type;
        using stream_descriptor = std::unique_ptr<boost::asio::posix::stream_descriptor>;
#else
        using native_handle_type = boost::asio::ip::tcp::socket::native_handle_type;
        using stream_descriptor = std::unique_ptr<boost::asio::ip::tcp::socket>;
#endif
        using flags_type = message::flags_type;
        using more_result_type = std::pair<size_t, bool>;

        static socket_type create_socket(context_ops::context_type context,
                                         int type,
                                         boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(context, "Invalid context");
            auto res = zmq_socket(context.get(), type);
            if (!res) {
                ec = make_error_code();
                return socket_type();
            }
            return socket_type(res);
        }

        static stream_descriptor get_stream_descriptor(boost::asio::io_service & io_service,
                                                       socket_type & socket,
                                                       boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(socket, "invalid socket");
            native_handle_type handle = 0;
            auto size = sizeof(native_handle_type);
            stream_descriptor res;
            auto rc = zmq_getsockopt(socket.get(), ZMQ_FD, &handle, &size);
            if (rc < 0)
                ec = make_error_code();
            else {
#if ! defined BOOST_ASIO_WINDOWS
                res.reset(new boost::asio::posix::stream_descriptor(io_service, handle));
#else
                // Use duplicated SOCKET, because ASIO socket takes ownership over it so destroys one in dtor.
                ::WSAPROTOCOL_INFO pi;
                ::WSADuplicateSocket(handle, ::GetCurrentProcessId(), &pi);
                handle = ::WSASocket(pi.iAddressFamily/*AF_INET*/, pi.iSocketType/*SOCK_STREAM*/, pi.iProtocol/*IPPROTO_TCP*/, &pi, 0, 0);
                res.reset(new boost::asio::ip::tcp::socket(io_service, boost::asio::ip::tcp::v4(), handle));
#endif
            }
            return res;
        }

        static boost::system::error_code bind(socket_type & socket,
                                              endpoint_type const& ep,
                                              boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(socket, "invalid socket");
            auto rc = zmq_bind(socket.get(), ep.c_str());
            if (rc < 0)
                ec = make_error_code();
            return ec;
        }

        static boost::system::error_code connect(socket_type & socket,
                                                 endpoint_type const& ep,
                                                 boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(socket, "invalid socket");
            auto rc = zmq_connect(socket.get(), ep.c_str());
            if (rc < 0)
                ec = make_error_code();
            return ec;
        }

        template<typename Option>
        static boost::system::error_code set_option(socket_type & socket,
                                                    Option const& opt,
                                                    boost::system::error_code & ec) {
            auto rc = zmq_setsockopt(socket.get(), opt.name(), opt.data(), opt.size());
            if (rc < 0)
                ec = make_error_code();
            return ec;
        }

        template<typename Option>
        static boost::system::error_code get_option(socket_type & socket,
                                                    Option & opt,
                                                    boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(socket, "invalid socket");
            size_t size = opt.size();
            auto rc = zmq_getsockopt(socket.get(), opt.name(), opt.data(), &size);
            if (rc < 0)
                ec = make_error_code();
            return ec;
        }

        static int get_events(socket_type & socket,
                              boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(socket, "invalid socket");
            int evs = 0;
            size_t size = sizeof(evs);
            auto rc = zmq_getsockopt(socket.get(), ZMQ_EVENTS, &evs, &size);
            if (rc < 0)
                ec = make_error_code();
            return evs;
        }

        static size_t send(message const& msg,
                           socket_type & socket,
                           flags_type flags,
                           boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(socket, "Invalid socket");
            auto rc = zmq_msg_send(const_cast<zmq_msg_t*>(&msg.msg_), socket.get(), flags);
            if (rc < 0) {
                ec = make_error_code();
                return 0;
            }
            return rc;
        }

        static size_t send(boost::asio::const_buffer const& buf,
                           socket_type & socket,
                           flags_type flags,
                           boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(socket, "Invalid socket");
            auto pv = boost::asio::buffer_cast<void const*>(buf);
            auto len = boost::asio::buffer_size(buf);
            auto rc = zmq_send_const(socket.get(), const_cast<void*>(pv), len, flags);
            if (rc < 0) {
                ec = make_error_code();
                return 0;
            }
            return rc;
        }

        template<typename ConstBufferSequence>
        static size_t send(ConstBufferSequence const& buffers,
                           socket_type & socket,
                           flags_type flags,
                           boost::system::error_code & ec) {
            size_t res = 0;
            auto last = std::distance(std::begin(buffers), std::end(buffers)) - 1;
            auto index = 0u;
            for (auto it = std::begin(buffers); it != std::end(buffers); ++it, ++index) {
                auto f = index == last ? flags & ~ZMQ_SNDMORE
                                       : flags;
                res += send(*it, socket, f, ec);
                if (ec) return 0;
            }
            return res;
        }

        static size_t receive(message & msg,
                              socket_type & socket,
                              flags_type flags,
                              boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(socket, "Invalid socket");
            msg.rebuild();
            auto rc = zmq_msg_recv(const_cast<zmq_msg_t*>(&msg.msg_), socket.get(), flags);
            if (rc < 0) {
                ec = make_error_code();
                return 0;
            }
            return rc;
        }

        template<typename MutableBufferSequence>
        static size_t receive(MutableBufferSequence const& buffers,
                              socket_type & socket,
                              flags_type flags,
                              boost::system::error_code & ec) {
            size_t res = 0;
            message msg;
            flags_type f = flags & ~ZMQ_RCVMORE;
            for (auto&& buf : buffers) {
                auto sz = receive(msg, socket, f, ec);
                if (ec)
                    return 0;

                if (msg.buffer_copy(buf) < sz) {
                    ec = make_error_code(boost::system::errc::no_buffer_space);
                    return 0;
                }
                res += sz;
                f = flags;
            }

            if ((flags & ZMQ_RCVMORE) && msg.more()) {
                ec = make_error_code(boost::system::errc::no_buffer_space);
                return res;
            }
            return res;
        }

        static size_t receive_more(message_vector & vec,
                                   socket_type & socket,
                                   flags_type flags,
                                   boost::system::error_code & ec) {
            size_t res = 0;
            message msg;
            auto more = false;
            do {
                res += receive(msg, socket, more ? flags | ZMQ_RCVMORE
                                                 : flags, ec);
                if (ec) return 0;
                more = msg.more();
                vec.emplace_back(std::move(msg));
            } while (more);
            return res;
        }

        static std::string monitor(socket_type & socket,
                                   int events,
                                   boost::system::error_code & ec) {
            BOOST_ASSERT_MSG(socket, "Invalid socket");
            std::ostringstream stm;
            stm << "inproc://monitor-" << socket.get();
            auto addr = stm.str();
            auto rc = zmq_socket_monitor(socket.get(), addr.c_str(), events);
            if (rc < 0)
                ec = make_error_code();
            return addr;
        }
    };
} // namespace detail
} // namespace azmq
#endif // AZMQ_DETAIL_SOCKET_OPS_HPP__

