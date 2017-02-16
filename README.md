# LPS node firmware  [![Build Status](https://api.travis-ci.org/bitcraze/lps-node-firmware.svg)](https://travis-ci.org/bitcraze/lps-node-firmware)

This project contains the source code for the Local positioning System node firmware. 

# Useful notes for the UWB sniffer client for reading CIR
The client script for plotting the firmware and some additional information is
located in /tools/client/*.

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

## Dependencies

You'll need to use either the Crazyflie VM, install some ARM toolchain or the Bitcraze docker builder image.

Frameworks for unit testing are pulled in as git submodules. To get them when cloning

```bash
git clone --recursive https://github.com/bitcraze/lps-node-firmware.git
```
        
or if you already have a cloned repo and want the submodules
 
```bash
git submodule init        
git submodule update        
```

### OS X
```bash
brew tap PX4/homebrew-px4
brew install gcc-arm-none-eabi
```

### Debian/Ubuntu

> `TODO: Please share!`

### Arch Linux

```bash
sudo pacman -S community/arm-none-eabi-gcc community/arm-none-eabi-gdb community/arm-none-eabi-newlib
```

### Windows

> `TODO: Please share!`

## Compiling

`make`

or 

`docker run --rm -v ${PWD}:/module bitcraze/builder ./tools/build/compile`

or 

`tools/do compile`

## Folder description:

> `TODO: Please share!`

# Make targets:
```
all        : Shortcut for build
flash      : Flash throgh jtag
openocd    : Launch OpenOCD
dfu        : Flash throgh DFU 
```

## Unit testing

We use [Unity](https://github.com/ThrowTheSwitch/unity) and [cmock](https://github.com/ThrowTheSwitch/CMock) for unit testing.

To run all tests 

`./tools/do test`

