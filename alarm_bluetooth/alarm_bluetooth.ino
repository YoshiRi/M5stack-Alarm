/*
 An example digital clock using a TFT LCD screen to show the time.
 Demonstrates use of the font printing routines. (Time updates but date does not.)

 It uses the time of compile/upload to set the time
 For a more accurate clock, it would be better to use the RTClib library.
 But this is just a demo...

 Make sure all the display driver and pin comnenctions are correct by
 editting the User_Setup.h file in the TFT_eSPI library folder.

 #########################################################################
 ###### DON'T FORGET TO UPDATE THE User_Setup.h FILE IN THE LIBRARY ######
 #########################################################################

 Based on clock sketch by Gilchrist 6/2/2014 1.0

A few colour codes:

code	color
0x0000	Black
0xFFFF	White
0xBDF7	Light Gray
0x7BEF	Dark Gray
0xF800	Red
0xFFE0	Yellow
0xFBE0	Orange
0x79E0	Brown
0x7E0	Green
0x7FF	Cyan
0x1F	Blue
0xF81F	Pink

 */

#include <M5Stack.h>
#include "BluetoothSerial.h"

#define TFT_GREY 0x5AEB

uint32_t targetTime = 0;                    // for next 1 second timeout

static uint8_t conv2d(const char* p); // Forward declaration needed for IDE 1.6.x
uint16_t getColor(uint8_t red, uint8_t green, uint8_t blue);

// __TIME__ may look like: 12h05m14s
uint8_t hh = conv2d(__TIME__), mm = conv2d(__TIME__ + 3), ss = conv2d(__TIME__ + 6); // Get H, M, S from compile time
uint8_t hh_ = hh, mm_ = mm;
bool timerflag = 0;
bool alarmON = 0;

byte omm = 99, oss = 99, omm_ = 99;
byte xcolon = 0, xsecs = 0;
unsigned int colour = 0;
int8_t flag = 0;

// serial DMX output
uint8_t DMXout[512];

//--- functions ----//
bool timer_clock_diff();
void update_clock();
void show_clock();
void button_action();
void ibutton_action();
void show_timer();
int draw_timerchar(const uint8_t, const int , const int, const int, const int);
void reviceClock();
void sendRGB(uint8_t red, uint8_t green, uint8_t blue);
void sendserial(uint8_t data);
// -----------------//



//----- music ---------//
#define NOTE_B1 262 
#define NOTE_Cdi 277 
#define NOTE_D1 294 
#define NOTE_Dd1 311 
#define NOTE_E1 330 
#define NOTE_F1 349 
#define NOTE_Fd1 370 
#define NOTE_G1 392 
#define NOTE_Gd1 415 
#define NOTE_A1 440 
#define NOTE_Ad1 466 
#define NOTE_H1 494
int music[] = { NOTE_E1, NOTE_E1, NOTE_E1, NOTE_E1, NOTE_E1, NOTE_E1, NOTE_E1, NOTE_G1, NOTE_Cdi, NOTE_D1, NOTE_E1, NOTE_F1, NOTE_F1, NOTE_F1, NOTE_F1, NOTE_F1, NOTE_E1, NOTE_E1, NOTE_E1, NOTE_D1, NOTE_D1, NOTE_E1, NOTE_D1, NOTE_G1};
// bluetooth
BluetoothSerial bts;


//-------- main start ------------------//

void setup(void) {
    // init lcd, serial, not init sd card
    M5.begin(true, false, true);
    // M5.Lcd.setRotation(1);
    M5.Lcd.fillScreen(TFT_BLACK);

    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);

    targetTime = millis() + 1000;
    flag = 0;

    // bluetooth serial setup
    Serial.begin(9600);
    bts.begin("M5Stack");//PC側で確認するときの名前
}

// main function
void loop() {
    uint32_t now = millis();
  if (targetTime < now) {
    // Set next update for 1 second later
    targetTime = now + 1000*60;

    update_clock();
    //show_clock(); //showing function in the timer_clock_diff()
    alarmON = timer_clock_diff();
    if(alarmON && timerflag){
        for(int i=0;i<22;i++){
            M5.Speaker.tone(music[i],500);
        }
        //M5.Speaker.mute();
    }
  }


   if (M5.BtnA.wasReleased()) {
    flag--;
    if(flag<0) flag=4;
  } else if (M5.BtnB.wasReleased()) {
    flag++;
    if(flag>4) flag=0;
  } else if (M5.BtnC.wasReleased()) {
    button_action(); // push to increase timer
    alarmON = timer_clock_diff();
  } else if (M5.BtnC.wasReleasefor(300)) { 
    ibutton_action();// long hold to decrease timer
    alarmON = timer_clock_diff();      
  }

    show_timer();
    M5.update();

}

//------------------------------------//

// Function to extract numbers from compile time string
static uint8_t conv2d(const char* p) {
  uint8_t v = 0;
  if ('0' <= *p && *p <= '9')
    v = *p - '0'; //convert number string to uint number value
  return 10 * v + *++p - '0';
}

bool timer_clock_diff(){
    int diff;
    diff = ((int)hh_*60+(int)mm_)-((int)hh*60+(int)mm);

    int diff_margin = 60; // start from one hour
    // lighting( changing background color )
    if(timerflag && diff < diff_margin && diff > -diff_margin){
        float diffrate = (float)abs(diff)/(float)diff_margin;
        uint8_t red = 255-255.0*diffrate;
        uint8_t green = 255-255.0*diffrate*diffrate*diffrate*diffrate;
        uint8_t blue = 255-125.0*diffrate*diffrate;
        M5.Lcd.fillScreen(getColor(red,green,blue));
        sendRGB(red,green,blue);
    }else{
        M5.Lcd.fillScreen(getColor(0,0,0));
        sendRGB(0,0,0);
    }
    show_clock();
    if(diff==0) return 1;
    return 0;
}

uint16_t getColor(uint8_t red, uint8_t green, uint8_t blue){
  return ((red>>3)<<11) | ((green>>2)<<5) | (blue>>3);
}

void update_clock(){
    // Adjust the time values by adding 1 second
    omm = mm; // Save last minute time for display update
    mm++;              // Advance minute
    if (mm > 59) {   // Check for roll-over
    mm = 0;
    hh++;          // Advance hour
        if (hh > 23) { // Check for 24hr roll-over (could roll-over on 13)
            hh = 0;      // 0 for 24 hour clock, set to 1 for 12 hour clock
        }
    }
}

void show_clock(){
    // Update digital time
    int xpos = 10;
    int ypos = 0; // Top left corner ot clock text, about half way down
    int txtsize = 4;
    // White    
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    xpos += M5.Lcd.drawString("Current Time : ",xpos,ypos,txtsize);
    //omm = mm;
    // Draw hours and minutes
    if (hh < 10) xpos += M5.Lcd.drawChar('0', xpos, ypos, txtsize); // Add hours leading zero for 24 hr clock
    xpos += M5.Lcd.drawNumber(hh, xpos, ypos, txtsize);             // Draw hours
    //xcolon = xpos; // Save colon coord for later to flash on/off later
    xpos += M5.Lcd.drawChar(':', xpos, ypos, txtsize);
    if (mm < 10) xpos += M5.Lcd.drawChar('0', xpos, ypos, txtsize); // Add minutes leading zero
    xpos += M5.Lcd.drawNumber(mm, xpos, ypos, txtsize);             // Draw minutes
    
    //Back to yellow
    M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);

}

void show_timer(){
    // Update digital time
    int xpos = 0;
    int ypos = 85; // Top left corner ot clock text, about half way down
    int xbarpos = 0;

    //show if timer is on or off
    if(flag==0){
        if(timerflag){
            M5.Lcd.setTextColor(TFT_BLUE, TFT_GREY);
            xpos += M5.Lcd.drawString("ON   ", xpos, ypos - 4, 4);
        }else{
            M5.Lcd.setTextColor(TFT_BLUE, TFT_GREY);
            xpos += M5.Lcd.drawString("OFF ", xpos, ypos - 4, 4);
        }
    }else{
        if(timerflag){
            M5.Lcd.setTextColor(TFT_BLUE, TFT_BLACK);
            xpos += M5.Lcd.drawString("ON   ", xpos, ypos - 4, 4);
        }else{
            M5.Lcd.setTextColor(TFT_BLUE, TFT_BLACK);
            xpos += M5.Lcd.drawString("OFF ", xpos, ypos - 4, 4);
        }
    }
    M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    // Draw hours and minutes
    xpos += draw_timerchar(hh_/10, xpos, ypos, 8, 1);
    xpos += draw_timerchar(hh_%10, xpos, ypos, 8, 2);
    xpos += M5.Lcd.drawChar(':', xpos, ypos - 8, 8);
    xpos += draw_timerchar(mm_/10, xpos, ypos, 8, 3);
    xpos += draw_timerchar(mm_%10, xpos, ypos, 8, 4);
    xpos += M5.Lcd.drawNumber(flag, xpos, ypos, 4);
}

// set background color to grey if flag is correspond to that place
int draw_timerchar(const uint8_t input, const int xpos, const int ypos, const int txtsize, const int position){
    int xposout = 0;
    if(flag==position) M5.Lcd.setTextColor(TFT_YELLOW, TFT_GREY);
    xposout = M5.Lcd.drawNumber(input, xpos, ypos, txtsize);
    M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    return xposout;
}

void button_action(){
    if(flag==0){
        timerflag = !timerflag;
    }else if(flag==1){
        hh_ += 10;
        if(hh_ > 30) hh_ -= 30;
        if(hh_ > 23) hh_ -= 20;
    }else if(flag==2){
        hh_ += 1;
        if(hh_%10 == 0) hh_ -= 10;
        if(hh_ > 23) hh_ -= 4;
    }else if(flag==3){
        mm_ += 10;
        if(mm_ > 59) mm_ -= 60;
    }else if(flag==4){
        mm_ += 1;
        if(mm_%10 ==0) mm_ -= 10;
    }
}

void ibutton_action(){
    if(flag==0){
        reviceClock();// reset alarm
        alarmON = timer_clock_diff();
    }else if(flag==1){
        if(hh_ > 9){ hh_ -= 10;}
        else{hh_ = 23; }
    }else if(flag==2){
        if(hh_%10 == 0){hh_ += 9;}
        else{hh_ -= 1;}
    }else if(flag==3){
        if(mm_ < 10){mm_ += 50;}
        else{mm_-= 10; }
    }else if(flag==4){
        if(mm_%10 ==0) {mm_ += 9;}
        else{mm_-=1;}
    }
}

void sendRGB(uint8_t red, uint8_t green, uint8_t blue){
    //DMXout[1] = red;
    //DMXout[2] = green;
    //DMXout[3] = blue;
    //uint8_t buf[3] = {red,green,blue};
    bts.print(red); bts.print(",");
    bts.print(green); bts.print(",");
    bts.print(blue); bts.print("\n");
}

void sendserial(uint8_t data){

}

// use T0 as serial port
void reviceClock(){
    bts.println("nowclock\n");
    delay(50);
    //uint8_t buff[3]; //hms
    //for(int i=0;i<3;i++){
    //    buff[i]=bts.read();
    //}
    hh = bts.read();
    mm = bts.read();
    ss = bts.read();
 }