//  SPDX-FileCopyrightText: 2023 Alexander Sholokhov <ra9yer@yahoo.com>
//  SPDX-License-Identifier: GPL-3.0-or-later
#include "soapy_afedri.hpp"

#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Logger.hpp>

StreamContext &AfedriDevice::get_stream_context_by_id(int stream_id)
{
    const auto it = _configured_streams.find(stream_id);
    if (it == _configured_streams.end())
    {
        SoapySDR::logf(SOAPY_SDR_ERROR, "call with incorrect or closed stream. stream_id=%d", stream_id);
        throw std::runtime_error("incorrect stream_id");
    }
    return it->second;
}

AfedriControl::VersionInfo const &AfedriDevice::get_version_info() const
{
    return _version_info;
}
