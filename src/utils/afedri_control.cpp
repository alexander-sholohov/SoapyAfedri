//  SPDX-FileCopyrightText: 2023 Alexander Sholokhov <ra9yer@yahoo.com>
//  SPDX-License-Identifier: GPL-3.0-or-later
#include "afedri_control.hpp"

#include "simple_tcp_communicator.hpp"

#include "inet_common.h"

#include <cassert>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

constexpr int default_wait_time = 1500; // in ms

static void vec_fill_pads(std::vector<unsigned char> &buf)
{
    int desired_length = buf[0];
    int pad_length = desired_length - static_cast<int>(buf.size());
    for (int i = 0; i < pad_length; i++)
    {
        buf.push_back(0);
    }
}

static void vec_push_uint32(std::uint32_t value, std::vector<unsigned char> &buf)
{
    for (size_t i = 0; i < 4; i++)
    {
        buf.push_back(value & 0xff);
        value = value >> 8;
    }
}

static std::uint32_t buf2int(unsigned char *buf)
{
    std::uint32_t res = 0;
    for (size_t i = 0; i < 4; i++)
    {
        res = res * 256 + buf[3 - i];
    }
    return res;
}

static double clamp(double value, double minv, double maxv)
{
    assert(minv < maxv);
    double res = value;
    if (res < minv)
    {
        res = minv;
    }

    if (res > maxv)
    {
        res = maxv;
    }

    return res;
}

static int linear_gain_map(double gain, double in_gain_from, double in_gain_to, int out_gain_from, int out_gain_to)
{
    const double gain1 = clamp(gain, in_gain_from, in_gain_to);
    const double part = (gain1 - in_gain_from) / (in_gain_to - in_gain_from);
    const double res = (double)out_gain_from + part * (out_gain_to - out_gain_from);
    const double res1 = std::floor(res + 0.5);
    return (int)res1;
}

unsigned char AfedriControl::make_internal_channel(Channel channel)
{
    // CH0,CH1,CH2,CH3, => 0,2,3,4

    unsigned char res = 0;
    switch (channel)
    {
    case Channel::CH0:
        res = 0;
        break;
    case Channel::CH1:
        res = 2;
        break;
    case Channel::CH2:
        res = 3;
        break;
    case Channel::CH3:
        res = 4;
        break;
    default:
        throw std::runtime_error("wrong channel");
    }

    return res;
}

AfedriControl::Channel AfedriControl::make_afedri_channel_from_0based_index(size_t channel_index)
{
    return static_cast<Channel>(channel_index);
}

int AfedriControl::rx_mode_to_number_of_channels(AfedriControl::RxMode rx_mode)
{
    int res = 1;
    switch (rx_mode)
    {
    case AfedriControl::RxMode::SingleChannelMode:
        res = 1;
        break;
    case AfedriControl::RxMode::DualDiversityMode:
        res = 2;
        break;
    case AfedriControl::RxMode::DualChannelMode:
        res = 2;
        break;
    case AfedriControl::RxMode::DiversityInternalAddMode:
        res = 1;
        break;
    case AfedriControl::RxMode::QuadDiversityMode:
        res = 4;
        break;
    case AfedriControl::RxMode::QuadChannelMode:
        res = 4;
        break;
    }

    return res;
}

AfedriControl::AfedriControl(std::string const &address, int port)
    : _comm(address, port)
{
}

static bool complete_read_condition(std::vector<unsigned char> const &buf)
{
    // first byte is length of packet in bytes
    return !buf.empty() && buf.size() == buf[0];
}

AfedriControl::VersionInfo AfedriControl::get_version_info()
{
    VersionInfo ret;

    // Target name
    {
        std::vector<unsigned char> v = {0x4, 0x20, 0x1, 0x0};
        assert(v.size() == v[0]);
        _comm.send(v);
        auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);

        if (rx_buf.size() < 5)
        {
            throw AfedriLogicError("get_version_info wrong reply");
        }
        rx_buf.push_back(0); // to be sure we have null char in the end

        ret.version_string = (char *)&rx_buf[4];
    }

    // main clock freq
    {
        auto low_word = read_eeprom(0);
        auto high_word = read_eeprom(1);
        std::uint32_t freq = high_word << 16 | low_word;
        if (freq == 0)
        {
            freq = 80000000;
        }
        ret.main_clock_frequency = freq;
    }

    // diversity
    ret.eeprom_diversity_mode = read_eeprom(8);

    // r820t specific
    ret.is_r820t_present = is_r820t_present();

    // HW FW version
    {
        std::vector<unsigned char> v = {0x4, 0x20, 0x4, 0x0};
        assert(v.size() == v[0]);
        _comm.send(v);
        auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
        std::ostringstream ss;
        for (size_t idx = 4; idx < rx_buf.size(); idx++)
        {
            ss << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(rx_buf[idx]);
        }
        ret.hw_fw_version = ss.str();
    }

    // interface version
    {
        std::vector<unsigned char> v = {0x4, 0x20, 0x3, 0x0};
        assert(v.size() == v[0]);
        _comm.send(v);
        auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
        std::ostringstream ss;
        for (size_t idx = 4; idx < rx_buf.size(); idx++)
        {
            ss << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(rx_buf[idx]);
        }
        ret.interface_version = ss.str();
    }

    // Serial number
    {
        std::vector<unsigned char> v = {0x4, 0x20, 0x2, 0x0};
        assert(v.size() == v[0]);
        _comm.send(v);
        auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
        rx_buf.push_back(0);
        ret.serial_number = (char *)&rx_buf[4];
    }

    // Product ID
    {
        std::vector<unsigned char> v = {0x4, 0x20, 0x9, 0x0};
        assert(v.size() == v[0]);
        _comm.send(v);
        auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
        std::ostringstream ss;
        for (size_t idx = 0; idx < 3; idx++)
        {
            ss << static_cast<char>(rx_buf[4 + idx]);
        }
        ss << "/" << static_cast<int>(rx_buf[7]); // Not sure what is it
        ret.product_id = ss.str();
    }

    {
        std::vector<unsigned char> v = {0x9, 0xe0, 0x2, 9};
        vec_fill_pads(v);
        assert(v.size() == v[0]);
        _comm.send(v);
        auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
        std::ostringstream ss;
        for (size_t idx = 0; idx < 4; idx++)
        {
            ss << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(rx_buf[7 - idx]);
        }
        ret.firmware_version = ss.str();
    }

    return ret;
}

std::uint32_t AfedriControl::read_eeprom(std::uint32_t address)
{
    std::vector<unsigned char> v = {0x9, 0xe0, 0x2, 0x55};
    v.push_back(address);
    vec_fill_pads(v);
    assert(v.size() == v[0]);
    _comm.send(v);
    auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
    std::uint32_t res = *(std::uint32_t *)&rx_buf[4] & 0xffff;
    return res;
}

void AfedriControl::start_capture()
{
    std::vector<unsigned char> v = {0x8, 0x0, 0x18, 0x0, 0x80, 0x2, 0x0, 0x0};
    assert(v.size() == v[0]);
    _comm.send(v);
    auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
}

void AfedriControl::stop_capture()
{
    std::vector<unsigned char> v = {0x8, 0x0, 0x18, 0x0, 0x80, 0x1, 0x0, 0x0};
    assert(v.size() == v[0]);
    _comm.send(v);
    auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
}

bool AfedriControl::is_capturing()
{
    std::vector<unsigned char> v = {0x4, 0x20, 0x18, 0x0};
    assert(v.size() == v[0]);
    _comm.send(v);
    auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
    auto state = rx_buf[5];
    if (state != 1 && state != 2)
    {
        throw AfedriLogicError("Unexpected rx state");
    }
    return state == 2;
}

void AfedriControl::set_frequency(AfedriControl::Channel channel, std::uint32_t freq)
{
    std::vector<unsigned char> v = {0xa, 0x0, 0x20, 0x0};
    v.push_back(make_internal_channel(channel));
    vec_push_uint32(freq, v);
    v.push_back(0);
    assert(v.size() == v[0]);
    _comm.send(v);
    auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
}

std::uint32_t AfedriControl::get_frequency(Channel channel)
{
    std::vector<unsigned char> v = {0x5, 0x20, 0x20, 0x0};
    v.push_back(make_internal_channel(channel));
    assert(v.size() == v[0]);
    _comm.send(v);
    auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);

    if (rx_buf.size() < 10)
    {
        throw AfedriLogicError("get_frequency wrong reply length");
    }

    return buf2int(&rx_buf[5]);
}

void AfedriControl::set_sample_rate(Channel /*channel*/, std::uint32_t sample_rate)
{
    std::vector<unsigned char> v = {0x9, 0x0, 0xb8, 0x0};
    v.push_back(0); // channel is always 0 for sample rate
    vec_push_uint32(sample_rate, v);
    assert(v.size() == v[0]);
    _comm.send(v);
    auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
}

std::uint32_t AfedriControl::get_sample_rate(Channel /*channel*/)
{
    std::vector<unsigned char> v = {0x5, 0x20, 0xb8, 0x0};
    v.push_back(0); // channel is always 0 for sample rate
    assert(v.size() == v[0]);
    _comm.send(v);
    auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);

    return buf2int(&rx_buf[5]);
}

void AfedriControl::set_rf_gain_notused(Channel channel, double gain)
{
    std::vector<unsigned char> v = {0x6, 0x0, 0x38, 0x0};
    v.push_back(make_internal_channel(channel));
    v.push_back(static_cast<unsigned char>(gain)); // TODO:
    assert(v.size() == v[0]);
    _comm.send(v);
    auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
}

void AfedriControl::set_af_gain_notused(Channel channel, double gain)
{
    std::vector<unsigned char> v = {0x6, 0x0, 0x48, 0x0};
    v.push_back(make_internal_channel(channel));
    v.push_back(static_cast<unsigned char>(gain)); // TODO:
    assert(v.size() == v[0]);
    _comm.send(v);
    auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
}

// FE Gain. It works on HF and VHF(r820t)
// [1,7] 7 steps total 12db
void AfedriControl::set_fe_gain(Channel channel, double gain)
{
    std::vector<unsigned char> v = {0x9, 0xe0, 0x2, 0x02};
    v.push_back(linear_gain_map(gain, 0.0, +12.0, 1, 7)); // linear map [0db;+12db] -> [1;7]
    v.push_back(make_internal_channel(channel));          // TODO: check
    vec_fill_pads(v);
    assert(v.size() == v[0]);
    _comm.send(v);
    auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
}

// RF Gain. It works on HF only
// [-10db .. +35db] in 3db steps
void AfedriControl::set_rf_gain(Channel channel, double gain)
{
    // map [-10db;+35db] -> [0x00;0x78] with 3db step, low 3 bits are always zero.
    double gain2 = clamp(gain, -10.0, +35.0);
    double v1 = std::floor((gain2 + 10.0) / 3.0 + 0.5);
    int res_gain = (int)v1 << 3;

    std::vector<unsigned char> v = {0x9, 0xe0, 0x2, 0x08};
    v.push_back(res_gain);
    v.push_back(make_internal_channel(channel)); // TODO: check
    vec_fill_pads(v);
    assert(v.size() == v[0]);
    _comm.send(v);
    auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
}

// datasheet -7.5db - +35db
void AfedriControl::set_r820t_lna_gain(Channel channel, double gain)
{
    std::vector<unsigned char> v = {0x9, 0xe0, 0x2, 0x4f};
    v.push_back(linear_gain_map(gain, -7.5, +35.0, 0, 0xf)); // linear map [-7.5db;+35db] -> [0;15]
    v.push_back(make_internal_channel(channel));             // TODO: check
    vec_fill_pads(v);
    assert(v.size() == v[0]);
    _comm.send(v);
    auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
}

//
void AfedriControl::set_r820t_mixer_gain(Channel channel, double gain)
{
    std::vector<unsigned char> v = {0x9, 0xe0, 0x2, 0x50};
    v.push_back(linear_gain_map(gain, 0.0, +2.0, 0, 0xf)); // linear map [0db;+2db] -> [0;15]
    v.push_back(make_internal_channel(channel));           // TODO: check
    vec_fill_pads(v);
    assert(v.size() == v[0]);
    _comm.send(v);
    auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
}

// datasheet  +1db +48db
void AfedriControl::set_r820t_vga_gain(Channel channel, double gain)
{
    std::vector<unsigned char> v = {0x9, 0xe0, 0x2, 0x51};
    v.push_back(linear_gain_map(gain, +1.0, +48.0, 0, 0xf)); // linear map [+1db;+48db] -> [0;15]
    v.push_back(make_internal_channel(channel));             // TODO: check
    vec_fill_pads(v);
    assert(v.size() == v[0]);
    _comm.send(v);
    auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
}

void AfedriControl::set_r820t_lna_agc(Channel channel, int mode)
{
    std::vector<unsigned char> v = {0x9, 0xe0, 0x2, 0x52};
    v.push_back(static_cast<unsigned char>(mode));
    v.push_back(make_internal_channel(channel)); // TODO: check
    vec_fill_pads(v);
    assert(v.size() == v[0]);
    _comm.send(v);
    auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
}

void AfedriControl::set_r820t_mixer_agc(Channel channel, int mode)
{
    std::vector<unsigned char> v = {0x9, 0xe0, 0x2, 0x53};
    v.push_back(static_cast<unsigned char>(mode));
    v.push_back(make_internal_channel(channel)); // TODO: check
    vec_fill_pads(v);
    assert(v.size() == v[0]);
    _comm.send(v);
    auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
}

void AfedriControl::set_overload_mode(int mode)
{
    std::vector<unsigned char> v = {0x9, 0xe0, 0x2, 0x45};
    v.push_back(static_cast<unsigned char>(mode)); //  4 low bits for channels.
    vec_fill_pads(v);
    assert(v.size() == v[0]);
    _comm.send(v);
    auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
}

void AfedriControl::set_rx_mode(Channel channel, RxMode mode)
{
    std::vector<unsigned char> v = {0x9, 0xe0, 0x2, 0x30};
    v.push_back(static_cast<unsigned char>(mode));
    v.push_back(make_internal_channel(channel)); // TODO: check
    vec_fill_pads(v);
    assert(v.size() == v[0]);
    _comm.send(v);
    auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
}

AfedriControl::RxMode AfedriControl::get_rx_mode()
{
    std::vector<unsigned char> v = {0x9, 0xe0, 0x2, 0xf};
    vec_fill_pads(v);
    assert(v.size() == v[0]);
    _comm.send(v);
    auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
    if (rx_buf.size() < 9 || rx_buf[3] != 0xf)
    {
        return RxMode::SingleChannelMode; // on error return single channel
    }

    return static_cast<RxMode>(rx_buf[4]);
}

bool AfedriControl::is_r820t_present()
{
    // HID_GENERIC_GET_R820T_REF_FREQ_COMMAND
    std::vector<unsigned char> v = {0x9, 0xe0, 0x2, 0x5b};
    vec_fill_pads(v);
    assert(v.size() == v[0]);
    _comm.send(v);

    auto rx_buf = _comm.read_with_timeout(default_wait_time, complete_read_condition);
    if (rx_buf.size() < 9)
    {
        return false;
    }
    // For some new devices this solution gives us false positive result.
    return (rx_buf[3] == 0x5b && (rx_buf[4] || rx_buf[5] || rx_buf[6] || rx_buf[7]));
}

std::uint32_t AfedriControl::calc_actual_sample_rate(std::uint32_t quartz, std::uint32_t samp_rate)
{
    const float m1 = (float)quartz / (4.0f * (float)samp_rate);
    const float m2 = std::floor(m1 + 0.5f);
    const float samp_rate1 = quartz / (4.0f * m2);
    const float samp_rate2 = std::floor(samp_rate1 + 0.5f);
    return (std::uint32_t)samp_rate2;
}
