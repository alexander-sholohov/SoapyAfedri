//  SPDX-FileCopyrightText: 2023 Alexander Sholokhov <ra9yer@yahoo.com>
//  SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

#include "simple_tcp_communicator.hpp"

class AfedriLogicError : public std::runtime_error
{
  public:
    AfedriLogicError(const std::string &what = "")
        : std::runtime_error(what)
    {
    }
};

class AfedriControl
{
  public:
    AfedriControl(std::string const &address, int port);
    AfedriControl(AfedriControl const &) = delete;

    enum class Channel
    {
        CH0 = 0,
        CH1 = 1,
        CH2 = 2,
        CH3 = 3,
    };

    enum class RxMode
    {
        SingleChannelMode = 0,
        DualDiversityMode = 1,
        DualChannelMode = 2,
        DiversityInternalAddMode = 3, // in this mode udp stream has one channel
        QuadDiversityMode = 4,
        QuadChannelMode = 5,
    };

    static Channel make_afedri_channel_from_0based_index(size_t channel_index);

    struct VersionInfo
    {
        std::string version_string;
        std::string serial_number;
        std::string firmware_version;
        std::string product_id;
        std::string hw_fw_version;
        std::string interface_version;
        std::uint32_t main_clock_frequency{};
        int diversity_mode{};
    };

    VersionInfo get_version_info();
    std::uint32_t read_eeprom(std::uint32_t address);

    // stream control
    void start_capture();
    void stop_capture();
    bool is_capturing();
    // frequency
    void set_frequency(Channel channel, std::uint32_t freq);
    std::uint32_t get_frequency(Channel channel);
    // sample rate
    void set_sample_rate(Channel channel, std::uint32_t samp_rate);
    std::uint32_t get_sample_rate(Channel channel);
    // gain
    void set_rf_gain_notused(Channel channel, double gain);
    void set_af_gain_notused(Channel channel, double gain);
    //
    void set_fe_gain(Channel channel, double gain);
    void set_rf_gain(Channel channel, double gain);
    //
    void set_r820t_lna_gain(Channel channel, double gain);
    void set_r820t_mixer_gain(Channel channel, double gain);
    void set_r820t_vga_gain(Channel channel, double gain);
    // agc
    void set_r820t_lna_agc(Channel channel, int mode);
    void set_r820t_mixer_agc(Channel channel, int mode);
    //
    void set_overload_mode(int mode); // bit masks for 4 channels.
    void set_rx_mode(Channel channel, RxMode mode);

    static std::uint32_t calc_actual_sample_rate(std::uint32_t quartz, std::uint32_t samp_rate);

  private:
    static unsigned char make_internal_channel(Channel channel);

    SimpleTcpCommunicator _comm;
};
