import numpy as np
import scipy.signal as sig

from .decoder import Decoder


class AM(Decoder):
    def _setup(self):
        self.Fs = 60e3

        nyq = 0.5 * self.Fs
        normal_cutoff = 9e3 / nyq
        self.b, self.a = sig.butter(5, normal_cutoff, btype='low', analog=False, output='ba')

    def _decode(self, data):
        filtered = sig.lfilter(self.b, self.a, data)
        decoded = np.abs(filtered)
        audio = sig.resample_poly(decoded, 4, 5)
        # Scale audio to adjust volume
        audio *= self.gain  # was 10000 / np.max(np.abs(audio))

        return audio
