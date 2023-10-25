//  SPDX-FileCopyrightText: 2023 Alexander Sholokhov <ra9yer@yahoo.com>
//  SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <vector>

#include "inet_common.h"

class AfedriDiscovery
{
  public:
    struct InterfaceItem
    {
        in_addr bind_address;
        in_addr broadcast_address;
    };

    struct AfedriFoundItem
    {
        std::string address;
        int port;
        std::string serial_number;
        std::string name;
    };

    static std::vector<InterfaceItem> enum_addresses();
    static std::vector<AfedriFoundItem> discovery();
};
