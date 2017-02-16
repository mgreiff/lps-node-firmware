/*
 *    ||          ____  _ __
 * +------+      / __ )(_) /_______________ _____  ___
 * | 0xBC |     / __  / / __/ ___/ ___/ __ `/_  / / _ \
 * +------+    / /_/ / / /_/ /__/ /  / /_/ / / /_/  __/
 *  ||  ||    /_____/_/\__/\___/_/   \__,_/ /___/\___/
 *
 * LPS node firmware.
 *
 * Copyright 2016, Bitcraze AB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Foobar is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 */
/* uwb_sniffer.c: Uwb sniffer implementation */

#include "uwb.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "cfg.h"
#include "led.h"

#include "libdw1000.h"

#include "dwOps.h"
#include "mac.h"
#define LENGTH_DATA 33
#define LE_LARGE_WINDOW_LENGTH 20
#define LE_SMALL_WINDOW_LENGTH 6
#define SEND_FULL_CIR true // Note that this should be set false if the
                           // sniffer should locate and send information
                           // regarding the relevant peaks rather than the full CIR

// Variables used to handle CIR data
static uint8_t data[LENGTH_DATA];
static int16_t data_real[LE_LARGE_WINDOW_LENGTH] = {0};
static int16_t data_imag[LE_LARGE_WINDOW_LENGTH] = {0};
static uint8_t LE_count = 0;
static int16_t LE_means[4] = {0,0,0,0};
static int16_t LE_magnitude = 2;
static int16_t LE_threshold = 100;

static uint32_t twrAnchorOnEvent(dwDevice_t *dev, uwbEvent_t event)
{
  static dwTime_t arrival;
  static packet_t rxPacket;

  if (event == eventPacketReceived) {
    int dataLength = dwGetDataLength(dev);
    dwGetReceiveTimestamp(dev, &arrival);
    dwGetData(dev, (uint8_t*)&rxPacket, dataLength);

    printf("From %02x to %02x @%02x%08x: ", rxPacket.sourceAddress[0],
                                          rxPacket.destAddress[0],
                                          (unsigned int) arrival.high8,
                                          (unsigned int) arrival.low32);
    for (int i=0; i<(dataLength - MAC802154_HEADER_LENGTH); i++) {
      printf("%02x", rxPacket.payload[i]);
    }

    // Sed CIR or just compute all relevant peaks in the imaginary/real
    // parts by the leading edge algorithm and send the time delay and 
    //
    // THe accumulator memory is enabled by setting two bits high
    // in the PMIC register, done once and only once in twrAnchorInit().
    // enable one of the if statements below for different diagnostics.
    //
    // To read and acces the CIR we need to set bytes in the PMIC register
    // a total of four bits need to be set, where something CIR-like can be read
    // if setting FACE and ACME. However, when forcing the RX clock enable, as
    // required in the data sheet, we can't read enything. Have I interpreted this correctly?
    // reg:36, bit 3:2 = 01 to force RX clock enable
    // reg:36, bit 6   = 1  to force FACE
    // reg:36, bit 15  = 1  to force AMCE
    //
    // see 181-182  http://thetoolchain.com/mirror/dw1000/dw1000_user_manual_v2.05.pdf
    //
    // reg:36:0 = 1 and reg:36:003 = 0 to Force RX clock enable
    // In the accumulator memory, we have
    // reg:25:000   CIR[0] real part low       int8
    // reg:25:001   CIR[0] real part high      int8
    // reg:25:002   CIR[0] imag part low       int8
    // reg:25:003   CIR[0] imag part high      int8
    // reg:25:004   CIR[1] real part low       int8
    // reg:25:005   CIR[1] real part high      int8
    // .            .                          .
    // .            .                          .
    // .            .                          .
    // reg:25:FDC   CIR[1015] real part low    int8
    // reg:25:FDD   CIR[1015] real part high   int8
    // reg:25:FDE   CIR[1015] imag part low    int8
    // reg:25:FDF   CIR[1015] imag part high   int8

    // To form the 16 bit equivalent CIR we therefore use
    // ((uint16_t)(CIR[0].high) << 8) | CIR[0].low

    if (SEND_FULL_CIR == true){
      // Sends the entire CIR
      printf(" CIR: ");
      for (int address = 0; address < 4064; address+=32) {
        dwSpiRead(dev, 0x25, address, data, 33);
        for (int i=1; i<33; i++) {
          printf("%02x", data[i]);
        }
      }
    } else {
      // Computes the relevant peaks by the LE algorithm (currently unsupported in decode_CIR.py)
      printf(" LE: ");
      for (int address = 0; address < 4064; address+=32) {
        dwSpiRead(dev, 0x25, address, data, 33);
        for (int i=1; i<33; i=i+4) {
          // Mean removal
          LE_means[0] -= data_real[LE_count % LE_LARGE_WINDOW_LENGTH] / LE_LARGE_WINDOW_LENGTH;
          LE_means[1] -= data_imag[LE_count % LE_LARGE_WINDOW_LENGTH] / LE_LARGE_WINDOW_LENGTH;
          LE_means[2] -= data_real[(LE_count - LE_SMALL_WINDOW_LENGTH) % LE_LARGE_WINDOW_LENGTH] / LE_SMALL_WINDOW_LENGTH;
          LE_means[3] -= data_imag[(LE_count - LE_SMALL_WINDOW_LENGTH) % LE_LARGE_WINDOW_LENGTH] / LE_SMALL_WINDOW_LENGTH;

          // Conversion
          data_real[LE_count % LE_LARGE_WINDOW_LENGTH] = (data[i+1] << 8 ) | (data[i] & 0xff);
          data_imag[LE_count % LE_LARGE_WINDOW_LENGTH] = (data[i+3] << 8 ) | (data[i+2] & 0xff);
          LE_count++;
          
          // Mean addition
          LE_means[0] += data_real[LE_count % LE_LARGE_WINDOW_LENGTH] / LE_LARGE_WINDOW_LENGTH;
          LE_means[1] += data_imag[LE_count % LE_LARGE_WINDOW_LENGTH] / LE_LARGE_WINDOW_LENGTH;
          LE_means[2] += data_real[LE_count % LE_LARGE_WINDOW_LENGTH] / LE_SMALL_WINDOW_LENGTH;
          LE_means[3] += data_imag[LE_count % LE_LARGE_WINDOW_LENGTH] / LE_SMALL_WINDOW_LENGTH;
          
          // LE check
          if ((( LE_means[0] < LE_magnitude * LE_means[2] ) && ( LE_means[0] > LE_threshold )) ||
              (( LE_means[1] < LE_magnitude * LE_means[3] ) && ( LE_means[1] > LE_threshold )) ){
            printf("%02x%02x", data_real[LE_count % LE_LARGE_WINDOW_LENGTH], data_imag[LE_count % LE_LARGE_WINDOW_LENGTH]);
          }
        }
      }
      for (int i = 0; i < LE_LARGE_WINDOW_LENGTH; i++ ){
        data_real[i] = 0;
        data_imag[i] = 0;
      }
      LE_count = 0;
    }
    
    printf("\r\n");
    dwNewReceive(dev);
    dwSetDefaults(dev);
    dwStartReceive(dev);
  } else {
    dwNewReceive(dev);
    dwSetDefaults(dev);
    dwStartReceive(dev);
  }

  return MAX_TIMEOUT;
}

static void twrAnchorInit(uwbConfig_t * newconfig, dwDevice_t *dev)
{
  ledBlink(ledMode, false);
  dwSpiWrite32(dev, PMSC, PMSC_CTRL0_SUB, dwSpiRead32(dev, PMSC, PMSC_CTRL0_SUB) | 0x00008040);
}

uwbAlgorithm_t uwbSnifferAlgorithm = {
  .init = twrAnchorInit,
  .onEvent = twrAnchorOnEvent,
};
