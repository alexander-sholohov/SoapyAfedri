//  SPDX-FileCopyrightText: 2023 Alexander Sholokhov <ra9yer@yahoo.com>
//  SPDX-License-Identifier: GPL-3.0-or-later
#include "simple_tcp_communicator.hpp"

#include "inet_common.h"

#include <chrono>
#include <cstring>
#include <iostream>

SimpleTcpCommunicator::SimpleTcpCommunicator(std::string const &address, int port)
    : _sock(-1)
{
    int rc;
    _sock = socket(AF_INET, SOCK_STREAM, 0);
    if (_sock < 0)
        throw ConnectError("socket error");

        // Simple way to make connect timeout (Linux only)
#ifdef __linux
    int synRetries = 2; // Send a total of 3 SYN packets => Timeout ~7s
    setsockopt(_sock, IPPROTO_TCP, TCP_SYNCNT, &synRetries, sizeof(synRetries));
#endif

    struct sockaddr_in sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;

    rc = inet_pton(AF_INET, address.c_str(), &(sa.sin_addr));
    if (!rc)
    {
        closesocket(_sock);
        throw ConnectError("inet_pton error");
    }
    sa.sin_port = htons(port);
    rc = connect(_sock, (struct sockaddr *)&sa, sizeof(sa));
    if (rc < 0)
    {
        closesocket(_sock);
        throw ConnectError("connect error");
    }
}

SimpleTcpCommunicator::~SimpleTcpCommunicator()
{
    closesocket(_sock);
}

void SimpleTcpCommunicator::send(std::vector<unsigned char> const &buf)
{
    int rc = ::send(_sock, (const char *)&buf[0], (int)buf.size(), 0);
    if (rc < 0)
        throw OperationError("Send error");
}

std::vector<unsigned char> SimpleTcpCommunicator::read_with_timeout(int timeout_in_ms,
                                                                    std::function<bool(std::vector<unsigned char> const &)> stop_predicate)
{
    std::vector<unsigned char> res;

    if (stop_predicate(res))
    {
        return res;
    }

    std::vector<unsigned char> tmp_buf;
    tmp_buf.resize(1024);

    auto start_stamp = std::chrono::system_clock::now();

    for (;;)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(_sock, &readfds);

        // 50ms delay max
        struct timeval tv = {0, 50000};
        int ret = select((int)_sock + 1, &readfds, NULL, NULL, &tv);
        if (ret)
        {
            ssize_t received = recv(_sock, (char *)&tmp_buf[0], (int)tmp_buf.size(), 0);
            if (received <= 0)
            {
                throw OperationError("Rx error");
            }

            // append result array
            res.insert(res.end(), tmp_buf.begin(), tmp_buf.begin() + received);

            // exit loop if predicate return true
            if (stop_predicate(res))
            {
                break;
            }
        }

        const auto now_stamp = std::chrono::system_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now_stamp - start_stamp);
        if (elapsed.count() > timeout_in_ms)
        {
            throw ReadTimeout("SimpleTcpCommunicator read timeout");
        }
    }

    return res;
}
