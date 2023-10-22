//  SPDX-FileCopyrightText: 2023 Alexander Sholokhov <ra9yer@yahoo.com>
//  SPDX-License-Identifier: GPL-3.0-or-later
#include "soapy_afedri.hpp"

#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Logger.hpp>

#include "afedri_control.hpp"

void AfedriDevice::setFrequency(const int /*direction*/, const size_t channel, const std::string &name, const double frequency,
                                const SoapySDR::Kwargs & /*args*/)
{
    if (name == "RF")
    {
        SoapySDR_logf(SOAPY_SDR_INFO, "Afedri: Setting center freq. channel=%d, freq=%d", (int)channel, (uint32_t)frequency);
        const auto ch = AfedriControl::make_afedri_channel_from_0based_index(remap_channel(channel));
        AfedriControl ac(_address, _port);
        ac.set_frequency(ch, (uint32_t)frequency);

        _saved_frequency = frequency;
    }
    else
    {
        SoapySDR_logf(SOAPY_SDR_WARNING, "Afedri: try to set frequency for wrong name: %s", name);
    }
}

double AfedriDevice::getFrequency(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "RF")
    {
        return _saved_frequency;
    }

    return 0.0;
}

std::vector<std::string> AfedriDevice::listFrequencies(const int direction, const size_t channel) const
{
    std::vector<std::string> names;
    names.push_back("RF");
    return names;
}

SoapySDR::RangeList AfedriDevice::getFrequencyRange(const int direction, const size_t channel, const std::string &name) const
{
    SoapySDR::RangeList results;
    if (name == "RF")
    {
        // TODO:
        results.push_back(SoapySDR::Range(100000, 35 * 1000000));
        results.push_back(SoapySDR::Range(35 * 1000000, 1450 * 1000000));
    }

    return results;
}

SoapySDR::ArgInfoList AfedriDevice::getFrequencyArgsInfo(const int direction, const size_t channel) const
{
    // TODO:
    return SoapySDR::ArgInfoList();
}
