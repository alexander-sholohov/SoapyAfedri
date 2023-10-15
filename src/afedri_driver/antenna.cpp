//  SPDX-FileCopyrightText: 2023 Alexander Sholokhov <ra9yer@yahoo.com>
//  SPDX-License-Identifier: GPL-3.0-or-later
#include "soapy_afedri.hpp"

#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Logger.hpp>

std::vector<std::string> AfedriDevice::listAntennas(const int direction, const size_t channel) const
{
    std::vector<std::string> res;
    res.push_back("RX");
    return res;
}

void AfedriDevice::setAntenna(const int /* direction */, const size_t channel, const std::string &name)
{
    SoapySDR::logf(SOAPY_SDR_INFO, "Afedri in setAntenna. ch=%d name=%s", (int)channel, name.c_str());
    _saved_antenna = name;
}

std::string AfedriDevice::getAntenna(const int direction, const size_t channel) const
{
    return _saved_antenna;
}
