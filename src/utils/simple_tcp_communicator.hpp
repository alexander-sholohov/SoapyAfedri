//  SPDX-FileCopyrightText: 2023 Alexander Sholokhov <ra9yer@yahoo.com>
//  SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "inet_common.h"

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#define DERIVE_SIMPLE_TCP_EXCEPTION(class_name, base_name) \
    class class_name : public base_name \
    { \
      public: \
        class_name(const std::string &what = "") \
            : base_name(what) \
        { \
        } \
    };

DERIVE_SIMPLE_TCP_EXCEPTION(CommunicatorErrorBase, std::runtime_error);
DERIVE_SIMPLE_TCP_EXCEPTION(ConnectError, CommunicatorErrorBase);
DERIVE_SIMPLE_TCP_EXCEPTION(OperationError, CommunicatorErrorBase);
DERIVE_SIMPLE_TCP_EXCEPTION(ReadTimeout, CommunicatorErrorBase);

class SimpleTcpCommunicator
{
  public:
    SimpleTcpCommunicator(std::string const &address, int port);
    SimpleTcpCommunicator(SimpleTcpCommunicator &) = delete;
    ~SimpleTcpCommunicator();

    void send(std::vector<unsigned char> const &buf);
    std::vector<unsigned char> read_with_timeout(int timeout_in_ms, std::function<bool(std::vector<unsigned char> const &)> stop_predicate);

  private:
    MYSOCKET _sock;
};
