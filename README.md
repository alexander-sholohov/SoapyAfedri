# Soapy SDR plugin for Afedri SDR-Net

## What is this thing?

The SoapyAfedri project provides a plugin module to use the Afedri SDR-Net device within the SoapySDR API and software that supports SoapySDR. This driver supports only TCP/UDP communication transport with the device. 


## Dependencies

* SoapySDR - https://github.com/pothosware/SoapySDR/wiki


```shell
sudo apt-get install build-essential
sudo apt-get install cmake

```

## Build with cmake

```shell
git clone https://github.com/alexander-sholohov/SoapyAfedri.git
cd SoapyAfedri
mkdir build
cd build
cmake ..
cmake --build . 
sudo cmake --install .
```

## Probing Soapy Afedri:

```shell
SoapySDRUtil --find
SoapySDRUtil --probe="driver=afedri,address=192.168.1.41,port=61000"
```

### Tested with:
- OpenWebRX
- SDR++
