//  SPDX-FileCopyrightText: 2023 Alexander Sholokhov <ra9yer@yahoo.com>
//  SPDX-License-Identifier: GPL-3.0-or-later
#include "soapy_afedri.hpp"

#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Logger.hpp>

#include "afedri_control.hpp"
#include "udp_rx.hpp"

void AfedriDevice::setSampleRate(const int direction, const size_t channel, const double rate)
{
    const std::uint32_t samp_rate = (std::uint32_t)rate;

    const auto ch = AfedriControl::make_afedri_channel_from_0based_index(remap_channel(channel));

    AfedriControl ac(_address, _port);
    ac.set_sample_rate(ch, samp_rate);

    const std::uint32_t quartz = _version_info.main_clock_frequency; // For me it was 76_800_000
    const std::uint32_t actual_samp_rate = AfedriControl::calc_actual_sample_rate(quartz, samp_rate);
    auto level = (actual_samp_rate == samp_rate) ? SOAPY_SDR_INFO : SOAPY_SDR_WARNING;
    SoapySDR_logf(level, "Afedri: Set sample rate as %d, actual sample rate will be %d, quartz=%d", samp_rate, actual_samp_rate, quartz);

    _saved_sample_rate = (double)actual_samp_rate;
}

double AfedriDevice::getSampleRate(const int direction, const size_t channel) const
{
    return _saved_sample_rate;
}

std::vector<double> AfedriDevice::listSampleRates(const int direction, const size_t channel) const
{
    std::vector<double> results;

    // golden sample rates (assume we have crystal 76.8MHz)
    results.push_back(48e3);
    results.push_back(50e3);
    results.push_back(60e3);
    results.push_back(75e3);
    results.push_back(80e3);
    results.push_back(96e3);
    results.push_back(100e3);
    results.push_back(120e3);
    results.push_back(150e3);
    results.push_back(160e3);
    results.push_back(192e3);
    results.push_back(200e3);
    results.push_back(256e3);
    results.push_back(300e3);
    results.push_back(320e3);
    results.push_back(400e3);
    results.push_back(600e3);
    results.push_back(640e3);
    results.push_back(768e3);
    results.push_back(800e3);
    results.push_back(960e3);
    results.push_back(1.2e6);
    results.push_back(1.28e6);
    results.push_back(1.6e6);
    results.push_back(1.92e6);
    results.push_back(2.4e6);

    return results;
}

SoapySDR::RangeList AfedriDevice::getSampleRateRange(const int direction, const size_t channel) const
{
    SoapySDR::RangeList results;
    results.push_back(SoapySDR::Range(48e3, 2.4e6));
    return results;
}

void AfedriDevice::setBandwidth(const int direction, const size_t channel, const double bw)
{
    // We do not have bandwidth concept.
    _saved_bandwidth = bw;
}

double AfedriDevice::getBandwidth(const int direction, const size_t channel) const
{
    if (_saved_bandwidth == 0.0)
    {
        return _saved_sample_rate;
    }

    return _saved_bandwidth;
}

std::vector<double> AfedriDevice::listBandwidths(const int direction, const size_t channel) const
{
    std::vector<double> results;
    return results;
}

SoapySDR::RangeList AfedriDevice::getBandwidthRange(const int direction, const size_t channel) const
{
    SoapySDR::RangeList results;

    // stub
    results.push_back(SoapySDR::Range(0, 2.4e6));
    return results;
}
