/*
 * Copyright (c) 2021 Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * 
 * This examples creates a USB Microphone device using the TinyUSB
 * library and captures data from an ANALOG microphone using a sample
 * rate of 16 kHz, to be sent the to PC.
 * 
 * The USB microphone code is based on the TinyUSB audio_test example.
 * 
 * https://github.com/hathach/tinyusb/tree/master/examples/device/audio_test
 */


#include "usb_microphone.h"

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/analog_microphone.h"
#include "tusb.h"

static uint32_t current_sample_rate = 48000;
/*
//--------------------------------------------------------------------+
// AUDIO GET REQUEST CALLBACK
// Invoked when host requests data from an audio entity
//--------------------------------------------------------------------+
bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const * p_request)
{
    audio_control_request_t const * request = (audio_control_request_t const *) p_request;

    // Handle Clock Control requests
    if (request->bControlSelector == AUDIO_CS_CTRL_SAM_FREQ)
    {
        if (request->bRequest == AUDIO_CS_REQ_CUR)
        {
            // Host is asking for the CURRENT sample rate
            audio_control_cur_4_t curf = { (int32_t) current_sample_rate };
            return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &curf, sizeof(curf));
        }
        else if (request->bRequest == AUDIO_CS_REQ_RANGE)
        {
            // Host is asking for the SUPPORTED sample rate range
            // We tell it we only support 48000 Hz (Min: 48000, Max: 48000, Res: 0)
            audio_control_range_4_n_t(1) rangef = {
                .wNumSubRanges = 1,
                .subrange[0] = { 48000, 48000, 0 } 
            };
            return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &rangef, sizeof(rangef));
        }
    }
    else if (request->bControlSelector == AUDIO_CS_CTRL_CLK_VALID && request->bRequest == AUDIO_CS_REQ_CUR)
    {
        // Host is asking if the clock is valid (Android strictly requires this)
        audio_control_cur_1_t cur_valid = { .bCur = 1 };
        return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &cur_valid, sizeof(cur_valid));
    }

    // Return false for unhandled requests to let TinyUSB STALL the endpoint
    // This tells the host we don't support the specific feature it asked for
    return false;
}

//--------------------------------------------------------------------+
// AUDIO SET REQUEST CALLBACK
// Invoked when host sends data to change an audio entity's state
//--------------------------------------------------------------------+
bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const * p_request, uint8_t *buf)
{
    audio_control_request_t const * request = (audio_control_request_t const *) p_request;

    // Handle host instructing us to CHANGE the sample rate
    if (request->bControlSelector == AUDIO_CS_CTRL_SAM_FREQ && request->bRequest == AUDIO_CS_REQ_CUR)
    {
        // Extract the new sample rate from the buffer provided by the host
        audio_control_cur_4_t const * curf = (audio_control_cur_4_t const *) buf;
        current_sample_rate = (uint32_t) curf->bCur;
        
        // Return true to acknowledge we accepted the new sample rate
        return true;
    }

    return false;
}
*/
// configuration
const struct analog_microphone_config config = {
    // GPIO to use for input, must be ADC compatible (GPIO 26 - 28)
    .gpio = 26,

    // bias voltage of microphone in volts
    //.bias_voltage = 1.25,
    .bias_voltage = 0,

    // sample rate in Hz
    .sample_rate = 48000,

    // number of samples to buffer
    .sample_buffer_size = SAMPLE_BUFFER_SIZE,
};

// variables
uint16_t sample_buffer[SAMPLE_BUFFER_SIZE];
volatile int samples_read = 0;

void on_analog_samples_ready()
{
    // callback from library when all the samples in the library
    // internal sample buffer are ready for reading
    samples_read = analog_microphone_read(sample_buffer, SAMPLE_BUFFER_SIZE);
}

// callback functions

void on_usb_microphone_tx_ready()
{
  // Only send data if the ADC has actually read something new
  if (samples_read > 0) {
    // Write ONLY the bytes that were populated, not the whole buffer size
    // Note: samples_read is the number of uint16_t, so multiply by 2 for bytes
    usb_microphone_write(sample_buffer, samples_read * sizeof(uint16_t));
    
    // Reset samples_read to 0 so we don't blast the same old data on the next 1ms tick
    samples_read = 0; 
  }
}


int main(void)
{
  // initialize the analog microphone
  if (analog_microphone_init(&config) < 0) {
      while (1) { tight_loop_contents(); }
  }

  // set callback that is called when all the samples in the library
  // internal sample buffer are ready for reading
  analog_microphone_set_samples_ready_handler(on_analog_samples_ready);

  // start capturing data from the analog microphone
  if (analog_microphone_start() < 0) {
      while (1) { tight_loop_contents();  }
  }

  // initialize the USB microphone interface
  usb_microphone_init();
  usb_microphone_set_tx_ready_handler(on_usb_microphone_tx_ready);

  while (1) {
    // run the USB microphone task continuously
    usb_microphone_task();
  }

  return 0;
}

