import numpy as np
import scipy.signal as sig

from .decoder import Decoder


class FM(Decoder):
    def _setup(self):
        # An FM broadcast signal has  a bandwidth of 200 kHz
        self.Fs = 240000

        #self.bl, self.al = sig.butter(8, float(self.f_bw)/float(self.Fs), 'low')
        #self.zi1 = sig.lfilter_zi(self.bl, self.al)

        # final resampler
        self.f_end = 48000
        self.lpf = sig.remez(64, [0, self.f_end, self.f_end + (self.Fs / 2 - self.f_end) / 4, self.Fs / 2], [1, 0], Hz=self.Fs)
        self.zi = sig.lfilter_zi(self.lpf, 1.0)

        self.dec_rate = int(self.Fs / self.f_end)

        d = self.Fs * 75e-6  # Calculate the # of samples to hit the -3dB point

        x = np.exp(-1 / d)  # Calculate the decay between each sample
        self.b = [1 - x]  # Create the filter coefficients
        self.a = [1, -x]

    def _decode(self, data):
        decoded = np.angle(data[1:] * np.conj(data[:-1]))
        deemph = sig.lfilter(self.b, self.a, decoded)
        audio, self.zi = sig.lfilter(self.lpf, 1.0, deemph, zi=self.zi)
        audio = audio[0::self.dec_rate]
        # Scale audio to adjust volume
        audio *= self.gain  # was 10000 / np.max(np.abs(audio))

        return audio
