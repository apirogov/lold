//lold_firmware
//Copyright (C) 2013 Anton Pirogov
//Licensed under the MIT License
//Sketch that receives frames to show over USB
//to use the lolshield as little screen (e.g. with lold)
//
//command syntax:
//a command ends with \n
//a frame is 9 comma-separated short values representing the bits of a row (1-16383)
//will be rendered directly and instantly in passthrough mode
//special commands:
//16384,1,DELAY,0,0,0,0,0,0 -> starts recording animation which is to be played with given delay
//16384,0,0,0,0,0,0,0,0 -> stop recording and play animation / toggle passthrough/play mode
//16385,DELAY,BLOCK_FLAG,Some Ascii String -> scroll text (blocks lolshield for that duration!)
//priority order: blocking text message > stored ani > passthrough > nonblock text message

#include <Charliplexing.h>
#include <Myfont.h>

#include <EEPROM.h>

uint16_t frame[9]; //currently read command

//buffer for text scroll command
int len=0;
char buf[128];

byte state = 0; //0=pass through, 1=record, 2=play stored
byte curFrame = 0; //current frame of recorded animation

//params of recorded animation
short frmDelay = 0;
byte numFrames = 0;


//write a frame
void writeToROM(byte num) {
  int adr = 9*2 * num + 3;
  for (int i=0; i<9; i++) {
    EEPROM.write(adr++, (byte)(frame[i] >> 8));
    EEPROM.write(adr++, (byte)frame[i]);
  }
}

//read a frame
void readFromROM(byte num) {
  int adr = 9*2 * num + 3;
  for (int i=0; i<9; i++) {
    frame[i] = ((uint16_t)EEPROM.read(adr++))<<8;
    frame[i] |= (uint16_t)EEPROM.read(adr++);
  }
}

void setup() {
  // Init LOL Shield, framebuffered
  LedSign::Init(1);
  
  // restore stored animation data
  numFrames = EEPROM.read(0);
  frmDelay  = ((uint16_t)EEPROM.read(1))<<8;
  frmDelay |= (uint16_t)EEPROM.read(2);
  
  // initialize serial
  Serial.begin(115200);
  Serial.println("Lolshield initialized.");
}

//return 1 on normal command, 2 on text scroll command, 0 on failure
int readCmd() {
  if (!Serial.available())
    return 0;

  frame[0] = Serial.parseInt();
  if (frame[0] < 16385) {
    byte curr = 1;
    while (curr<9) {
      if (!Serial.available())
        return 0;
      frame[curr] = Serial.parseInt();
      curr++;
    }
    Serial.find("\n");
    return 1;
  } else { //render ascii text command
    frame[1] = Serial.parseInt(); //delay
    frame[2] = Serial.parseInt(); //blocking?
    Serial.read(); //skip comma
    len = Serial.readBytesUntil('\n', buf, 128);
    if (len==0)
      return 0;
    buf[len] = '\0';
    Serial.find("\n");
    return 2;
  }
}

//read frames, show/record/play frames
void loop() {
  int ret = readCmd();
  
  if (ret==1) {  //has read complete line
    //Serial.print("."); //ack
    if (frame[0]>16383) { //special command
      if (frame[0]==16384) { //record command
        if (frame[1]) { //start recording and play
          Serial.println("Start record");
          frmDelay = frame[2];
          EEPROM.write(1, (byte)(frmDelay >> 8));
          EEPROM.write(2, (byte)frmDelay);
          
          numFrames = 0;
          curFrame = 0;
          state = 1;
          return;
        } else { //stop rec/ toggle play/passthrough
          if (state == 1) {
            EEPROM.write(0, numFrames);
            state = 2;
            Serial.println("Stop rec, play");  
          } else {
            Serial.println("Toggle play/passthrough");  
            state = state==2 ? state = 0 : state = 2;
          } 
          curFrame = 0;
        }
      }
    }
    
    if (state == 0) { //full frame, pass through
      displayFrame(frame);
    } else if (state == 1) { //full frame, record
      displayFrame(frame);
      writeToROM(curFrame++);
      
      numFrames = curFrame;
      if (numFrames==56) { //got all frames -> play
        Serial.println("Max reached. Stop rec, play");
        EEPROM.write(0, numFrames);
        state = 2;
        curFrame = 0;
      }
    }
  } else if (ret==2) {
    Serial.println("Render text");
    scrollText(len, buf);
  } else if (ret==0) {
    if (state==2) { //no input, play state
      readFromROM(curFrame++);
      displayFrame(frame);
      if (curFrame == numFrames) {
        curFrame = 0;
      }
      delay(frmDelay);
    }
  }
}

//output a frame from an int[9]
//each int represents a line, each bit of the int represents one of the 14 leds
void displayFrame(uint16_t *frame)
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
  LedSign::Flip(false); //non-blocking flip
}

//modified and fixed Myfont::Banner for double buffering and using custom delay
void scrollText(int len, char* text) {
    int xoff=14;/* setmx offset to the right end of the screen*/
    for(int i=0; i<len*6 +52; i++){ /*scrolling loop*/
        LedSign::Clear(); /*empty the screen */
        for(int j=0; j<len; j++){ /*loop over all of the chars in the text*/
            Myfont::Draw(xoff + j*6, (unsigned char)text[j]); /* call the draw font function*/
        }
        xoff--; /* decrement x offset*/
        LedSign::Flip(false);
        delay(frame[1]); /*scrolling speed increments if delay goes down*/

        if (!frame[2] && Serial.available()) //abort animation if nonblocking + input waiting
          return;
    }
}
