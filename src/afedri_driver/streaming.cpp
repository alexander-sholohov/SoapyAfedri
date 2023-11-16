//  SPDX-FileCopyrightText: 2023 Alexander Sholokhov <ra9yer@yahoo.com>
//  SPDX-License-Identifier: GPL-3.0-or-later
#include "soapy_afedri.hpp"

#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Logger.hpp>

#include "afedri_control.hpp"
#include "udp_rx.hpp"

#include <cstring>
#include <sstream>

// we have this due to lack of support std find_if in c++14
static StreamsWithinChannel::iterator my_find_if(StreamsWithinChannel::iterator first, StreamsWithinChannel::iterator last,
                                                 std::function<bool(const StreamItem &)> pred)
{
    while (first != last)
    {
        if (pred(*first))
            return first;
        ++first;
    }
    return last;
}

SoapySDR::Stream *AfedriDevice::setupStream(const int direction, const std::string &format, const std::vector<size_t> &channels,
                                            const SoapySDR::Kwargs & /*args*/)
{
    SoapySDR::logf(SOAPY_SDR_INFO, "Afedri in setupStream. Num_channels=%d, format=%s", channels.size(), format.c_str());

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

    std::string selected_format;

    // Check the format
    if (format == SOAPY_SDR_CF32)
    {
        selected_format = format;
    }
    else if (format == SOAPY_SDR_CS16)
    {
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

    // Add StreamItem to udp rx context
    {
        auto udp_rx_ctx = _udp_rx_thread_defer->get_ctx();
        std::unique_lock<std::mutex> lock(udp_rx_ctx->mtx_channel);
        for (size_t channel_id : wrk_channels)
        {
            auto &stream = udp_rx_ctx->channels[channel_id];
            // find free StreamItem slot or create new one
            auto pred = [](const StreamItem &stream_item) -> bool { return stream_item.unique_stream_id == 0; };
            auto stream_it = my_find_if(stream.begin(), stream.end(), pred);
            if (stream_it != stream.end())
            {
                stream_it->unique_stream_id = just_obtained_stream_id;
            }
            else
            {
                stream.emplace_back(just_obtained_stream_id);
            }
        }
    }

    // Debug output
    {
        std::ostringstream ss;
        ss << "Afedri: stream_id=" << just_obtained_stream_id << ", actual_channels=[";
        for (size_t idx = 0; idx < wrk_channels.size(); idx++)
        {
            if (idx != 0)
                ss << ",";
            ss << wrk_channels[idx];
        }
        ss << "], format=" << selected_format;
        auto s = ss.str();
        SoapySDR::log(SOAPY_SDR_INFO, s.c_str());
    }

    return (SoapySDR::Stream *)(new int(just_obtained_stream_id));
}

void AfedriDevice::closeStream(SoapySDR::Stream *stream)
{
    int stream_id = *reinterpret_cast<int *>(stream);
    SoapySDR::logf(SOAPY_SDR_DEBUG, "Afedri in closeStream stream_id=%d", stream_id);

    this->deactivateStream(stream, 0, 0);

    // Remove StreamItem from udp rx context channels (make it inactive)
    {
        auto udp_rx_ctx = _udp_rx_thread_defer->get_ctx();
        std::unique_lock<std::mutex> lock(udp_rx_ctx->mtx_channel);

        // scan through all channels and make strem_items slot inactive
        for (auto &channel : udp_rx_ctx->channels)
        {
            for (auto &stream_item : channel)
            {
                if (stream_item.unique_stream_id == stream_id)
                {
                    stream_item.unique_stream_id = 0;
                }
            }
        }
    }

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
    const int stream_id = *reinterpret_cast<int *>(stream);
    SoapySDR::logf(SOAPY_SDR_DEBUG, "Afedri in activateStream stream_id=%d flags=%d ", stream_id, flags);

    if (flags != 0)
    {
        return SOAPY_SDR_NOT_SUPPORTED;
    }

    StreamContext &stream_context = get_stream_context_by_id(stream_id);
    stream_context.active = true;

    _afedri_control.start_capture(); // Activate stream. Multiple calls - not a problem.
    SoapySDR::logf(SOAPY_SDR_DEBUG, "Afedri start capture");

    _udp_rx_thread_defer->get_ctx()->rx_active = true; // flag to allow process UDP RX data

    return 0;
}

int AfedriDevice::deactivateStream(SoapySDR::Stream *stream, const int flags, const long long /*timeNs*/)
{
    const int stream_id = *reinterpret_cast<int *>(stream);
    SoapySDR::logf(SOAPY_SDR_DEBUG, "Afedri in deactivateStream stream_id=%d, flags=%d", stream_id, flags);

    if (flags != 0)
    {
        return SOAPY_SDR_NOT_SUPPORTED;
    }

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

    SoapySDR::logf(SOAPY_SDR_INFO, "Afedri num_active_streams=%d", num_active_streams);

    if (num_active_streams == 0)
    {
        _afedri_control.stop_capture();
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

    auto stream_find_pred = [stream_id](const StreamItem &stream_item) -> bool { return stream_item.unique_stream_id == stream_id; };

    {
        auto &stream = udp_rx_context->channels[stream_context.channels[0]];
        auto stream_it = my_find_if(stream.begin(), stream.end(), stream_find_pred);

        std::unique_lock<std::mutex> lock(stream_it->mtx);

        auto us = std::chrono::microseconds(timeoutUs);
        auto pred = [&stream_it]() { return stream_it->buffer.elementsAvailable() > 0; };
        bool is_signalled = stream_it->signal.wait_for(lock, us, pred);
        if (is_signalled)
        {
            // Number of elements in first channel limited by input parameter numElems
            elements_to_read_from_first_channel = std::min(max_elements_in_shorts, stream_it->buffer.elementsAvailable());

            read_data_for_channels[0].resize(elements_to_read_from_first_channel);
            stream_it->buffer.peek(read_data_for_channels[0].data(), elements_to_read_from_first_channel);
            stream_it->buffer.consume(elements_to_read_from_first_channel);
        }
    }

    if (elements_to_read_from_first_channel == 0)
    {
        return SOAPY_SDR_TIMEOUT;
    }

    // number of data available to read from each channel must be the equal, but for some reason we need a protection logic:
    // read data from other channels, but no more than from the first channel.
    for (size_t idx = 1; idx < stream_context.channels.size(); idx++)
    {
        auto &stream = udp_rx_context->channels[stream_context.channels[idx]];
        auto stream_it = my_find_if(stream.begin(), stream.end(), stream_find_pred);

        std::unique_lock<std::mutex> lock(stream_it->mtx); // lock for buffer access
        const size_t elements_to_read = std::min(elements_to_read_from_first_channel, stream_it->buffer.elementsAvailable());
        read_data_for_channels[idx].resize(elements_to_read);
        stream_it->buffer.peek(read_data_for_channels[idx].data(), elements_to_read);
        stream_it->buffer.consume(elements_to_read);
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
