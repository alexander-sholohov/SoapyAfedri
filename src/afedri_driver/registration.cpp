//  SPDX-FileCopyrightText: 2023 Alexander Sholokhov <ra9yer@yahoo.com>
//  SPDX-License-Identifier: GPL-3.0-or-later
#include "soapy_afedri.hpp"

#include <SoapySDR/Logger.hpp>
#include <SoapySDR/Registry.hpp>

#include <iostream>
#include <sstream>

class WrongParamsError : public std::runtime_error
{
  public:
    WrongParamsError(const std::string &what = "")
        : std::runtime_error(what)
    {
    }
};

struct Params
{
    std::string driver{};
    std::string address{};
    int port{};
    std::string bind_address{"0.0.0.0"};
    int bind_port{};
    bool is_address_present{};
    bool is_port_present{};
    int rx_mode{-1};                // not set by default
    int num_channels{1};            // 1 by default
    int force_selected_channel{-1}; // not active by default

    std::string make_address_port() const
    {
        return std::string(address + ":" + std::to_string(port));
    }

    std::string as_debug_string() const
    {
        std::ostringstream ss;
        ss << "driver=" << driver << " address=" << address << " port=" << port << " bind_address=" << bind_address
           << " bind_port=" << bind_port << " rx_mode=" << rx_mode << " num_channels=" << num_channels
           << " force_selected_channel=" << force_selected_channel << "";
        return ss.str();
    }

    static Params make_from_kwargs(const SoapySDR::Kwargs &args);
};

Params Params::make_from_kwargs(const SoapySDR::Kwargs &args)
{
    Params res;

    if (args.count("driver"))
    {
        res.driver = args.at("driver");
    }

    if (args.count("address"))
    {
        res.address = args.at("address");
        res.is_address_present = true;
    }

    if (args.count("port"))
    {
        res.port = std::stoi(args.at("port"));
        res.is_port_present = true;

        res.bind_port = res.port; // use the same value for bind_port. Can be overwritten later.
    }

    if (args.count("rx_mode"))
    {
        res.rx_mode = std::stoi(args.at("rx_mode"));
    }

    if (args.count("num_channels"))
    {
        res.num_channels = std::stoi(args.at("num_channels"));
    }

    if (args.count("num_channels"))
    {
        res.num_channels = std::stoi(args.at("num_channels"));
    }

    if (args.count("force_set_channel"))
    {
        res.force_selected_channel = std::stoi(args.at("force_set_channel"));
    }

    if (args.count("bind_port"))
    {
        res.bind_port = std::stoi(args.at("bind_port"));
    }

    return res;
}

/***********************************************************************
 * Find available devices
 **********************************************************************/
SoapySDR::KwargsList findMyDevice(const SoapySDR::Kwargs &args)
{
    //(void)args;
    SoapySDR::log(SOAPY_SDR_WARNING, "Afedri trying to find device");
    auto params = Params::make_from_kwargs(args);
    auto res = SoapySDR::KwargsList();

    if (!params.is_address_present || !params.is_port_present)
    {
        SoapySDR::logf(SOAPY_SDR_INFO, "Afedri driver: Skip device detecting because address or port is not specified");
        return res;
    }

    try
    {
        SoapySDR::logf(SOAPY_SDR_INFO, "Afedri driver: Try to make device for params: %s", params.as_debug_string().c_str());
        AfedriDevice ad(params.address, params.port, params.bind_address, params.bind_port, params.rx_mode, params.num_channels,
                        params.force_selected_channel);
        auto m = SoapySDR::Kwargs();
        auto label = std::string("afedri :: " + params.make_address_port());
        m["label"] = label;

        m["version_string"] = ad.get_version_info().version_string;

        res.push_back(m);

        SoapySDR::logf(SOAPY_SDR_INFO, "Afedri device detected: %s", label.c_str());
    }
    catch (const std::exception &)
    {
        // This means afedri hasn't been detected.
    }

    return res;
}

/***********************************************************************
 * Make device instance
 **********************************************************************/
SoapySDR::Device *makeMyDevice(const SoapySDR::Kwargs &args)
{
    //(void)args;
    SoapySDR::log(SOAPY_SDR_WARNING, "Afedri is making device:");
    for (auto it = args.begin(); it != args.end(); ++it)
    {
        SoapySDR::logf(SOAPY_SDR_INFO, "afedri_key: %s - %s", it->first.c_str(), it->second.c_str());
    }

    auto params = Params::make_from_kwargs(args);
    SoapySDR::logf(SOAPY_SDR_INFO, "Afedri driver: Making device for params: %s", params.as_debug_string().c_str());

    return new AfedriDevice(params.address, params.port, params.bind_address, params.bind_port, params.rx_mode, params.num_channels,
                            params.force_selected_channel);
}

/***********************************************************************
 * Registration
 **********************************************************************/
static SoapySDR::Registry registerMyDevice("afedri", &findMyDevice, &makeMyDevice, SOAPY_SDR_ABI_VERSION);
