
# es-sensor-decoders

Decoders used in electrosense sensors (http://electrosense.org). 


## Compile

```
$ git clone https://github.com/electrosense/es-sensor-decoders
$ make 
```

If you want to create debian package just type:

```
$ dpkg-buildpackage -us -uc
```

## Decoders

Currently the following decoders have been added:

 * LTE-Cell-Scanner: LTE SDR cell scanner optimized to work with very low performance RF front ends
 * acarsdec: Acarsdec is a multi-channels acars decoder with built-in rtl_sdr
 * dump1090: Simple Mode S decoder for RTLSDR devices 
 * rtl-ais_es: A simple AIS tuner and generic dual-frequency FM demodulator 
 * AM/FM decoder: AM/FM radio decoder.
