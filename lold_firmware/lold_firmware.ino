//lold_firmware
//Simple Sketch that receives frames to show over USB
//to use lolshield as little screen (e.g. with lold)
//Copyright (C) 2013 Anton Pirogov
//Licensed under the MIT License

#include <Charliplexing.h>

int frame[9];

void setup() {
  // Init LOL Shield
  LedSign::Init();
  
  // initialize serial:
  Serial.begin(9600);
}

//read frames and output if received a full frame (9 ints)
void loop() {
  int curr = 0;
  
  while (Serial.available() && curr<9) {
    frame[curr] = Serial.parseInt();
    curr++;
    delay(5); //give next char some time
  }
  
  if (curr >= 9) {
    displayFrame(frame);
  }
}

//output a frame from an int[9]
//each int represents a line, each bit of the int represents one of the 14 leds
void displayFrame(int *frame)
{
  for(byte line = 0; line < 9; line++) {    
    for (byte led=0; led<14; ++led) {
      if (frame[line] & (1<<led)) {
        LedSign::Set(led, line, 1);
      } else {
        LedSign::Set(led, line, 0);
      }
    }
  }
}
