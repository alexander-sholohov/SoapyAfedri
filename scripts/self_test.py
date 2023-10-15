import SoapySDR
import numpy as np


def main():
    SoapySDR.setLogLevel(SoapySDR.SOAPY_SDR_DEBUG)
    af = SoapySDR.Device(dict(driver="afedri", address="192.168.1.41", port="61000"))
    print(af)
    num_channels = af.getNumChannels(SoapySDR.SOAPY_SDR_RX)
    print(f"num_channels={num_channels}")
    # ci = af.getChannelInfo(SoapySDR.SOAPY_SDR_RX, 0)
    # print(ci)

    rx_stream = af.setupStream(SoapySDR.SOAPY_SDR_RX, SoapySDR.SOAPY_SDR_CF32, [0])
    print(rx_stream)
    num_samps_total = 10000
    af.activateStream(rx_stream, 0, 0, num_samps_total)

    buff = np.array([0] * 1024, np.complex64)
    for _idx in range(5):
        sr = af.readStream(rx_stream, [buff], buff.size)
        print(f"sr:  {sr}")

    af.deactivateStream(rx_stream)
    af.closeStream(rx_stream)


if __name__ == "__main__":
    main()
