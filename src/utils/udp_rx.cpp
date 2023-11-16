//  SPDX-FileCopyrightText: 2023 Alexander Sholokhov <ra9yer@yahoo.com>
//  SPDX-License-Identifier: GPL-3.0-or-later
#include "udp_rx.hpp"

#include <cstring>
#include <sstream>
#include <thread>
#include <vector>

#include "inet_common.h"

constexpr size_t num_data_bytes_in_block = 1024;
constexpr size_t num_bytes_expected = num_data_bytes_in_block + 4; // 1028

constexpr size_t max_num_elements_in_block = num_data_bytes_in_block / 2; // I/Q (2 bytes)

// one element is I or Q (2 bytes)

static void net_recv_operation(std::shared_ptr<UdpRxContext> ctx)
{
    struct sockaddr_in client_addr;
    std::vector<unsigned char> rx_buf(num_bytes_expected);

    // result buffers
    std::vector<short> buf0(max_num_elements_in_block);
    std::vector<short> buf1(max_num_elements_in_block);
    std::vector<short> buf2(max_num_elements_in_block);
    std::vector<short> buf3(max_num_elements_in_block);

    // to access by index
    short *arr_buf[4] = {buf0.data(), buf1.data(), buf2.data(), buf3.data()};

    const size_t num_of_channels = ctx->channels.size();

    rx_buf.resize(num_bytes_expected);

    fd_set readfds;

    for (;;)
    {
        std::memset(&client_addr, 0, sizeof(client_addr));
        socklen_t client_addr_len = sizeof(client_addr);

        int sock = ctx->sock;
        // check for invalid socket
        if (sock == -1)
        {
            break;
        }
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        struct timeval tv = {0, 200000}; // 0.2 seconds delay
        int ret = select(sock + 1, &readfds, NULL, NULL, &tv);

        // check for force stop condition
        if (ctx->flag_stop)
        {
            break;
        }

        // check for invalid socket
        if (ctx->sock == -1)
        {
            break;
        }

        // check for timeout
        if (ret == 0)
        {
            continue;
        }

        // Read data. The call must be nonblocked because select told us we can read data.
        int bytes_did_read =
            recvfrom(ctx->sock, (char *)&rx_buf[0], (int)rx_buf.size(), 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if (bytes_did_read < 0)
        {
            if (ctx->log_debug_print)
            {
                ctx->log_debug_print("reading error...");
            }
            break;
        }

        // check for force stop condition again
        if (ctx->flag_stop)
        {
            break;
        }

        if (bytes_did_read != num_bytes_expected)
        {
            if (ctx->log_debug_print)
            {
                std::ostringstream ss;
                ss << "Num bytes expected=" << num_bytes_expected << ", num bytes read=" << bytes_did_read;
                ctx->log_debug_print(ss.str());
            }
            continue;
        }

        if (!ctx->rx_active)
        {
            // no need to do data processing (dummy read)
            continue;
        }

        short *buf = (short *)&rx_buf[4]; // skip 4 bytes (marker and packet count)

        size_t pos = 0; // position in buf0, buf1, buf2, buf3

        for (size_t idx = 0; idx < max_num_elements_in_block; /* no inc */)
        {
            for (size_t channel = 0; channel < num_of_channels; channel++)
            {
                // Take I+Q pair from UDP rx stream and put to specified channel's buffer
                const short I = buf[idx];
                const short Q = buf[idx + 1];
                arr_buf[channel][pos] = I;
                arr_buf[channel][pos + 1] = Q;

                idx += 2; // step on one I+Q pair (2 shorts)
            }
            pos += 2; // step on one I+Q pair
        }

        // pos - number of elements in each result buffer

        // transfer from result buffers to buffers in context
        for (size_t channel = 0; channel < num_of_channels; channel++)
        {
            for (auto &stream : ctx->channels[channel])
            {
                // only to active streams
                if (stream.unique_stream_id)
                {
                    std::unique_lock<std::mutex> lock(stream.mtx); // protect buffer
                    stream.buffer.put(arr_buf[channel], pos);      // put data to each stream within same channels
                }
            }
        }

        // Notify them all.
        for (size_t channel = 0; channel < num_of_channels; channel++)
        {
            for (auto &stream : ctx->channels[channel])
            {
                if (stream.unique_stream_id)
                {
                    stream.signal.notify_one();
                }
            }
        }
    }

    if (ctx->sock != -1)
    {
        closesocket(ctx->sock);
        ctx->sock = -1;
    }

    if (ctx->log_debug_print)
    {
        ctx->log_debug_print("Exit RX thread");
    }
}

void UdpRxContext::stop_working_thread_close_socket()
{
    if (sock != -1)
    {
        if (log_debug_print)
        {
            log_debug_print("in stop_working_thread_close_socket");
        }

        flag_stop = true;
        closesocket(sock);
        sock = -1;
    }

    // wait thread exit
    if (thr.joinable())
    {
        thr.join();
    }
}

std::shared_ptr<UdpRxContext> UdpRxControl::start_thread(size_t number_of_channels, std::string const &bind_address, int bind_port,
                                                         void (*log_debug_print)(std::string const &))
{
    int sock = (int)socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        throw UdpRxError("Socket error");
    }
    const char reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));

    struct sockaddr_in sockaddr;
    std::memset(&sockaddr, 0, sizeof(sockaddr));

    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(bind_port);

    if (inet_pton(AF_INET, bind_address.c_str(), &(sockaddr.sin_addr)) != 1)
    {
        closesocket(sock);
        std::stringstream ss;
        ss << "inet_pton error. address='" << bind_address << "'.";
        throw UdpRxError(ss.str());
    }

    if (bind(sock, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0)
    {
        closesocket(sock);
        std::stringstream ss;
        ss << "Bind error. address='" << bind_address << "' port=" << bind_port << " : " << strerror(errno);
        throw UdpRxError(ss.str());
    }

    auto ctx = std::make_shared<UdpRxContext>(sock, number_of_channels);
    ctx->log_debug_print = log_debug_print;
    ctx->thr = std::thread(net_recv_operation, ctx); // start thread, place object to context

    return ctx;
}

void UdpRxControl::stop_thread(std::shared_ptr<UdpRxContext> ctx)
{
    ctx->stop_working_thread_close_socket();
}
