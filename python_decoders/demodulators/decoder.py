import sys
import time

import numpy as np
import scipy.signal as sig


import ringbuf

class Decoder(object):
    def _setup(self):
        pass

    def __init__(self, sample_frequency):
        self.running = True
        self.Fs_in = sample_frequency
        self.Fs = sample_frequency
        buf_size = 10 * 256 * 1024
        self.buf = ringbuf.RingBuffer(capacity=buf_size)
        self._setup()
        if self.Fs != self.Fs_in:
            print >>sys.stderr, "Resampling from %d to %d" % (self.Fs_in, self.Fs)
            self.lpf1 = sig.remez(64, [0, self.Fs, self.Fs + (self.Fs_in / 2 - self.Fs) / 4, self.Fs_in / 2], [1, 0],
                                  Hz=self.Fs_in)
            self.zi1 = sig.lfilter_zi(self.lpf1, 1.0)
            self.dec_rate1 = int(self.Fs_in / self.Fs)

    def feed(self, data, gain):
        self.buf.extend(data)
        self.gain = gain

    def stop(self):
        self.running = False

    def _decode(self, data):
        return np.empty(0)

    def run(self):
        while self.running:
            if self.buf.is_full:
                print >> sys.stderr, "Processing too slow! Buffer is full :-("

            if len(self.buf) > 0:
                data = self.buf.popn(len(self.buf))
                if self.Fs != self.Fs_in:
                    data,self.zi1 = sig.lfilter(self.lpf1, 1.0, data, zi=self.zi1)
                    data = data[0::self.dec_rate1]

                audio = self._decode(data)

                sys.stdout.write(audio.astype("int16").tobytes())
            else:
                time.sleep(0.01)
