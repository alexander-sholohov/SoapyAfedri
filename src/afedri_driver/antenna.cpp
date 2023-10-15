//  SPDX-FileCopyrightText: 2023 Alexander Sholokhov <ra9yer@yahoo.com>
//  SPDX-License-Identifier: GPL-3.0-or-later
#include "soapy_afedri.hpp"

#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Logger.hpp>

void AfedriDevice::setAntenna(const int /* direction */, const size_t channel, const std::string &name)
{
    SoapySDR::logf(SOAPY_SDR_INFO, "Afedri in setAntenna. ch=%d name=%s", (int)channel, name.c_str());
}
