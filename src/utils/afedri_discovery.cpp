//  SPDX-FileCopyrightText: 2023 Alexander Sholokhov <ra9yer@yahoo.com>
//  SPDX-License-Identifier: GPL-3.0-or-later

#include "afedri_discovery.hpp"

#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

#include "portable_utils.h"

// Win32 solution from
// https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getadaptersaddresses

#ifdef _WIN32
#include <iphlpapi.h>
#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))
#define WORKING_BUFFER_SIZE 15000
#define MAX_TRIES 3

// Link with Iphlpapi.lib
#pragma comment(lib, "IPHLPAPI.lib")

static std::vector<AfedriDiscovery::InterfaceItem> _enum_addresses_win32()
{
    std::vector<AfedriDiscovery::InterfaceItem> res;

    /* Declare and initialize variables */

    DWORD dwSize = 0;
    DWORD dwRetVal = 0;

    // Set the flags to pass to GetAdaptersAddresses
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;

    // IPV4
    ULONG family = AF_INET;

    LPVOID lpMsgBuf = NULL;

    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    ULONG outBufLen = 0;
    ULONG Iterations = 0;

    PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;
    PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;

    // Allocate a 15 KB buffer to start with.
    outBufLen = WORKING_BUFFER_SIZE;

    do
    {

        pAddresses = (IP_ADAPTER_ADDRESSES *)MALLOC(outBufLen);
        if (pAddresses == NULL)
        {
            std::runtime_error("Memory allocation failed for IP_ADAPTER_ADDRESSES struct");
        }

        dwRetVal = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);

        if (dwRetVal == ERROR_BUFFER_OVERFLOW)
        {
            FREE(pAddresses);
            pAddresses = NULL;
        }
        else
        {
            break;
        }

        Iterations++;

    } while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < MAX_TRIES));

    if (dwRetVal == NO_ERROR)
    {
        for (pCurrAddresses = pAddresses; pCurrAddresses; pCurrAddresses = pCurrAddresses->Next)
        {

            if ((IF_TYPE_ETHERNET_CSMACD != pCurrAddresses->IfType) && (IF_TYPE_IEEE80211 != pCurrAddresses->IfType))
            {
                continue;
            }

            if (pCurrAddresses->OperStatus != IfOperStatusUp)
            {
                continue; // Ignore adapters not UP
            }

            for (pUnicast = pCurrAddresses->FirstUnicastAddress; pUnicast; pUnicast = pUnicast->Next)
            {
                if (pUnicast->Address.lpSockaddr->sa_family != AF_INET)
                {
                    continue;
                }

                ULONG mask;
                ConvertLengthToIpv4Mask(pUnicast->OnLinkPrefixLength, &mask);

                AfedriDiscovery::InterfaceItem item;
                item.bind_address = ((struct sockaddr_in *)pUnicast->Address.lpSockaddr)->sin_addr;
                item.broadcast_address.s_addr = item.bind_address.s_addr | (~mask);

                res.push_back(item);
            }
        }
    }
    else
    {
        if (dwRetVal == ERROR_NO_DATA)
        {
            std::cout << "No addresses were found for the requested parameters" << std::endl;
        }
        else
        {
            std::cerr << "GetAdaptersAddresses error=" << dwRetVal << std::endl;
        }
    }

    if (pAddresses)
    {
        FREE(pAddresses);
    }
    return res;
}

#else
#include <ifaddrs.h>
#include <net/if.h>

static std::vector<AfedriDiscovery::InterfaceItem> _enum_addresses_posix()
{
    std::vector<AfedriDiscovery::InterfaceItem> res;
    struct ifaddrs *addrs = NULL, *p;

    if (getifaddrs(&addrs))
    {
        std::cerr << "getifaddrs() failed. errno=" << errno << std::endl;
        return res;
    }

    for (p = addrs; p; p = p->ifa_next)
    {
        if (p->ifa_addr->sa_family != AF_INET)
        {
            continue;
        }

        if ((p->ifa_flags & IFF_RUNNING) && (p->ifa_flags & IFF_BROADCAST) && (p->ifa_flags & IFF_LOOPBACK) == 0)
        {
            // ok
        }
        else
        {
            continue;
        }

        const sockaddr_in *sa_addr_in = (struct sockaddr_in *)(p->ifa_addr);
        const sockaddr_in *sa_mask_in = (struct sockaddr_in *)(p->ifa_netmask);

        AfedriDiscovery::InterfaceItem item;

        item.bind_address = sa_addr_in->sin_addr;

        // we OR mask to bind address to make broadcast address
        item.broadcast_address = item.bind_address;
        item.broadcast_address.s_addr |= ~sa_mask_in->sin_addr.s_addr;

        res.push_back(item);
    }

    freeifaddrs(addrs);
    return res;
}

#endif

std::vector<AfedriDiscovery::InterfaceItem> AfedriDiscovery::enum_addresses()
{
#ifdef _WIN32
    return _enum_addresses_win32();
#else
    return _enum_addresses_posix();
#endif
}

//
// https://stackoverflow.com/questions/1537964/visual-c-equivalent-of-gccs-attribute-packed
#ifdef __GNUC__
#define PACK(__Declaration__) __Declaration__ __attribute__((__packed__))
#endif
#ifdef _MSC_VER
#define PACK(__Declaration__) __pragma(pack(push, 1)) __Declaration__ __pragma(pack(pop))
#endif

// -------------- Some constants and structures from sdr_discovery.h project SDR_Network_Control_x2

#define DISCOVER_SERVER_PORT (48321) /* PC client Tx port, SDR Server Rx Port */
#define DISCOVER_CLIENT_PORT (48322) /* PC client Rx port, SDR Server Tx Port */

#define KEY0 (0x5A)
#define KEY1 (0xA5)
#define MSG_REQ (0)
#define MSG_RESP (1)

PACK(struct DiscoveryStruct {  // 56 fixed common byte fields
    unsigned char length[2];   // length of total message in bytes (little endian byte order)
    unsigned char key[2];      // fixed key key[0]==0x5A  key[1]==0xA5
    unsigned char op;          // 0==Request(to device)  1==Response(from device) 2 ==Set(to device)
    char name[16];             // Device name string null terminated
    char sn[16];               // Serial number string null terminated
    unsigned char ipaddr[16];  // device IP address (little endian byte order)
    unsigned char port[2];     // device Port number (little endian byte order)
    unsigned char customfield; // Specifies a custom data field for a particular device
});

// ---------------

static void net_recv_operation(int rx_sock, std::vector<AfedriDiscovery::AfedriFoundItem> &res)
{
    std::vector<unsigned char> rx_buf;
    rx_buf.resize(500);

    fd_set readfds;

    // 5 iterations of 0.1 sec delay -> 0.5 sec per interface
    for (size_t idx = 0; idx < 10; idx++)
    {
        FD_ZERO(&readfds);
        FD_SET(rx_sock, &readfds);

        struct timeval tv = {0, 50000}; // 0.05 seconds delay
        int ret = select(rx_sock + 1, &readfds, NULL, NULL, &tv);

        if (ret > 0)
        {
            struct sockaddr_in client_addr;
            std::memset(&client_addr, 0, sizeof(client_addr));
            socklen_t client_addr_len = sizeof(client_addr);

            int bytes_did_read =
                recvfrom(rx_sock, (char *)&rx_buf[0], (int)rx_buf.size(), 0, (struct sockaddr *)&client_addr, &client_addr_len);

            if (bytes_did_read < static_cast<int>(sizeof(DiscoveryStruct)))
            {
                continue;
            }

            const DiscoveryStruct *ptr = (DiscoveryStruct *)&rx_buf[0];
            if (ptr->op != MSG_RESP || ptr->key[0] != KEY0 || ptr->key[1] != KEY1)
            {
                // Not Afedri reply
                continue;
            }

            AfedriDiscovery::AfedriFoundItem item;

            {
                std::ostringstream ss;
                for (size_t idx = 0; idx < 4; idx++)
                {
                    if (idx != 0)
                    {
                        ss << ".";
                    }
                    ss << static_cast<unsigned>(ptr->ipaddr[3 - idx]);
                }
                item.address = ss.str();
            }

            item.port = (ptr->port[1] << 8) + ptr->port[0];
            item.serial_number = ptr->sn;
            item.name = ptr->name;
            res.push_back(item);
        }
    }
}

static void probe_interface(AfedriDiscovery::InterfaceItem const &addr, std::vector<AfedriDiscovery::AfedriFoundItem> &res)
{
    int rx_sock = (int)socket(AF_INET, SOCK_DGRAM, 0);
    if (rx_sock < 0)
    {
        throw std::runtime_error("Socket error");
    }

    {
        const int broadcastEnable = 1;
        setsockopt(rx_sock, SOL_SOCKET, SO_BROADCAST, (const char *)&broadcastEnable, sizeof(broadcastEnable));
        const int reuseEnable = 1;
        setsockopt(rx_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuseEnable, sizeof(reuseEnable));
    }

    in_addr inaddress_broadcast;
    inaddress_broadcast.s_addr = INADDR_BROADCAST;

    struct sockaddr_in sockaddr_rx;
    std::memset(&sockaddr_rx, 0, sizeof(sockaddr_rx));

    sockaddr_rx.sin_family = AF_INET;
    sockaddr_rx.sin_port = htons(DISCOVER_CLIENT_PORT);
    sockaddr_rx.sin_addr.s_addr = INADDR_ANY;

    if (bind(rx_sock, (struct sockaddr *)&sockaddr_rx, sizeof(sockaddr_rx)) < 0)
    {
        std::stringstream ss;
        ss << "rx_sock bind error: " << get_error_text() << std::endl;
        std::cerr << ss.str();
        closesocket(rx_sock);
        return;
    }

    for (int pass = 0; pass < 2; pass++)
    {
        int tx_sock = (int)socket(AF_INET, SOCK_DGRAM, 0);
        if (tx_sock < 0)
        {
            throw std::runtime_error("Socket error");
        }

        {
            const int broadcastEnable = 1;
            setsockopt(tx_sock, SOL_SOCKET, SO_BROADCAST, (const char *)&broadcastEnable, sizeof(broadcastEnable));
        }

        struct sockaddr_in sockaddr_tx;
        std::memset(&sockaddr_tx, 0, sizeof(sockaddr_tx));

        sockaddr_tx.sin_family = AF_INET;
        sockaddr_tx.sin_port = htons(DISCOVER_SERVER_PORT);
        sockaddr_tx.sin_addr = addr.bind_address;

        struct sockaddr_in broadcast_sockaddr_tx = sockaddr_tx;
        broadcast_sockaddr_tx.sin_addr = (pass == 1) ? inaddress_broadcast : addr.broadcast_address;

        if (bind(tx_sock, (struct sockaddr *)&sockaddr_tx, sizeof(sockaddr_rx)) < 0)
        {
            std::stringstream ss;
            ss << "tx_sock bind error: " << get_error_text() << std::endl;
            std::cerr << ss.str();

            closesocket(tx_sock);
            continue;
        }

        DiscoveryStruct ds;
        int length = sizeof(ds);
        std::memset(&ds, 0, length);
        ds.length[0] = length & 0xff;
        ds.length[1] = (length >> 8) & 0xff;
        ds.key[0] = KEY0;
        ds.key[1] = KEY1;
        ds.op = MSG_REQ;

        sendto(tx_sock, (const char *)&ds, length, 0, (struct sockaddr *)&sockaddr_tx, sizeof(sockaddr));
        sendto(tx_sock, (const char *)&ds, length, 0, (struct sockaddr *)&broadcast_sockaddr_tx, sizeof(sockaddr));
        net_recv_operation(rx_sock, res);

        closesocket(tx_sock);
    }

    closesocket(rx_sock);
}

std::vector<AfedriDiscovery::AfedriFoundItem> AfedriDiscovery::discovery()
{
    std::vector<AfedriDiscovery::AfedriFoundItem> res;

    const auto addresses = enum_addresses();

    // gather all afedri devices
    for (auto const &addr : addresses)
    {
        probe_interface(addr, res);
    }

    // remove duplicates based on address+port key
    std::map<std::pair<std::string, int>, AfedriFoundItem> unique;
    for (auto const &elm : res)
    {
        unique.insert(std::make_pair(std::make_pair(elm.address, elm.port), elm));
    }
    res.clear();
    for (auto const &elm : unique)
    {
        res.push_back(elm.second);
    }

    return res;
}
