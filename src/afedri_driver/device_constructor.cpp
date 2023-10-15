//  SPDX-FileCopyrightText: 2023 Alexander Sholokhov <ra9yer@yahoo.com>
//  SPDX-License-Identifier: GPL-3.0-or-later
#include "soapy_afedri.hpp"

#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Logger.hpp>
// #include <SoapySDR/Types.h>

#include <algorithm>
#include <iostream>

constexpr const char *VERSION = "1.0.1";

static void debug_print_for_thread(std::string const &str)
{
    SoapySDR::logf(SOAPY_SDR_INFO, "Afedri RX Thread: %s", str.c_str());
}

AfedriDevice::AfedriDevice(std::string const &address, int port, std::string const &bind_address, int bind_port, int afedri_mode,
                           int num_channels, int force_selected_channel)
    : _address(address),
      _port(port),
      _bind_address(bind_address),
      _bind_port(bind_port),
      _afedri_rx_mode(afedri_mode),
      _num_channels(num_channels),
      _force_selected_channel(force_selected_channel),
      _stream_sequence_provider(1),
      _saved_frequency(0.0),
      _saved_sample_rate(0.0),
      _saved_bandwidth(0.0)
{
#ifdef _MSC_VER
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        throw std::runtime_error("WSAStartup error");
    }
#endif

    AfedriControl ac(_address, _port);

    _version_info = ac.get_version_info();

    if (_afedri_rx_mode != -1)
    {
        auto ch = AfedriControl::make_afedri_channel_from_0based_index(0); // What channel to use here?
        ac.set_rx_mode(ch, static_cast<AfedriControl::RxMode>(_afedri_rx_mode));
    }

    // Create UDP Rx process
    try
    {
        auto thrctx = UdpRxControl::start_thread(_num_channels, _bind_address, _bind_port, debug_print_for_thread);
        _udp_rx_thread_defer.reset(new UdpRxContextDefer(thrctx)); // this is for automatically stop thread on destroy driver
    }
    catch (UdpRxError &ex)
    {
        SoapySDR::logf(SOAPY_SDR_WARNING, "Afedri device present, but we can't bind UDP socket for RX thread. : %s", ex.what());
        throw;
    }

    SoapySDR::logf(SOAPY_SDR_INFO, "Afedri device created.");
}

std::string AfedriDevice::getDriverKey(void) const
{
    return "Afedri";
}

std::string AfedriDevice::getHardwareKey(void) const
{
    return _version_info.version_string;
}

SoapySDR::Kwargs AfedriDevice::getHardwareInfo(void) const
{
    SoapySDR::Kwargs m;

    m["version_string"] = _version_info.version_string;
    m["serial_number"] = _version_info.serial_number;
    m["firmware_version"] = _version_info.firmware_version;
    m["product_id"] = _version_info.product_id;
    m["hw_fw_version"] = _version_info.hw_fw_version;
    m["interface_version"] = _version_info.interface_version;
    m["main_clock_frequency"] = std::to_string(_version_info.main_clock_frequency);
    m["diversity_mode"] = std::to_string(_version_info.diversity_mode);
    m["soapy_afedri_driver_version"] = VERSION;

    return m;
}
