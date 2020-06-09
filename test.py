#!/usr/bin/python2 -u

import signal
import sys
import thread
import socket
import struct
import os

import numpy as np
from rtlsdr import RtlSdr

socket_address = '/tmp/socket'
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

sdr = RtlSdr()

Fc = int(float(sys.argv[1]))  # Capture center frequency
Fs = int(float(sys.argv[2]))  # Sample rate
gain = int(float(sys.argv[3]))

# configure device
sdr.sample_rate = Fs      # Hz
sdr.center_freq = Fc      # Hz
sdr.gain = 'auto'

running = True

if __name__ == '__main__':

    def fill_cb(bytes, _):
        global cli, gain
        hdr = struct.pack("<QLL", Fc, gain, len(bytes))
        try:
            cli.send(hdr)
            cli.send(bytes)
        except socket.error, msg:
            print >>sys.stderr, msg
            sys.exit(1)

    try:
        os.unlink(socket_address)
    except OSError:
        if os.path.exists(socket_address):
            raise

    try:
        sock.bind(socket_address)
        sock.listen(1)
        cli, _ = sock.accept()
        print "Connected"
    except socket.error, msg:
        print >>sys.stderr, msg
        sys.exit(1)

    thread.start_new_thread(sdr.read_bytes_async, (fill_cb, 256*1024))

    def signal_handler(sig, frame):
        global sdr, running, sock, cli
        print >>sys.stderr, 'You pressed Ctrl+C!'
        sdr.cancel_read_async()
        sdr.close()
        cli.close()
        sock.close()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)
    signal.pause()

