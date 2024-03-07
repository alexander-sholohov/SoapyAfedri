//  SPDX-FileCopyrightText: 2023 Alexander Sholokhov <ra9yer@yahoo.com>
//  SPDX-License-Identifier: GPL-3.0-or-later
#include "soapy_afedri.hpp"

#include "afedri_discovery.hpp"

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
    int rx_mode{-1};     // not set by default
    int num_channels{0}; // 0 - means must be set automatically
    int map_ch0{-1};     // not active by default

    std::string make_address_port() const
    {
        return std::string(address + ":" + std::to_string(port));
    }

    std::string as_debug_string() const
    {
        std::ostringstream ss;
        ss << "driver=" << driver << " address=" << address << " port=" << port << " bind_address=" << bind_address
           << " bind_port=" << bind_port << " rx_mode=" << rx_mode << " num_channels=" << num_channels << " map_ch0=" << map_ch0 << "";
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
    }

    if (args.count("port"))
    {
        res.port = std::stoi(args.at("port"));
        res.bind_port = res.port; // use the same value for bind_port. Can be overwritten later.
    }

    if (args.count("bind_address"))
    {
        res.bind_address = args.at("bind_address");
    }

    if (args.count("bind_port"))
    {
        res.bind_port = std::stoi(args.at("bind_port"));
    }

    if (args.count("rx_mode"))
    {
        res.rx_mode = std::stoi(args.at("rx_mode"));
    }

    if (args.count("num_channels"))
    {
        res.num_channels = std::stoi(args.at("num_channels"));
    }

    if (args.count("map_ch0"))
    {
        res.map_ch0 = std::stoi(args.at("map_ch0"));
    }

    return res;
}

static void myWSAStartup()
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        throw std::runtime_error("WSAStartup error");
    }
#endif
}

/***********************************************************************
 * Find available devices
 **********************************************************************/
SoapySDR::KwargsList findMyDevice(const SoapySDR::Kwargs &args)
{
    //(void)args;
    SoapySDR::log(SOAPY_SDR_WARNING, "Afedri trying to find device");

    myWSAStartup();

    auto res = SoapySDR::KwargsList();

    const auto devices = AfedriDiscovery::discovery();

    if (devices.empty())
    {
        SoapySDR::logf(SOAPY_SDR_INFO, "Afedri driver: no any device found");
    }

    for (auto const &dev : devices)
    {
        const bool isAddressMatch = args.count("address") == 0 || args.at("address") == dev.address;
        const bool isPortMatch = args.count("port") == 0 || args.at("port") == std::to_string(dev.port);
        if (!isAddressMatch || !isPortMatch)
        {
            continue;
        }

        auto m = SoapySDR::Kwargs();
        auto label = std::string("afedri :: " + dev.address + ":" + std::to_string(dev.port));
        m["label"] = label;
        m["address"] = dev.address;
        m["port"] = std::to_string(dev.port);
        m["serial"] = dev.serial_number;
        m["version_string"] = dev.name;
        res.push_back(m);
    }

    bool isAddressAndPortProvided = args.count("address") != 0 && args.count("port") != 0;
    if (res.empty() && isAddressAndPortProvided)
    {
        // adress and port was provided, but we haven't found this device by discovery process
        // in this case we try to instantiate the device explicitly

        auto params = Params::make_from_kwargs(args);

        try
        {
            SoapySDR::logf(SOAPY_SDR_INFO, "Afedri driver: Force try to make device for params: %s", params.as_debug_string().c_str());
            AfedriDevice ad(params.address, params.port, params.bind_address, params.bind_port, params.rx_mode, params.num_channels,
                            params.map_ch0);
            auto m = SoapySDR::Kwargs();
            auto label = std::string("afedri :: " + params.make_address_port());
            m["label"] = label;

            m["version_string"] = ad.get_version_info().version_string;

            res.push_back(m);

            SoapySDR::logf(SOAPY_SDR_INFO, "Afedri device detected: %s", label.c_str());
        }
        catch (const std::exception &)
        {
            // we are not interesting in the error
            // this simply means afedri hasn't been detected
        }
    }

    return res;
}

/***********************************************************************
 * Make device instance
 **********************************************************************/
SoapySDR::Device *makeMyDevice(const SoapySDR::Kwargs &args)
{
    //(void)args;
    myWSAStartup();

    SoapySDR::log(SOAPY_SDR_WARNING, "Afedri is making device:");
    for (auto it = args.begin(); it != args.end(); ++it)
    {
        SoapySDR::logf(SOAPY_SDR_INFO, "afedri_key: %s - %s", it->first.c_str(), it->second.c_str());
    }

    const bool isAddressAndPortProvided = args.count("address") != 0 && args.count("port") != 0;
    if (!isAddressAndPortProvided)
    {
        throw std::runtime_error("Unable to create Afedri device without address and port");
    }

    auto params = Params::make_from_kwargs(args);
    SoapySDR::logf(SOAPY_SDR_INFO, "Afedri driver: Making device for params: %s", params.as_debug_string().c_str());

    return new AfedriDevice(params.address, params.port, params.bind_address, params.bind_port, params.rx_mode, params.num_channels,
                            params.map_ch0);
}

/***********************************************************************
 * Registration
 **********************************************************************/
static SoapySDR::Registry registerMyDevice("afedri", &findMyDevice, &makeMyDevice, SOAPY_SDR_ABI_VERSION);
