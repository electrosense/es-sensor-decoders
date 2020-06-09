#!/usr/bin/python2 -u

import signal
import socket
import struct
import sys
import thread

import numpy as np

from demodulators.am import AM
from demodulators.fm import FM

socket_address = '/tmp/socket'
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
running = True

if __name__ == '__main__':
    if len(sys.argv) != 3 or sys.argv[1] not in ['am', 'fm']:
        print >>sys.stderr, "Usage: %s am|fm <sample_frequency> <gain>" % sys.argv[0]
        exit(1)

    Fs = int(float(sys.argv[2]))  # Sample rate

    if sys.argv[1] == 'am':
        decoder = AM(Fs)
    elif sys.argv[1] == 'fm':
        decoder = FM(Fs)

    if decoder.Fs != Fs:
        print >>sys.stderr, "Wrong sample frequency, expected %d" % decoder.Fs
        sys.exit(1)

    def read_samples():
        global decoder, running
        while running:
            try:
                hdr = sock.recv(8 + 4 + 4, socket.MSG_WAITALL)
                center_frequency, gain, n_samples = struct.unpack("<QLL", hdr)
                raw = sock.recv(n_samples, socket.MSG_WAITALL)
                data = struct.unpack("<%dB" % len(raw), raw)
                data = np.array(data).astype(np.float64).view(np.complex128)
                data /= 127.5
                data -= (1 + 1j)
                decoder.feed(data, gain)
            except socket.error as msg:
                print >> sys.stderr, str(msg)
                sys.exit(1)
        sock.close()


    try:
        sock.connect(socket_address)
    except socket.error as msg:
        print >> sys.stderr, str(msg)
        sys.exit(1)

    thread.start_new_thread(read_samples, ())
    thread.start_new_thread(decoder.run, ())


    def signal_handler(sig, frame):
        global decoder, running
        print >>sys.stderr, 'You pressed Ctrl+C!'
        decoder.stop()
        running = False
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)
    signal.pause()

