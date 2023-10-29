//  SPDX-FileCopyrightText: 2023 Alexander Sholokhov <ra9yer@yahoo.com>
//  SPDX-License-Identifier: GPL-3.0-or-later

#include "soapy_afedri.hpp"

#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Logger.hpp>
// #include <SoapySDR/Types.h>

#include <algorithm>
#include <iostream>

/*******************************************************************
 * Channels API
 ******************************************************************/

size_t AfedriDevice::getNumChannels(const int dir) const
{
    return (dir == SOAPY_SDR_RX) ? _num_channels : 0;
}

bool AfedriDevice::getFullDuplex(const int /*direction*/, const size_t /*channel*/) const
{
    return false;
}

/*******************************************************************
 * Stream API
 ******************************************************************/

std::vector<std::string> AfedriDevice::getStreamFormats(const int /* direction */, const size_t /* channel */) const
{
    std::vector<std::string> formats;

    formats.push_back(SOAPY_SDR_CS16);
    formats.push_back(SOAPY_SDR_CF32);

    return formats;
}

std::string AfedriDevice::getNativeStreamFormat(const int direction, const size_t /* channel */, double &fullScale) const
{
    // Check that direction is SOAPY_SDR_RX
    if (direction != SOAPY_SDR_RX)
    {
        throw std::runtime_error("Afedri is RX only, use SOAPY_SDR_RX");
    }

    fullScale = 32768;
    return SOAPY_SDR_CS16;
}

SoapySDR::ArgInfoList AfedriDevice::getStreamArgsInfo(const int direction, const size_t /* channel */) const
{
    // Check that direction is SOAPY_SDR_RX
    if (direction != SOAPY_SDR_RX)
    {
        throw std::runtime_error("Afedri is RX only, use SOAPY_SDR_RX");
    }

    SoapySDR::ArgInfoList streamArgs;

    return streamArgs;
}

static std::string to_lower(std::string const &s)
{
    auto data = s;
    std::transform(data.begin(), data.end(), data.begin(), [](unsigned char c) { return std::tolower(c); });

    return data;
}

static int str2boolint(std::string const &s)
{
    int res = 0;

    if (s == "true" || s == "True" || s == "1")
    {
        res = 1;
    }

    return res;
}

void AfedriDevice::writeSetting(const std::string &key, const std::string &value)
{
    SoapySDR::logf(SOAPY_SDR_INFO, "Afedri in writeSetting. key=%s value=%s", key.c_str(), value.c_str());
    const std::string lower_key = to_lower(key);
    const auto afedri_channel = AfedriControl::make_afedri_channel_from_0based_index(remap_channel(0)); // TODO: check channel index

    if (lower_key == "r820t_lna_agc")
    {
        _afedri_control.set_r820t_lna_agc(afedri_channel, str2boolint(value));
    }
    else if (lower_key == "r820t_mixer_agc")
    {
        _afedri_control.set_r820t_mixer_agc(afedri_channel, str2boolint(value));
    }
    else
    {
        SoapySDR::logf(SOAPY_SDR_WARNING, "Afedri in writeSetting.  key=%s ignored!", key.c_str());
    }
}
