/* Sega Activator device reader
 * by Matheus Fraguas
 * https://github.com/sonik-br/sega_activator_to_usb
 * 
 * Based on docs from:
 * https://bitbucket.org/eke/genesis-plus-gx/issues/157/activator-support
 * https://gendev.spritesmind.net/forum/viewtopic.php?f=23&t=2949
*/

#include <DigitalIO.h>
#include "usb_gamepad.h"

// pins
#define PIN_TH 4
#define PIN_TR 13
#define PIN_TL 5
#define PIN_D0 9
#define PIN_D1 8
#define PIN_D2 7
#define PIN_D3 6

DigitalPin<PIN_D0> md_D0;
DigitalPin<PIN_D1> md_D1;
DigitalPin<PIN_D2> md_D2;
DigitalPin<PIN_D3> md_D3;
DigitalPin<PIN_TL> md_TL;
DigitalPin<PIN_TR> md_TR;
DigitalPin<PIN_TH> md_TH;

#define INIT_STEPS_MAX 2

GamePad_ GamePad;

void resetGamepad() {
  GamePad.set_buttons(0);
}

inline uint8_t __attribute__((always_inline)) getData() {
  return (md_TR << 4) | (md_TL << 3) | (md_D3 << 2) | (md_D2 << 1) | (md_D1 << 0);
}

inline bool __attribute__((always_inline)) setD0AndWaitD1(uint8_t d0Value) {
  // set D0
  if (d0Value)
    md_D0.config(INPUT, LOW); // input, float
  else
    md_D0.config(OUTPUT, LOW); // low

  // wait for D1 to follow
  uint32_t startTime = micros();
  while (md_D1 != d0Value) {
    if (micros() - startTime > 50) { // 35us seems ok
      return false; // timed out
    }
  }
  return true; // acknowledged
}

void endProcess() {
  md_D0.config(INPUT, LOW); // input, float
  md_TH.write(HIGH);
  
  // delay before next read
  delay(8); // should be 16ms? seems to work as low as 3
}

void setup() {
  Serial.begin(115200);
  // init input pins with pull-up
  md_D1.config(INPUT, HIGH);
  md_D2.config(INPUT, HIGH);
  md_D3.config(INPUT, HIGH);
  md_TL.config(INPUT, HIGH);
  md_TR.config(INPUT, HIGH);
  
  // init output pins
  md_D0.config(INPUT, LOW); // no pull up, let it float
  md_TH.config(OUTPUT, HIGH); // TH default state is high

  GamePad.begin();

  delay(16);
}

void loop() {
/*
OU IN IN IN IN IN OU pin mode (IN/OUT)
TH TR TL D3 D2 D1 D0 --------------------- 0 x x x x x 0 (write) start acquisition sequence
x  x  x  0  1  0  0 (read) peripheral ID code = 4
0  x  x  x  x  x  1 (write) acquisition cycle comment 1\. x x x x x 1 x (read) wait acquisition ready (D1=D0) x 2U 2L 1U 1L 1 x (read) sensors 1-2
0  x  x  x  x  x  0 (write) acquisition cycle comment 2\. x x x x x 0 x (read) wait acquisition ready (D1=D0) x 4U 4L 3U 3L 0 x (read) sensors 3-4
0  x  x  x  x  x  1 (write) acquisition cycle comment 3\. x x x x x 1 x (read) wait acquisition ready (D1=D0) x 6U 6L 5U 5L 1 x (read) sensors 5-6
0  x  x  x  x  x  0 (write) acquisition cycle comment 4\. x x x x x 0 x (read) wait acquisition ready (D1=D0) x 8U 8L 7U 7L 0 x (read) sensors 7-8
1  x  x  x  x  x  0 (write) end acquisition sequence
*/

  static uint16_t lastDataIn = 0;
  static uint8_t init_step = 0;

  //begin
  
  // try to detect the device and it's ID
  md_TH.write(LOW);
  delayMicroseconds(4); // very short delay. works even without it. at ~20us it will trigger the device into a 3btn controller mode

  if (!setD0AndWaitD1(LOW)) { // timed out
    endProcess();
    init_step = 0; // reset the steps
    return;
  }

  delayMicroseconds(8); // some delay is required here. 4us seems ok
  
  // check the id
  if ((getData() & 0b11) != 0b10) { // a 3btn controller would reply both D2 and D3 as zero
    endProcess();
    init_step = 0; // reset the steps
    return;
  }

//  if (!connected) { // was not connected. is connected now
//    connected = true;
//    init_step = 0; // reset the steps
//    endProcess();
//    return;
//  }



  // get data
  uint16_t dataIn = 0;
  bool dataZeroState = HIGH;

  // sensors loop. each read will get 2 sensors, low and hi for each
  // ask for next nibble via LOW-HIGH cycle
  for (uint8_t i = 0; i < 4; ++i) {

    if (!setD0AndWaitD1(dataZeroState)) { // timed out
      endProcess();
      init_step = 0; // reset the steps
      return;
    }

    delayMicroseconds(8); // some delay. not sure if it's needed

    // data is ready. read it
    uint8_t nibble = getData() >> 1; // discard data from D1.
    dataIn |= (nibble << (4UL * i));

    dataZeroState = !dataZeroState; // will flip D0 pin to ask for next nibble
    delayMicroseconds(50); // delay before next read. needed?
  }

  // data changed? send over usb
  if (dataIn != lastDataIn) {

    // device is initializing?
    // wait until some clean data arrives
    // initialization data looks like
    // 1111111111111111
    // 0101010101010111
    // 0101010101011111
    // 0101010101111111
    // 0101010111111111
    // 0101011111111111
    // 0101111111111111
    // 0111111111111111
    // 1111111111111111
    if (dataIn == 0xFFFF && init_step < INIT_STEPS_MAX)
      ++init_step;
    
    if (init_step == INIT_STEPS_MAX) // initialization done, ready to use
      GamePad.set_buttons(~dataIn); // data in is active low, usb button is active high
  }

  lastDataIn = dataIn;

  //end
  endProcess();
}
