//  SPDX-FileCopyrightText: 2023 Alexander Sholokhov <ra9yer@yahoo.com>
//  SPDX-License-Identifier: GPL-3.0-or-later
#include "soapy_afedri.hpp"

#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Logger.hpp>

#include "afedri_control.hpp"
#include "udp_rx.hpp"

#include <cstring>
#include <sstream>

SoapySDR::Stream *AfedriDevice::setupStream(const int direction, const std::string &format, const std::vector<size_t> &channels,
                                            const SoapySDR::Kwargs & /*args*/)
{
    SoapySDR::logf(SOAPY_SDR_WARNING, "Afedri in setupStream. Num_channels=%d, format=%s", channels.size(), format.c_str());

    if (direction != SOAPY_SDR_RX)
    {
        throw std::runtime_error("AfedriDevice is RX only.");
    }

    auto wrk_channels = channels; // Make a working copy

    // Force make 0 channel if no channel was provided.
    if (wrk_channels.empty())
    {
        wrk_channels.push_back(0);
    }

    // Check channels size
    if (wrk_channels.size() > _num_channels || wrk_channels.size() > 4)
    {
        SoapySDR::log(SOAPY_SDR_ERROR, "invalid number of channels");
        throw std::runtime_error("setupStream invalid number of channels");
    }

    // Check channels index
    for (auto ch : wrk_channels)
    {
        if (ch >= _num_channels || ch >= 4)
        {
            SoapySDR::log(SOAPY_SDR_ERROR, "invalid number of channels");
            throw std::runtime_error("setupStream invalid channel selection");
        }
    }

    // remap channels
    for (size_t idx = 0; idx < wrk_channels.size(); idx++)
    {
        wrk_channels[idx] = remap_channel(wrk_channels[idx]);
    }

    // Debug output
    {
        std::ostringstream ss;
        ss << "Afedri actual channels: [";
        for (size_t idx = 0; idx < wrk_channels.size(); idx++)
        {
            if (idx != 0) ss << ",";
            ss << wrk_channels[idx];
        }
        ss << "]";
        auto s = ss.str();
        SoapySDR::log(SOAPY_SDR_INFO, s.c_str());
    }

    std::string selected_format;

    // Check the format
    if (format == SOAPY_SDR_CF32)
    {
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CF32.");
        selected_format = format;
    }
    else if (format == SOAPY_SDR_CS16)
    {
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CS16.");
        selected_format = format;
    }
    else
    {
        SoapySDR::log(SOAPY_SDR_ERROR, "Invalid stream format");
        throw std::runtime_error("setupStream invalid format '" + format +
                                 "' -- Only CS16, and CF32 are supported by AfedriDevice module.");
    }

    int just_obtained_stream_id;

    {
        std::unique_lock<std::mutex> lock(_streams_protect_mtx);
        just_obtained_stream_id = _stream_sequence_provider++;
        _configured_streams[just_obtained_stream_id] = StreamContext(wrk_channels, selected_format, false);
    }

    return (SoapySDR::Stream *)(new int(just_obtained_stream_id));
}

void AfedriDevice::closeStream(SoapySDR::Stream *stream)
{
    SoapySDR::logf(SOAPY_SDR_DEBUG, "Afedri in closeStream");

    this->deactivateStream(stream, 0, 0);

    int stream_id = *reinterpret_cast<int *>(stream);

    {
        std::unique_lock<std::mutex> lock(_streams_protect_mtx);
        _configured_streams.erase(stream_id); // destroy stream context by id
    }

    delete reinterpret_cast<int *>(stream);
}

size_t AfedriDevice::getStreamMTU(SoapySDR::Stream * /*stream*/) const
{
    return 1024; // TODO: Maybe 256?
}

int AfedriDevice::activateStream(SoapySDR::Stream *stream, const int flags, const long long /*timeNs*/, const size_t /*numElems*/)
{
    SoapySDR::logf(SOAPY_SDR_DEBUG, "in activateStream flags=%d ", flags);

    if (flags != 0)
    {
        return SOAPY_SDR_NOT_SUPPORTED;
    }

    const int stream_id = *reinterpret_cast<int *>(stream);
    StreamContext &stream_context = get_stream_context_by_id(stream_id);
    stream_context.active = true;

    AfedriControl ac(_address, _port);
    ac.start_capture(); // Activate stream. Multiple calls - not a problem.
    SoapySDR::logf(SOAPY_SDR_DEBUG, "Afedri start capture");

    _udp_rx_thread_defer->get_ctx()->rx_active = true; // flag to allow process UDP RX data

    return 0;
}

int AfedriDevice::deactivateStream(SoapySDR::Stream *stream, const int flags, const long long /*timeNs*/)
{
    SoapySDR::logf(SOAPY_SDR_DEBUG, "Afedri in deactivateStream flags=%d", flags);

    if (flags != 0)
    {
        return SOAPY_SDR_NOT_SUPPORTED;
    }

    const int stream_id = *reinterpret_cast<int *>(stream);
    StreamContext &stream_context = get_stream_context_by_id(stream_id);
    stream_context.active = false;

    // Calculate number of active streams.
    int num_active_streams = 0;
    for (auto const &item : _configured_streams)
    {
        if (item.second.active)
        {
            num_active_streams++;
        }
    }

    if (num_active_streams == 0)
    {
        AfedriControl ac(_address, _port);
        ac.stop_capture();
        SoapySDR::logf(SOAPY_SDR_INFO, "Afedri stop capture");

        _udp_rx_thread_defer->get_ctx()->rx_active = false; // flag to stop process UDP RX data
    }

    return 0;
}

int AfedriDevice::readStream(SoapySDR::Stream *stream, void *const *buffs, const size_t numElems, int & /*flags*/, long long & /*timeNs*/,
                             const long timeoutUs)
{
    const int stream_id = *reinterpret_cast<int *>(stream);
    StreamContext const &stream_context = get_stream_context_by_id(stream_id);

    // SoapySDR::logf(SOAPY_SDR_DEBUG, "in readStream flags=%d numElems=%d, timeoutUs=%d ", flags, numElems, timeoutUs);

    if (!_udp_rx_thread_defer)
    {
        // should never happen
        SoapySDR::logf(SOAPY_SDR_ERROR, "UDP thread not present");
        throw std::runtime_error("UDP thread not present");
    }

    auto udp_rx_context = _udp_rx_thread_defer->get_ctx();
    if (!udp_rx_context->is_alive())
    {
        // should never happen
        SoapySDR::logf(SOAPY_SDR_ERROR, "UDP thread is not alive");
        throw std::runtime_error("UDP thread is not alive");
    }

    if (stream_context.channels.empty())
    {
        // Should never happen, but if happens - then nothing to do.
        return 0;
    }

    // Each soapySDR sample(CS16 or CF32) takes 2 our elements (I(short) + Q(short)).
    const size_t data_format_scale_factor = 2;

    const size_t max_elements_in_shorts = numElems * data_format_scale_factor;
    size_t elements_to_read_from_first_channel = 0;

    std::vector<std::vector<short>> read_data_for_channels(4); // 4 channels max

    {
        const size_t first_global_channel_idx = stream_context.channels[0];

        std::unique_lock<std::mutex> lock(udp_rx_context->channels[first_global_channel_idx].mtx); // lock for buffer access

        auto us = std::chrono::microseconds(timeoutUs);
        auto pred = [&]() { return udp_rx_context->channels[first_global_channel_idx].buffer.elementsAvailable() > 0; };
        bool is_signalled = udp_rx_context->channels[first_global_channel_idx].signal.wait_for(lock, us, pred);
        if (is_signalled)
        {
            // Number of elements in first channel limited by input parameter numElems
            elements_to_read_from_first_channel =
                std::min(max_elements_in_shorts, udp_rx_context->channels[first_global_channel_idx].buffer.elementsAvailable());
            read_data_for_channels[0].resize(elements_to_read_from_first_channel);
            udp_rx_context->channels[first_global_channel_idx].buffer.peek(read_data_for_channels[0].data(),
                                                                           elements_to_read_from_first_channel);
            udp_rx_context->channels[first_global_channel_idx].buffer.consume(elements_to_read_from_first_channel);
        }
    }

    if (elements_to_read_from_first_channel == 0)
    {
        return SOAPY_SDR_TIMEOUT;
    }

    // Well, ideally number of data available to read from each channel must be the equal, but for some reason we need
    // this logic. Read data from other channels, but no more than from first channel.
    for (size_t idx = 1; idx < stream_context.channels.size(); idx++)
    {
        size_t global_channel_idx = stream_context.channels[idx];

        std::unique_lock<std::mutex> lock(udp_rx_context->channels[global_channel_idx].mtx); // lock for buffer access
        const size_t elements_to_read =
            std::min(elements_to_read_from_first_channel, udp_rx_context->channels[global_channel_idx].buffer.elementsAvailable());
        read_data_for_channels[idx].resize(elements_to_read);
        udp_rx_context->channels[global_channel_idx].buffer.peek(read_data_for_channels[idx].data(), elements_to_read);
        udp_rx_context->channels[global_channel_idx].buffer.consume(elements_to_read);
    }

    if (stream_context.format == SOAPY_SDR_CS16)
    {
        // for CS16 format we use simple byte-to-byte copy.
        for (size_t idx = 0; idx < stream_context.channels.size(); idx++)
        {
            void *dst_buff = buffs[idx];
            std::memcpy(dst_buff, read_data_for_channels[idx].data(), read_data_for_channels[idx].size() * sizeof(short));
        }
    }
    else if (stream_context.format == SOAPY_SDR_CF32)
    {
        const float F_INT16MAX = 32768.0f;
        // Convert short -> float in loop for each channel.
        for (size_t idx = 0; idx < stream_context.channels.size(); idx++)
        {
            float *dst_buff = (float *)buffs[idx];
            for (size_t j = 0; j < read_data_for_channels[idx].size(); j++)
            {
                *dst_buff = (float)read_data_for_channels[idx][j] / F_INT16MAX;
                dst_buff++;
            }
        }
    }

    return (int)elements_to_read_from_first_channel / (int)data_format_scale_factor;
}
