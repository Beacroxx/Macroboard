#include <Arduino.h>
#include <Bounce2.h>
#include <Encoder.h>
#include <MIDI.h>

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

int main() {
  // Set up the analog to digital conversion
  analogReadResolution(12);
  analogReadAveraging(64);

  // Initialize the button debouncing
  for (int i = 0; i < 15; i++) {
    buttons[i].attach(button[i], INPUT_PULLUP);
    buttons[i].interval(5);
  }

  // Initialize MIDI
  usbMIDI.begin();

  while (true) {
    // Store the previous slider values
    prevSlider[0] = slider[0];
    prevSlider[1] = slider[1];

    // Read the current slider values
    slider[0] = map(analogRead(22), 0, 4095, 0, 127);
    slider[1] = map(analogRead(23), 0, 4095, 0, 127);

    // If the slider values have changed, add them to the buffer
    if (prevSlider[0] != slider[0] || prevSlider[1] != slider[1]) {
      usbMIDI.sendControlChange(1, slider[0], 1);
      usbMIDI.sendControlChange(2, slider[1], 1);
    }

    // Iterate over the buttons, add their values to the buffer if they have changed
    for (int i = 0; i < 10; i++) {
      buttons[i].update();
      if (buttons[i].fell()) {
        usbMIDI.sendNoteOn(60 + i, 127, 1);
      } else if (buttons[i].rose()) {
        usbMIDI.sendNoteOff(60 + i, 127, 1);
      }
    }

    // Iterate over the encoders, add their values to the buffer if they have changed
    for (int i = 0; i < 5; i++) {
      long newPos = encoders[i].read();
      buttons[i+10].update();
      if (buttons[i+10].fell()) {
        usbMIDI.sendNoteOn(70 + i, 127, 1);
      } else if (buttons[i+10].rose()) {
        usbMIDI.sendNoteOff(70 + i, 127, 1);
      }
      if (abs(newPos / 4) > 0) {
        usbMIDI.sendControlChange(i + 3, (int)(newPos / 4) > 0 ? 65 : 63, 1);

        // Reset the encoder value
        encoders[i].write(0);
      }
    }

    usbMIDI.send_now();
    // Wait 10 milliseconds before looping again
    delay(10);
  }
}
