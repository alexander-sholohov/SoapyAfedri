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
                           int num_channels, int map_ch0)
    : _afedri_control(address, port),
      _bind_address(bind_address),
      _bind_port(bind_port),
      _afedri_rx_mode(afedri_mode),
      _num_channels(num_channels),
      _map_ch0(map_ch0),
      _stream_sequence_provider(1),
      _saved_frequency(0.0),
      _saved_sample_rate(0.0),
      _saved_bandwidth(0.0)
{

    _version_info = _afedri_control.get_version_info();

    // Check rx_mode
    if (_afedri_rx_mode > 5)
    {
        _afedri_rx_mode = -1;
    }

    if (_afedri_rx_mode != -1)
    {
        auto ch = AfedriControl::make_afedri_channel_from_0based_index(0); // TODO: Check what channel to use here?
        _afedri_control.set_rx_mode(ch, static_cast<AfedriControl::RxMode>(_afedri_rx_mode));
        SoapySDR::logf(SOAPY_SDR_WARNING, "Afedri set_rx_mode to %d", _afedri_rx_mode);
    }

    // Reset r802t AGC for channel 0. TODO: check channel index
    if (_version_info.is_r820t_present)
    {
        auto ch = AfedriControl::make_afedri_channel_from_0based_index(remap_channel(0));
        _afedri_control.set_r820t_lna_agc(ch, 0);
        _afedri_control.set_r820t_mixer_agc(ch, 0);
    }

    if (_num_channels == 0 && _afedri_rx_mode != -1)
    {
        if (_afedri_rx_mode == 1 || _afedri_rx_mode == 2)
        {
            _num_channels = 2;
        }
        else if (_afedri_rx_mode == 4 || _afedri_rx_mode == 5)
        {
            _num_channels = 4;
        }
        else
        {
            _num_channels = 1;
        }
    }
    else if (_num_channels == 0 || _num_channels > 4)
    {
        _num_channels = 1;
    }

    SoapySDR::logf(SOAPY_SDR_INFO, "Afedri _num_channels=%d", _num_channels);

    // prevent remap error
    if (_map_ch0 >= static_cast<int>(_num_channels))
    {
        SoapySDR::log(SOAPY_SDR_WARNING, "Afedri incorrect map_ch0 was reset.");
        _map_ch0 = -1;
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
    m["is_r820t_present"] = std::to_string(_version_info.is_r820t_present);

    m["soapy_afedri_driver_version"] = VERSION;
    m["origin"] = "https://github.com/alexander-sholohov/SoapyAfedri";

    return m;
}
