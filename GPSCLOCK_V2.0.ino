#include <avr/pgmspace.h>
#include <Time.h>
#include <TimeLib.h>
#include <Wire.h>
#include <Encoder.h>
#include <TinyGPS.h>
#include <DS3232RTC.h>
#include <SoftwareSerial.h>
#include <Adafruit_NeoPixel.h>

// Defines / settings
#define RGBLEDPIN     6
#define RotaryAPIN    2
#define RotaryBPIN    3
#define TimeChangePIN 4
#define GPSclockRX   10
#define GPSClockTx   11
#define N_LEDS       117          // 13 wide x 9 high grid            !!!MAX 255!!!
#define colorAmount  (4*(sizeof(colors)/sizeof(uint32_t)))
// A4 & A5 for i2c of the DS3232


SoftwareSerial SerialGPS = SoftwareSerial(GPSclockRX, GPSClockTx);
TinyGPS gps;
Encoder myEnc(RotaryAPIN, RotaryBPIN);
long oldPosition  = -999;
long old2Position = -999;
boolean encoderPushed = false;
int prevMin = 0;
Adafruit_NeoPixel grid = Adafruit_NeoPixel(N_LEDS, RGBLEDPIN, NEO_GRB + NEO_KHZ400);
int intBrightness = 255; // the brightness of the clock (0 = off and 255 = 100%)
int offset = 2; //(GMT+1, add another 1 for summertime) -- changeable with pushable encoder


//Clock Colours
uint32_t colorVariable; //current colour
const PROGMEM uint32_t colorWhite = grid.Color(255, 255, 255);
const PROGMEM uint32_t colorBlack = grid.Color(0, 0, 0);
const PROGMEM uint32_t colorRed = grid.Color(255, 30, 30);
const PROGMEM uint32_t colorGreen = grid.Color(30, 255, 30);
const PROGMEM uint32_t colorBlue = grid.Color(30, 30, 255);
const PROGMEM uint32_t colorPurple = grid.Color(255, 0, 255);
const PROGMEM uint32_t colorYellow = grid.Color(125, 255, 0);
const PROGMEM uint32_t colorWhatever = grid.Color(102, 51, 204);
const PROGMEM uint32_t colorMore = grid.Color(255, 153, 0);
uint32_t colorMix;
const PROGMEM uint32_t colors[] = {colorWhite, colorRed, colorGreen, colorBlue, colorPurple, colorYellow, colorWhatever, colorMore, colorMix}; //cycle through with encoder
int AllOrNothing[] = {60, 157, 255};

//Separate words
byte arrITIS[] = {116, 115, 113, 112, 255};
byte arrNAME[] = {70, 71, 72, 73, 74, 255};


// minutesArray = all Minutes leds in a bi-dimensional array from {NONE, FIVE, ..., FIVE TO}, 255 as endstop char
byte minutesArray [12] [22] {
  {5, 4, 3, 2, 1, 0, 255}, //O'CLOCK
  {90, 89, 88, 87, 85, 84, 83, 82, 81, 80, 79, 65, 66, 67, 68, 255},                              //FIVE MINUTES PAST
  {106, 105, 104, 65, 66, 67, 68, 255},                                                           //TEN  PAST
  {109, 91, 92, 93, 94, 95, 96, 97, 65, 66, 67, 68, 255},                                         //A QUARTER PAST
  {98, 99, 100, 101, 102, 103, 85, 84, 83, 82, 81, 80, 79, 5, 66, 67, 68, 255},                   //TWENTY MINUTES PAST
  {98, 99, 100, 101, 102, 103, 90, 89, 88, 87, 85, 84, 83, 82, 81, 80, 79, 65, 66, 67, 68, 255},  //TWENTY FIVE MINUTES PAST
  {110, 109, 108, 107, 65, 66, 67, 68, 255},                                                      //HALF PAST
  {98, 99, 100, 101, 102, 103, 90, 89, 88, 87, 85, 84, 83, 82, 81, 80, 79, 76, 77, 255},          //TWENTY FIVE MINUTES TO
  {98, 99, 100, 101, 102, 103, 85, 84, 83, 82, 81, 80, 79, 76, 77, 255},     //TWENTY MINUTES TO
  {109, 91, 92, 93, 94, 95, 96, 97, 76, 77, 255},                            //A QUARTER TO
  {106, 105, 104, 85, 84, 83, 82, 81, 80, 79, 76, 77, 255},                  //TEN TO
  {90, 89, 88, 87, 85, 84, 83, 82, 81, 80, 79, 76, 77, 255}                         //FIVE TO
};

// hoursArray = all hours leds in a bi-dimensional array from {12, 1, ..., 11}, 255 as endstop char
byte hoursArray [12] [7] {
  {12, 11, 10, 9, 8, 7, 255}, //TWELVE
  {64, 63, 62, 255},       //ONE
  {60, 59, 58, 255},       //TWO
  {56, 55, 54, 53, 52, 255}, //THREE
  {39, 40, 41, 42, 255},   //FOUR
  {43, 44, 45, 46, 255},   //FIVE
  {47, 48, 49, 50, 51, 255}, //SIX
  {38, 37, 36, 255},       //SEVEN
  {35, 34, 33, 32, 31, 255}, //EIGHT
  {29, 28, 27, 26, 255},   //NINE
  {14, 15, 16, 255},       //TEN
  {19, 20, 21, 22, 23, 24, 255}, //ELEVEN
};

void setup()
{
  randomSeed(analogRead(A0));
  Serial.begin(9600);
  while (!Serial) ; // Needed for Leonardo only
  SerialGPS.begin(9600);
  Serial.println("Booting ... ");

  delay(200);

  // initialize the buttons
  pinMode(TimeChangePIN, INPUT_PULLUP); //temp pulled up to not have the low on the button..
  pinMode(RotaryAPIN, INPUT_PULLUP);
  pinMode(RotaryBPIN, INPUT_PULLUP);

  // setup the LED strip
  grid.begin();
  grid.show();

  // set the brightness of the strip
  grid.setBrightness(intBrightness);

  //startup sequence
  colorWipe(colorBlack, 0);
  paintRandomSelected(arrNAME);
  fadeOut(25);
  colorVariable = colors[0];


}

void loop() {
  while (SerialGPS.available()) {
    if (gps.encode(SerialGPS.read())) { // process gps messages
      // when TinyGPS reports new data...
      unsigned long age;
      int Year;
      byte Month, Day, Hour, Minute, Second;
      gps.crack_datetime(&Year, &Month, &Day, &Hour, &Minute, &Second, NULL, &age);
      if (age < 500) {
        //Serial.println("GPS time available with good age - updated RTC");
        // set the Time to the latest GPS reading
        setTime(Hour, Minute, Second, Day, Month, Year);
        RTC.set(now());
      }
    }
  }
  setTime(RTC.get());


  if (digitalRead(TimeChangePIN) == HIGH) {
    if (encoderPushed != digitalRead(TimeChangePIN)) {
      myEnc.write(0);
      colorWipe(colorBlack, 0);
    }
    encoderPushed = digitalRead(TimeChangePIN);

    // encoder rotation for colours
    long newPosition = myEnc.read();
    if (newPosition > colorAmount) {
      newPosition = 0;
      myEnc.write(0);
    }
    if (newPosition < 0) {
      newPosition = colorAmount;
      myEnc.write(colorAmount);
    }
    if (newPosition != oldPosition) {
      oldPosition = newPosition;
      colorVariable = colors[(oldPosition / 4)];

      colorWipe(colorBlack, 0);
      delay(100);

      //update now to show color change
      if (colorVariable == colorMix) {
        displayTimeRandom();
      }
      else if (colorVariable != colorMix) {
        displayTime();
      }

    }
  }

  // encoder rotation for time
  if (digitalRead(TimeChangePIN) == LOW) {
    if (encoderPushed != digitalRead(TimeChangePIN)) {
      myEnc.write(0);
      colorWipe(colorBlack, 0);
    }
    encoderPushed = digitalRead(TimeChangePIN);

    long timezonePosition = myEnc.read();
    //Serial.print("Encoder: ");
    //Serial.println(timezonePosition);
    if (timezonePosition > 48) {
      timezonePosition = 0;
      myEnc.write(0);
    }
    if (timezonePosition < 0) {
      timezonePosition = 48;
      myEnc.write(48);
    }
    if (timezonePosition != old2Position) {
      old2Position = timezonePosition;

      Serial.print("  Timezone: ");
      Serial.println(offset);
      offset = old2Position / 4;

      colorWipe(colorBlack, 0);
      delay(100);

      //update now to show color change
      if (colorVariable == colorMix) {
        displayTimeRandom();
      }
      else if (colorVariable != colorMix) {
        displayTime();
      }
    }
  }

  grid.setBrightness(intBrightness);

  if (timeStatus() != timeNotSet) {
    if (minute() > prevMin) { //update the display only if at least 60 seconds have passed
      prevMin = minute();
      digitalClockDisplay();
      if (colorVariable != colorMore) {
        displayTime();
      }
      else if (colorVariable == colorMore) {
        displayTimeRandom();
      }
    }
  }
}

void digitalClockDisplay() {
  // digital clock display of the time
  Serial.print(hour()+offset);
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print("-");
  Serial.print(month());
  Serial.print("-");
  Serial.print(year());
  Serial.print(" offset: "); //timezone adjustment
  Serial.print(offset);
  Serial.println();

}

void printDigits(int digits) {
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t color, uint8_t wait) {
  for (uint16_t i = 0; i < grid.numPixels(); i++) {
    grid.setPixelColor(i, color);
  }
  grid.show();
  delay(wait);
}

void paintWord(byte arrWord[], uint32_t intColor) {
  for (int i = 0; i < grid.numPixels() + 1; i++) {
    int temp = (int)arrWord[i];
    if (temp == 255) {
      grid.show();
      break;
    } else {
      grid.setPixelColor(temp, intColor);
    }
  }
}


void paintRandom(byte arrWord[]) {
  int a = random(0, 256);
  int b = random(0, 256);
  int c = random(0, 256);
  uint32_t randomColor = grid.Color(a, b, c);
  for (int i = 0; i < grid.numPixels() + 1; i++) {
    int temp = (int)arrWord[i];
    if (temp == 255) { //255 instead of -1 because reasons (no, because i have byte arrays now, so don't go over 255 LEDS!)
      grid.show();
      break;
    } else {
      grid.setPixelColor(temp, randomColor);
    }
  }
}

void paintRandomSelected(byte arrWord[]) {
  for (int i = 0; i < grid.numPixels() + 1; i++) {
    int temp = (int)arrWord[i];
    uint32_t randomColor = grid.Color(AllOrNothing[random(3)], AllOrNothing[random(3)], AllOrNothing[random(3)]);
    if (temp == 255) { //255 instead of -1 because reasons (no, because i have byte arrays now, so don't go over 255 LEDS!)
      grid.show();
      break;
    } else {
      grid.setPixelColor(temp, randomColor);
    }
  }
}


void fadeOut(int time) {
  for (int i = intBrightness; i > 0; --i) {
    grid.setBrightness(i);
    grid.show();
    delay(time);
  }
}

void displayTime() {
  paintWord(arrITIS, colorVariable);

  int onHour = (hour()+offset) % 12;

  // Set the HOURS first
  if (minute() >= 35) { //now its TO the next hour
    if (onHour == 11) { // exception due to end of array
      paintWord(hoursArray[onHour], colorBlack); //turn off previous hour (current)
      paintWord(hoursArray[0], colorVariable); //loop back to beginning of 12 hour cycle and light up
    }
    else { //not (yet) TO TWELVE, so normal hour addition for the TO
      paintWord(hoursArray[onHour], colorBlack); //turn off previous hour
      paintWord(hoursArray[(onHour + 1)], colorVariable); //loop back to beginning of 12 hour cycle and light up
    }
  }
  else { // minutes < 35
    if (onHour == 0) { // exception due to start of aaray
      paintWord(hoursArray[11], colorBlack); //turn off previous  11 oclock, we're now at 12 (loopback)
      paintWord(hoursArray[onHour], colorVariable); //starting again from start of array
    }
    else {
      paintWord(hoursArray[(onHour - 1)], colorBlack); //turn off previous
      paintWord(hoursArray[onHour], colorVariable); //turn on current hour
    }
  }

  int cMins = minute() / 5;

  if (cMins == 0) {
    paintWord(minutesArray[11], colorBlack); //turn off FIVE MINUTES TO
    paintWord(minutesArray[cMins], colorVariable); //o'clock
  }
  else {
    paintWord(minutesArray[cMins - 1], colorBlack); //turn off previous minute leds
    paintWord(minutesArray[cMins], colorVariable); //curent minutes leds
  }
}

void displayTimeRandom() {
  paintRandomSelected(arrITIS);

  int onHour = (hour()+offset) % 12;

  // Set the HOURS first
  if (minute() >= 35) { //now its TO the next hour
    if (onHour == 11) { // exception due to end of array
      paintWord(hoursArray[onHour], colorBlack); //turn off previous hour (current)
      paintRandomSelected(hoursArray[0]); //loop back to beginning of 12 hour cycle and light up
    }
    else { //not (yet) TO TWELVE, so normal hour addition for the TO
      paintWord(hoursArray[onHour], colorBlack); //turn off previous hour
      paintRandomSelected(hoursArray[(onHour + 1)]); //loop back to beginning of 12 hour cycle and light up
    }
  }
  else { // minutes < 35
    if (onHour == 0) { // exception due to start of aaray
      paintWord(hoursArray[11], colorBlack); //turn off previous  11 oclock, we're now at 12 (loopback)
      paintRandomSelected(hoursArray[onHour]); //starting again from start of array
    }
    else {
      paintWord(hoursArray[(onHour - 1)], colorBlack); //turn off previous
      paintRandomSelected(hoursArray[onHour]); //turn on current hour
    }
  }

  int cMins = minute() / 5;

  if (cMins == 0) {
    paintWord(minutesArray[11], colorBlack); //turn off FIVE MINUTES TO
    paintRandomSelected(minutesArray[cMins]); //o'clock
  }
  else {
    paintWord(minutesArray[cMins - 1], colorBlack); //turn off previous minute leds
    paintRandomSelected(minutesArray[cMins]); //curent minutes leds
  }
}
