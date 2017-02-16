#!/usr/bin/env python
import serial
import re
import codecs
from collections import namedtuple
import struct
import matplotlib.pyplot as plt
import numpy as np
from math import sqrt, floor
import time

SYNC_PACKET_TYPE = 0x11
RANGE_PACKET_TYPE = 0x12

Packet = namedtuple("Packet", "src dst time data cir")

def decode_line(line):
    match = re.match("From ([0-9a-f]+) to ([0-9a-f]+) @([0-9a-f]+): ([0-9a-f]+) CIR: ([0-9a-f]+)", line)
    if match is not None:
        if len(match.group(5)) % 2 != 0:
            return None
        time = struct.unpack(">Q", b'\0\0\0' +
                             codecs.decode(match.group(3), 'hex'))[0]
        return Packet(src=codecs.decode(match.group(1), 'hex')[0],
                      dst=codecs.decode(match.group(2), 'hex')[0],
                      time=time,
                      data=codecs.decode(match.group(4), 'hex'),
                      cir=codecs.decode(match.group(5), 'hex'))
    else:
        return None

def decode_dwtime(time_bytes):
    return struct.unpack("<Q", time_bytes + b'\0\0\0')[0]

def decode_packet(pk):
    if pk.data[0] == SYNC_PACKET_TYPE:
        mastertime = decode_dwtime(pk.data[1:6])
        return SyncPacket(src=pk.src, time=pk.time, mastertime=mastertime)
    elif pk.data[0] == RANGE_PACKET_TYPE:
        mastertime = decode_dwtime(pk.data[1:6])
        anchortimes = []
        for t in struct.unpack("5s"*6, pk.data[6:]):
            anchortimes += [decode_dwtime(t)]
        return RangePacket(src=pk.src, time=pk.time, mastertime=mastertime,
                           anchortimes=anchortimes)
    else:
        return None

if __name__ == "__main__":
    print("Openning sniffer serial port ...")
    ser = serial.Serial("/dev/ttyACM0", timeout=2)
    print(ser.name)

    current_time = 0

    prev_range = [None]*6
    current_range0 = None
    current_sync = None
    prev_timing = None
    distance = [None]*6  # Distance in tick between the anchor and 0
    distance[0] = 0      # By definition ...
    drift = [1.0]*6
    local_drift = 1.0

    plt.ion()
    fig = plt.figure()
    ax1 = fig.add_subplot(221)
    ax2 = fig.add_subplot(222)
    ax3 = fig.add_subplot(223)
    ax4 = fig.add_subplot(224)
    concatenatedData = []
    xx = np.arange(700,900)
    yy = []
    while True:
        line = ser.readline()
        pk = decode_line(codecs.decode(line, 'utf8'))

        if pk is None:
            continue

        print("CIR Size: {}\n".format(len(pk.cir)))

        if len(pk.cir) != 4064:
            continue

        print float(pk.time)/1e9

        # unpack 2*1026 little endian short (int16_t)
        data = struct.unpack("<" + ("h"*1016*2), pk.cir)

        # Real time plotting of the channel impulse response as it arrives
        if data is None:
            continue
        if len(data) < 3:
            continue
        try:
            ax1.cla()
            ax2.cla()
            ax3.cla()
            ax4.cla()
            ax1.set_title('Real part of CIR')
            ax2.set_title('Imaginary part of CIR')
            ax3.set_title('Amplitude of CIR')
            ax3.set_title('Contour of CIR amplitude')
            datavec = data
            N = int(floor(len(datavec)/2))
            real = np.zeros(N)
            imag = np.zeros(N)
            amp = np.zeros(N);
            for ii in range(N):
                real[ii] = datavec[2*ii]
                imag[ii] = datavec[2*ii+1]
            amp = np.sqrt(real**2 + imag**2)
            ax1.plot(range(N), real)
            #ax2.plot(range(N), imag)
            ax3.plot(range(N), amp)
            concatenatedData = concatenatedData + [amp]
            tempData = np.array(concatenatedData)
            Z = tempData[:,700:900]
            yy = yy + [time.time()]
            X, Y = np.meshgrid(xx, np.array(yy))
            ax4.contourf(X, Y, Z, 40, vmin=0, vmax=500)
            ax2.contourf(X, Y, Z, 40)
            fig.canvas.draw()

        except Exception as e:
            print e
    ser.close()
