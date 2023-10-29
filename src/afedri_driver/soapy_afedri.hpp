//  SPDX-FileCopyrightText: 2023 Alexander Sholokhov <ra9yer@yahoo.com>
//  SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <SoapySDR/Device.hpp>

#include "afedri_control.hpp"
#include "udp_rx.hpp"

struct StreamContext
{
    StreamContext() = default;
    StreamContext(std::vector<size_t> channels, std::string format, bool active)
        : channels(std::move(channels)), format(std::move(format)), active(active)
    {
    }

    std::vector<size_t> channels;
    std::string format;
    bool active;
};

/***********************************************************************
 * Device interface
 **********************************************************************/
class AfedriDevice : public SoapySDR::Device
{
  public:
    AfedriDevice(std::string const &address, int port, std::string const &bind_address, int bind_port, int afedri_mode, int num_channels,
                 int map_ch0);

    std::string getDriverKey(void) const override;

    std::string getHardwareKey(void) const override;

    SoapySDR::Kwargs getHardwareInfo(void) const override;

    /*******************************************************************
     * Channels API
     ******************************************************************/

    size_t getNumChannels(const int dir) const override;

    bool getFullDuplex(const int /*direction*/, const size_t /*channel*/) const override;

    /*******************************************************************
     * Gain API
     ******************************************************************/

    std::vector<std::string> listGains(const int direction, const size_t channel) const override;

    void setGain(const int direction, const size_t channel, const double value) override;

    void setGain(const int direction, const size_t channel, const std::string &name, const double value) override;

    double getGain(const int direction, const size_t channel, const std::string &name) const override;

    SoapySDR::Range getGainRange(const int direction, const size_t channel, const std::string &name) const override;

    /*******************************************************************
     * Frequency API
     ******************************************************************/

    void setFrequency(const int direction, const size_t channel, const std::string &name, const double frequency,
                      const SoapySDR::Kwargs &args = SoapySDR::Kwargs()) override;

    double getFrequency(const int direction, const size_t channel, const std::string &name) const override;

    std::vector<std::string> listFrequencies(const int direction, const size_t channel) const override;

    SoapySDR::RangeList getFrequencyRange(const int direction, const size_t channel, const std::string &name) const override;

    SoapySDR::ArgInfoList getFrequencyArgsInfo(const int direction, const size_t channel) const override;

    /*******************************************************************
     * Sample Rate API
     ******************************************************************/

    void setSampleRate(const int direction, const size_t channel, const double rate) override;

    double getSampleRate(const int direction, const size_t channel) const override;

    std::vector<double> listSampleRates(const int direction, const size_t channel) const override;

    SoapySDR::RangeList getSampleRateRange(const int direction, const size_t channel) const override;

    void setBandwidth(const int direction, const size_t channel, const double bw) override;

    double getBandwidth(const int direction, const size_t channel) const override;

    std::vector<double> listBandwidths(const int direction, const size_t channel) const override;

    SoapySDR::RangeList getBandwidthRange(const int direction, const size_t channel) const override;

    /*******************************************************************
     * Stream API
     ******************************************************************/

    std::vector<std::string> getStreamFormats(const int direction, const size_t channel) const override;

    std::string getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const override;

    SoapySDR::ArgInfoList getStreamArgsInfo(const int direction, const size_t channel) const override;

    /*******************************************************************
     * Stream API
     ******************************************************************/

    SoapySDR::Stream *setupStream(const int direction, const std::string &format,
                                  const std::vector<size_t> &channels = std::vector<size_t>(),
                                  const SoapySDR::Kwargs &args = SoapySDR::Kwargs()) override;

    void closeStream(SoapySDR::Stream *stream) override;

    size_t getStreamMTU(SoapySDR::Stream *stream) const override;

    int activateStream(SoapySDR::Stream *stream, const int flags = 0, const long long timeNs = 0, const size_t numElems = 0) override;

    int deactivateStream(SoapySDR::Stream *stream, const int flags = 0, const long long timeNs = 0) override;

    int readStream(SoapySDR::Stream *stream, void *const *buffs, const size_t numElems, int &flags, long long &timeNs,
                   const long timeoutUs = 100000) override;

    /*******************************************************************
     * Antenna API
     ******************************************************************/
    std::vector<std::string> listAntennas(const int direction, const size_t channel) const override;

    void setAntenna(const int direction, const size_t channel, const std::string &name) override;

    std::string getAntenna(const int direction, const size_t channel) const override;

    /*******************************************************************
     * Settings API
     ******************************************************************/
    void writeSetting(const std::string &key, const std::string &value) override;

  public:
    AfedriControl::VersionInfo const &get_version_info() const;

  protected:
    StreamContext &get_stream_context_by_id(int stream_id);

  private:
    size_t remap_channel(size_t soapy_incoming_channel) const;

    AfedriControl _afedri_control;
    std::string _bind_address;
    int _bind_port;

    int _afedri_rx_mode;  // [0,5] (Single/DualDiversity/Dual/DiversityInternal/QuadDiversity/Quad)
    size_t _num_channels; // can be 1,2 or 4.
    int _map_ch0;         // -1 if remap is not active

    std::mutex _streams_protect_mtx; // protection for _configured_streams
    int _stream_sequence_provider;
    std::map<int, StreamContext> _configured_streams;

    std::map<std::string, double> _saved_gains;
    double _saved_frequency;
    double _saved_sample_rate;
    double _saved_bandwidth;
    std::string _saved_antenna;

    std::unique_ptr<UdpRxContextDefer> _udp_rx_thread_defer;
    AfedriControl::VersionInfo _version_info;
};
