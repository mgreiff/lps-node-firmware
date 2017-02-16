Useful notes for the UWB sniffer client for reading CIR

This directory contains (i) n updated version of the firmware in
~/lps-node-firmware and (iii) a script decode_cir.py, which plots the CIR in
real time.

In order to set the anchor and perform basic debugging, use picocom to read the
serial port open with

    ``picocom /dev/ttyACMX``
    
where ``X`` is the latched port (usually 0), and exit using ``C-a-x``.

Note that if something goes wrong, the printed CIR will likely consist of
newline characters ``\n``, converted to the hexadecimal ``0a`` in the bit
conversion. To enable CIR reading in anchor \emph{sniff} mode, we must set the
byte at index 15 to 1 in the PMSC register (0x36) at control register 0 (0x00),
please see the comments in UWB sniffer for more details. This corresponds to
the operation <insert_old_settings>|0x00008000, as 2^15 in int base 32 is 8000
in hexadecimal form (note that the integers are signed!). Changing the chip the
chip settings is done on startup, by

dwSpiWrite32(dev, PMSC, PMSC_CTRL0_SUB, dwSpiRead32(dev, PMSC, PTRL0_SUB) | 0x00008040);

In order to visualize the data, remember to chmod -x the decode_cir.py scipt in
order to make it executable.
