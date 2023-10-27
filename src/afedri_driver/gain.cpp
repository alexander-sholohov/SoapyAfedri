//  SPDX-FileCopyrightText: 2023 Alexander Sholokhov <ra9yer@yahoo.com>
//  SPDX-License-Identifier: GPL-3.0-or-later
#include "soapy_afedri.hpp"

#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Logger.h>
#include <SoapySDR/Logger.hpp>

#include "afedri_control.hpp"

constexpr const char *R820T_LNA_GAIN = "R820T_LNA_GAIN";
constexpr const char *R820T_MIXER_GAIN = "R820T_MIXER_GAIN";
constexpr const char *R820T_VGA_GAIN = "R820T_VGA_GAIN";
constexpr const char *RF = "RF";
constexpr const char *FE = "FE";

std::vector<std::string> AfedriDevice::listGains(const int /*direction*/, const size_t /*channel*/) const
{
    std::vector<std::string> results;

    results.push_back(RF);
    results.push_back(FE);

    results.push_back(R820T_LNA_GAIN);
    results.push_back(R820T_MIXER_GAIN);
    results.push_back(R820T_VGA_GAIN);

    return results;
}

void AfedriDevice::setGain(const int /* direction */, const size_t /* channel */, const double /* value */)
{
    SoapySDR_logf(SOAPY_SDR_WARNING, "Afedri: General setGain not supported.");
}

void AfedriDevice::setGain(const int /*direction*/, const size_t channel, const std::string &name, const double value)
{
    SoapySDR_logf(SOAPY_SDR_INFO, "Afedri: setGain Name=%s, Gain=%f ", name.c_str(), value);
    const auto ch = AfedriControl::make_afedri_channel_from_0based_index(remap_channel(channel));
    _saved_gains[name] = value;

    if (name == RF)
    {
        AfedriControl ac(_address, _port);
        ac.set_rf_gain(ch, value);
    }
    else if (name == FE)
    {
        AfedriControl ac(_address, _port);
        ac.set_fe_gain(ch, value);
    }
    else if (name == R820T_LNA_GAIN)
    {
        AfedriControl ac(_address, _port);
        ac.set_r820t_lna_gain(ch, value);
    }
    else if (name == R820T_MIXER_GAIN)
    {
        AfedriControl ac(_address, _port);
        ac.set_r820t_mixer_gain(ch, value);
    }
    else if (name == R820T_VGA_GAIN)
    {
        AfedriControl ac(_address, _port);
        ac.set_r820t_vga_gain(ch, value);
    }
    else
    {
        SoapySDR_logf(SOAPY_SDR_WARNING, "Afedri: setGain. Wrong name: %s", name.c_str());
    }
}

double AfedriDevice::getGain(const int /*direction*/, const size_t /*channel*/, const std::string &name) const
{
    auto it = _saved_gains.find(name);
    if (it != _saved_gains.end())
        return it->second;

    return 0.0;
}

SoapySDR::Range AfedriDevice::getGainRange(const int /*direction*/, const size_t /*channel*/, const std::string &name) const
{
    if (name == RF)
    {
        return SoapySDR::Range(-10.0, +35.0);
    }
    else if (name == FE)
    {
        return SoapySDR::Range(0.0, +12.0);
    }
    else if (name == R820T_LNA_GAIN)
    {
        return SoapySDR::Range(-7.5, +35.0);
    }
    else if (name == R820T_MIXER_GAIN)
    {
        return SoapySDR::Range(0.0, +2.0);
    }
    else if (name == R820T_VGA_GAIN)
    {
        return SoapySDR::Range(+1.0, +48.0);
    }
    else
    {
        SoapySDR_logf(SOAPY_SDR_ERROR, "Afedri: getGainRange. Wrong name: %s", name.c_str());
        throw std::runtime_error("getGainRange wrong name");
    }
}
