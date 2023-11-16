//  SPDX-FileCopyrightText: 2023 Alexander Sholokhov <ra9yer@yahoo.com>
//  SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "buffer.hpp"

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

struct StreamItem
{
    StreamItem(int stream_id)
        : unique_stream_id(stream_id){};

    int unique_stream_id; // 0 means unused
    std::mutex mtx{};     // common mutex to protect access to any of buffers
    std::condition_variable signal{};
    CBuffer buffer{1024 * 1024}; // 1Mb should be enough
};

// we use deque because it allows to store objects with deleted copy constructor
typedef std::deque<StreamItem> StreamsWithinChannel;

struct UdpRxContext
{
    UdpRxContext(int socket, size_t number_of_channels)
        : sock(socket), channels(number_of_channels)
    {
    }
    UdpRxContext() = delete;
    UdpRxContext(UdpRxContext &) = delete;
    UdpRxContext &operator=(UdpRxContext const &) = delete;

    bool is_alive() const
    {
        return sock != -1 && !flag_stop;
    }

    void stop_working_thread_close_socket(); // The only correct way to stop attached thread

    int sock;
    std::vector<StreamsWithinChannel> channels; // possible number of elements in the vector: 1,2,4
    std::mutex mtx_channel{};                   // mutex to protect multiple modify access to channels
    std::thread thr{};
    bool flag_stop{false};
    bool rx_active{false};
    void (*log_debug_print)(std::string const &){}; // function to print string to log.
};

// Special class wrapper to initiate rx thread stop on destroing.
class UdpRxContextDefer
{
  public:
    UdpRxContextDefer(std::shared_ptr<UdpRxContext> ctx)
        : _ctx(ctx)
    {
    }
    ~UdpRxContextDefer()
    {
        _ctx->stop_working_thread_close_socket();
    }
    std::shared_ptr<UdpRxContext> get_ctx()
    {
        return _ctx;
    }

  private:
    std::shared_ptr<UdpRxContext> _ctx;
};

class UdpRxError : public std::runtime_error
{
  public:
    UdpRxError(const std::string &what = "")
        : std::runtime_error(what)
    {
    }
};

// Util class with two static methods.
class UdpRxControl
{
    UdpRxControl() = delete;
    UdpRxControl(UdpRxControl &) = delete;

  public:
    static std::shared_ptr<UdpRxContext> start_thread(size_t number_of_channels, std::string const &bind_address, int bind_port,
                                                      void (*log_debug_print)(std::string const &));
    static void stop_thread(std::shared_ptr<UdpRxContext> ctx);
};
