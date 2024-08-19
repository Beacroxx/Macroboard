#include <Arduino.h>
#include <Bounce2.h>
#include <Encoder.h>
#include <usb_rawhid.h>

// USB Vendor and Product IDs
#define VENDOR_ID               0x16C0
#define PRODUCT_ID              0x0480
#define RAWHID_USAGE_PAGE       0xFFAB
#define RAWHID_USAGE            0x0200

// USB Raw HID packet sizes and intervals
#define RAWHID_TX_SIZE          64
#define RAWHID_TX_INTERVAL      10
#define RAWHID_RX_SIZE          64
#define RAWHID_RX_INTERVAL      8

// Pins for buttons and encoders
const uint8_t button[15] = {21, 38, 20, 39, 17, 40, 16, 41, 15, 14, 33, 34, 35, 36, 37};

// Bounce instances for debouncing buttons
Bounce buttons[15];

// Pins for encoder A and B channels
const uint8_t encoderA[5] = {1, 3, 5, 7, 9};
const uint8_t encoderB[5] = {0, 2, 4, 6, 8};

// Encoder instances
Encoder encoders[5]{
    Encoder(encoderA[0], encoderB[0]), Encoder(encoderA[1], encoderB[1]),
    Encoder(encoderA[2], encoderB[2]), Encoder(encoderA[3], encoderB[3]),
    Encoder(encoderA[4], encoderB[4])};

// Previous and current values for the two sliders
uint16_t prevSlider[2] = {0, 0};
uint16_t slider[2] = {0, 0};

// Buffer for sending data over USB Raw HID
uint8_t buffer[64];
uint8_t idx = 0;

int main() {
  // Set up the analog to digital conversion
  analogReadResolution(12);
  analogReadAveraging(64);

  // Initialize the button debouncing
  for (int i = 0; i < 15; i++) {
    buttons[i].attach(button[i], INPUT_PULLUP);
    buttons[i].interval(5);
  }

  while (true) {
    // Store the previous slider values
    prevSlider[0] = slider[0];
    prevSlider[1] = slider[1];

    // Read the current slider values
    slider[0] = analogRead(22);
    slider[1] = analogRead(23);

    // Debounce the slider values
    if (abs(slider[0] - prevSlider[0]) < 5) {
      slider[0] = prevSlider[0];
    }
    if (abs(slider[1] - prevSlider[1]) < 5) {
      slider[1] = prevSlider[1];
    }

    // If the slider values have changed, add them to the buffer
    if (prevSlider[0] != slider[0] || prevSlider[1] != slider[1]) {
      buffer[idx++] = 0x01;
      buffer[idx++] = (uint8_t)(slider[0] >> 8);
      buffer[idx++] = (uint8_t)(slider[0] & 0xFF);
      buffer[idx++] = 0x02;
      buffer[idx++] = (uint8_t)(slider[1] >> 8);
      buffer[idx++] = (uint8_t)(slider[1] & 0xFF);
    }

    // Iterate over the buttons, add their values to the buffer if they have changed
    for (int i = 0; i < 10; i++) {
      buttons[i].update();
      if (buttons[i].fell()) {
        buffer[idx++] = 0x03;
        buffer[idx++] = i;
      }
    }

    // Iterate over the encoders, add their values to the buffer if they have changed
    for (int i = 0; i < 5; i++) {
      long newPos = encoders[i].read();
      buttons[i+10].update();
      if (abs(newPos / 4) > 0) {
        buffer[idx++] = 0x04;
        buffer[idx++] = newPos > 0;
        buffer[idx++] = i;
        buffer[idx++] = !buttons[i+10].read();

        // Reset the encoder value
        encoders[i].write(0);
      }
    }

    // If the buffer is not empty, send it over USB Raw HID
    if (idx > 0) {
      usb_rawhid_send(buffer, 100);
      idx = 0;

      // Clear the buffer
      memset(buffer, 0, sizeof(buffer));
    }

    // Wait 10 milliseconds before looping again
    delay(10);
  }
}
