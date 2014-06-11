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
//16386,PIXELS -> pixel shade values (1-7), left to right, top to bottom (126 chars) (only passthrough mode!)
//16387,VAL -> set max. brightness (monochrome) / current brightness (shaded) (between 1 and (SHADES-1))
//priority order: rec ani > blocking text message > play ani > passthrough > nonblock text message

#include <Charliplexing.h>
#include <Font.h>
#define LOWERCASE

#include <EEPROM.h>

boolean grayscale = false; //is the current frame in grayscale?
uint16_t frame[9]; //currently read frame
byte maxbright = SHADES-1; //maximum brightness (shaded)/current overall brightness (monochrome)

//buffer for text scroll command
int len=0;
char buf[200];

byte state = 0; //0=pass through, 1=record, 2=play stored animation
byte curFrame = 0; //current recorded/played frame of recorded animation

//params of recorded animation
uint16_t frmDelay = 0; //delay between frames
byte numFrames = 0; //total number of frames saved

//----------------------------

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

//----------------------------

void setup() {
  // Init LOL Shield, framebuffered, with grayscale
  LedSign::Init(GRAYSCALE|DOUBLE_BUFFER);
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
  if (!Serial.available()) //no input
    return 0;

  frame[0] = Serial.parseInt();
  if (frame[0] == 16387) { //set brightness?
    int val = Serial.parseInt(); //get value
    maxbright = min(SHADES-1, max(1, val)); //set sanitized value
    Serial.println("Set brightness");
    Serial.find("\n");
    return 0; //say there was no input
  } if (frame[0] == 16385) { //render ascii text command?
    frame[1] = Serial.parseInt(); //delay
    frame[2] = Serial.parseInt(); //blocking flag
    Serial.read(); //skip comma
    len = Serial.readBytesUntil('\n', buf, 200); //read string
    if (len==0)
      return 0; //something went wrong
    buf[len] = '\0';
    return 2;
  } else if (frame[0] == 16386) { //shaded frame
    grayscale = true;
    Serial.read(); //skip comma
    len = Serial.readBytesUntil('\n', buf, 200); //read data
    if (len==0)
      return 0; //something went wrong
    buf[len] = '\0';
    return 1;
  } else { //normal frame or recording control command
    //it is monochrome -> first value is first row and already read
    byte curr = 1;
    grayscale = false;
    //read frame
    while (curr<9) {
      if (!Serial.available())
        return 0;
      frame[curr] = Serial.parseInt();
      curr++;
    }
    Serial.find("\n"); //skip to end of command
    return 1;
  }
}

//helper function -> stop recording, start playback (for whatever reason)
void finRec() {
  grayscale = false;
  EEPROM.write(0, numFrames);
  state = 2;
  curFrame = 0;
}

//read frames, show/record/play frames
void loop() {
  int ret = readCmd();

  if (ret==2) { //scroll text command
    if (state==1) { //only when not recording!
      Serial.println("Reject render text -> recording!");
      return;
    }
    Serial.println("Render text");
    scrollText(buf, len);
    Serial.println("Render text done");

  } else if (ret==1) {  //has read complete line
    if (frame[0]>16383) { //special command?
      if (frame[0]==16384) { //record command
        if (frame[1]==1) { //start recording
          Serial.println("Start record");
          frmDelay = frame[2];
          //store delay
          EEPROM.write(1, (byte)(frmDelay >> 8));
          EEPROM.write(2, (byte)frmDelay);
          //init recording state
          numFrames = 0;
          curFrame = 0;
          state = 1;
          return;
        } else { //stop recording or toggle play/passthrough
          if (state == 1) { //we were recording?
            finRec();
            Serial.println("Stop rec, play");
          } else {
            Serial.println("Toggle play/passthrough");
            state = state==2 ? 0 : 2;
            if (state == 2) {
              //disable grayscale mode if starting playback
              grayscale = false;
              //also start with first frame
              curFrame = 0;
            }
          }
        }
      }
    }

    if (state == 0) { //full frame and in pass through mode?
      if (!grayscale)
        displayFrame(frame);
      else
        displayGrayscaleFrame(buf, len);
    } else if (state == 1) { //full frame, record mode?
      if (grayscale) { // we can't afford storing shaded frames!
        finRec();
        Serial.println("No grayscale recording. Stop rec, play");
        return;
      }
      //frame was ok - show and store
      displayFrame(frame);
      writeToROM(curFrame++);

      numFrames = curFrame;
      if (numFrames==56) { //got all frames -> play
        finRec();
        Serial.println("Max reached. Stop rec, play");
      }
    }
  } else if (ret==0 && state==2) {
    //no input + play state -> play next frame of stored animation
    readFromROM(curFrame++);
    displayFrame(frame);
    if (curFrame == numFrames) { //loop animation
      curFrame = 0;
    }
    delay(frmDelay);
  }
}

//output a frame from a long[9]
//each value represents a line
//each bit (when monochrome)/4bit(shaded) of the long
//represents brightness of one led in that line
void displayFrame(uint16_t *frame) {
  for(byte line = 0; line < DISPLAY_ROWS; line++) {
    for (byte led=0; led<DISPLAY_COLS; led++) {
      byte val = (frame[line]>>led) & 1;
      LedSign::Set(led, line, val ? maxbright : 0);
    }
  }
  LedSign::Flip(false); //non-blocking flip
}

//render a grayscale frame that uses direct pixel shade encoding
void displayGrayscaleFrame(char *data, int len) {
  int pos = 0;
  for (byte y=0; y<DISPLAY_ROWS; y++)
    for (byte x=0; x<DISPLAY_COLS && pos<len; x++, pos++)
      LedSign::Set(x, y, data[pos]-'0');
  LedSign::Flip(false); //non-blocking flip
}

//scroll text at full brightness
void scrollText(char *text, int len) {
  //pre-calculate text width
  int pixWidth = 0;
  for (int i=0; i<len; i++)
    pixWidth += Font::Draw(text[i],-DISPLAY_COLS,0);

  //render loop
  int x=0;
  for (int j=DISPLAY_COLS-1; j>-DISPLAY_COLS-pixWidth; j--) {

    //abort animation if nonblocking + input waiting or if playback mode
    if (!frame[2] && Serial.available() || state==2) {
      Serial.println("Interrupted render text");
      return;
    }

    //render one frame
    x=j;
    LedSign::Clear();
    for(int i=0; i<len; i++) {
      x += Font::Draw(text[i],x,0, maxbright);
      if (x>=DISPLAY_COLS-1)
        break;
    }
    LedSign::Flip(false);

    delay(frame[1]); //use given frame delay
  }
}

