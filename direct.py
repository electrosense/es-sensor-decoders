#!/usr/bin/python2 -u

import signal
import sys
import thread

import numpy as np
from rtlsdr import RtlSdr

from decoders.am import AM
from decoders.fm import FM

if __name__ == '__main__':

    def fill_cb (samples, _):
        global decoder, Gain
        decoder.feed(np.array(samples).astype("complex64"), Gain)

    if len(sys.argv) != 4 or sys.argv[1] not in ['am', 'fm']:
        print >>sys.stderr, "Usage: %s am|fm <freqency> <gain>" % sys.argv[0]
        exit(1)

    sdr = RtlSdr()

    Fc = int(float(sys.argv[2]))  # Capture center frequency
    Fs = int(2.4e6)  # Sample rate
    Gain = int(float(sys.argv[3]))

    # configure device
    sdr.sample_rate = Fs  # Hz
    sdr.center_freq = Fc  # Hz
    sdr.gain = 'auto'

    if sys.argv[1] == 'am':
        decoder = AM(Fs)
    elif sys.argv[1] == 'fm':
        decoder = FM(Fs)

    thread.start_new_thread(sdr.read_samples_async, (fill_cb, 256*1024))
    thread.start_new_thread(decoder.run(), ())


    def signal_handler(sig, frame):
        global sdr, decoder
        print >>sys.stderr, 'You pressed Ctrl+C!'
        sdr.cancel_read_async()
        decoder.stop()
        sdr.close()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)
    signal.pause()

