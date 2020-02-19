/*

  Under construction......

  It is a sketch for a radio All band and SSB based on Si4735.

  Under construction.....


  This sketch uses the TFT from MICROYUM 2.0"

  wire up on Arduino UNO, Pro mini

  TFT               Pin
  SCK/SCL           13
  SDA/SDI/MOSI      11
  CS/SS             10
  DC/A0/RS          9
  RET/RESET/RTS     8


  Last update: Jan 2020.
*/

#include <SI4735.h>

// #include <Adafruit_GFX.h>    // Core graphics library
// #include <Adafruit_ST7735.h> // Hardware-specific library for ST7735

#include <SPI.h>
#include "TFT_22_ILI9225.h"


#include "Rotary.h"

// Test it with patch_init.h or patch_full.h. Do not try load both.
#include "patch_init.h" // SSB patch for whole SSBRX initialization string

const uint16_t size_content = sizeof ssb_patch_content; // see ssb_patch_content in patch_full.h or patch_init.h


// TFT MICROYUM or ILI9225 based device pin setup
#define TFT_RST 8
#define TFT_RS  9
#define TFT_CS  10  // SS
#define TFT_SDI 11  // MOSI
#define TFT_CLK 13  // SCK
#define TFT_LED 0   // 0 if wired to +3.3V directly
#define TFT_BRIGHTNESS 200



#define FM_BAND_TYPE 0
#define MW_BAND_TYPE 1
#define SW_BAND_TYPE 2
#define LW_BAND_TYPE 3


#define RESET_PIN 12

// Enconder PINs
#define ENCODER_PIN_A 2
#define ENCODER_PIN_B 3

// Buttons controllers
#define MODE_SWITCH 4      // Switch MODE (Am/LSB/USB)
#define BANDWIDTH_BUTTON 5 // Used to select the banddwith. Values: 1.2, 2.2, 3.0, 4.0, 0.5, 1.0 KHz
// #define VOL_UP 6        // Volume Up
// #define VOL_DOWN 7      // Volume Down
#define BAND_BUTTON_UP 6   // Next band
#define BAND_BUTTON_DOWN 7 // Previous band
#define AGC_SWITCH 14      // Pin A0 - Switch AGC ON/OF
#define STEP_SWITCH 15     // Pin A1 - Used to select the increment or decrement frequency step (1, 5 or 10 KHz)
#define BFO_SWITCH 16      // Pin A3 - Used to select the enconder control (BFO or VFO)



#define CAPACITANCE 30  // You might need to adjust this value.
#define MIN_ELAPSED_TIME 100
#define MIN_ELAPSED_RSSI_TIME 150

#define DEFAULT_VOLUME 50 // change it for your favorite sound volume

#define FM 0
#define LSB 1
#define USB 2
#define AM 3
#define LW 4

#define SSB 1


bool bfoOn = false;
bool disableAgc = true;
bool ssbLoaded = false;
bool fmStereo = true;
bool touch = false;

int currentBFO = 0;
int previousBFO = 0;

long elapsedRSSI = millis();
long elapsedButton = millis();
long elapsedFrequency = millis();

// Encoder control variables
volatile int encoderCount = 0;

// Some variables to check the SI4735 status
uint16_t currentFrequency;
uint16_t previousFrequency;
uint8_t currentStep = 1;
uint8_t currentBFOStep = 25;

uint8_t bwIdxSSB = 2;
const char * const bandwitdthSSB[]  = {"1.2", "2.2", "3.0", "4.0", "0.5", "1.0"};

uint8_t bwIdxAM = 1;
const char * const bandwitdthAM[]  = {"6", "4", "3", "2", "1", "1.8", "2.5"};

const char * const bandModeDesc[]  = {"FM ", "LSB", "USB", "AM "};
uint8_t currentMode = FM;

char bufferDisplay[64]; // Useful to handle string
char bufferFreq[10];
char bufferBFO[15];
char bufferStep[15];
char bufferBW[15];
char bufferAGC[15];
char bufferBand[15];
char bufferStereo[10];



/*
   Band data structure
*/
typedef struct
{
  const char *bandName; // Band description
  uint8_t bandType;     // Band type (FM, MW or SW)
  uint16_t minimumFreq; // Minimum frequency of the band
  uint16_t maximumFreq; // maximum frequency of the band
  uint16_t currentFreq; // Default frequency or current frequency
  uint16_t currentStep; // Defeult step (increment and decrement)
} Band;

/*
   Band table
*/
Band band[]  = {
  {"FM  ", FM_BAND_TYPE, 8400, 10800, 10390, 10},
  {"LW  ", LW_BAND_TYPE, 100, 510, 300, 1},
  {"AM  ", MW_BAND_TYPE, 520, 1720, 810, 10},
  {"160m", SW_BAND_TYPE, 1800, 3500, 1900, 1}, // 160 meters
  {"80m ", SW_BAND_TYPE, 3500, 4500, 3700, 1}, // 80 meters
  {"60m ", SW_BAND_TYPE, 4500, 5500, 4850, 5},
  {"49m ", SW_BAND_TYPE, 5600, 6300, 6000, 5},
  {"41m ", SW_BAND_TYPE, 6800, 7800, 7100, 5}, // 40 meters
  {"31m ", SW_BAND_TYPE, 9200, 10000, 9600, 5},
  {"30m ", SW_BAND_TYPE, 10000, 11000, 10100, 1}, // 30 meters
  {"25m ", SW_BAND_TYPE, 11200, 12500, 11940, 5},
  {"22m ", SW_BAND_TYPE, 13400, 13900, 13600, 5},
  {"20m ", SW_BAND_TYPE, 14000, 14500, 14200, 1}, // 20 meters
  {"19m ", SW_BAND_TYPE, 15000, 15900, 15300, 5},
  {"18m ", SW_BAND_TYPE, 17200, 17900, 17600, 5},
  {"17m ", SW_BAND_TYPE, 18000, 18300, 18100, 1},  // 17 meters
  {"15m ", SW_BAND_TYPE, 21000, 21900, 21200, 1},  // 15 mters
  {"12m ", SW_BAND_TYPE, 24890, 26200, 24940, 1},  // 12 meters
  {"CB  ", SW_BAND_TYPE, 26200, 27900, 27500, 1},  // CB band (11 meters)
  {"10m ", SW_BAND_TYPE, 28000, 30000, 28400, 1}
};

const int lastBand = (sizeof band / sizeof(Band)) - 1;
int bandIdx = 0;

uint8_t rssi = 0;
uint8_t stereo = 1;
uint8_t volume = DEFAULT_VOLUME;

// Devices class declarations
Rotary encoder = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);


// For 1.44" and 1.8" TFT with ST7735 use:
// Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Use hardware SPI (faster - on Uno: 13-SCK, 12-MISO, 11-MOSI)
TFT_22_ILI9225 tft = TFT_22_ILI9225(TFT_RST, TFT_RS, TFT_CS, TFT_LED, TFT_BRIGHTNESS);


SI4735 si4735;

void setup()
{
  // Encoder pins
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);

  pinMode(BANDWIDTH_BUTTON, INPUT_PULLUP);
  pinMode(BAND_BUTTON_UP, INPUT_PULLUP);
  pinMode(BAND_BUTTON_DOWN, INPUT_PULLUP);
  // pinMode(VOL_UP, INPUT_PULLUP);
  // pinMode(VOL_DOWN, INPUT_PULLUP);
  pinMode(BFO_SWITCH, INPUT_PULLUP);
  pinMode(AGC_SWITCH, INPUT_PULLUP);
  pinMode(STEP_SWITCH, INPUT_PULLUP);
  pinMode(MODE_SWITCH, INPUT_PULLUP);


  // Use this initializer if using a 1.8" TFT screen:
  tft.begin();
  tft.setOrientation(1);
  tft.clear();
  /*
    tft.setFont(Terminal6x8);
    tft.drawText(36, 20, "SI4735 Arduino Library", COLOR_RED); // Print string
    tft.drawText(80, 60, "By PU2CLR", COLOR_YELLOW);
    int16_t si4735Addr = si4735.getDeviceI2CAddress(RESET_PIN);
    if ( si4735Addr == 0 ) {
    tft.drawText(15, 120, "Si47XX was not detected!", COLOR_YELLOW);
    // while (1);
    } else {
    sprintf(bufferDisplay, "The Si473X I2C address is 0x%x ", si4735Addr);
    tft.drawText(15, 120, bufferDisplay, COLOR_RED);
    }

    delay(2000);
    tft.clear();
  */

  showTemplate();
  // while (1);
  // Encoder interrupt
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), rotaryEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), rotaryEncoder, CHANGE);


  // The line below may be necessary to setup I2C pins on ESP32
  // Wire.begin(ESP32_I2C_SDA, ESP32_I2C_SCL);

  si4735.setup(RESET_PIN, 1);

  // Set up the radio for the current band (see index table variable bandIdx )
  useBand();
  currentFrequency = previousFrequency = si4735.getFrequency();

  si4735.setVolume(volume);

  clearBuffer(bufferDisplay);
  clearBuffer(bufferFreq);
  clearBuffer(bufferStep);
  clearBuffer(bufferBFO);
  clearBuffer(bufferBW);
  clearBuffer(bufferAGC);
  clearBuffer(bufferBand);

  showStatus();
}


/*
   Shows the static content on  display
*/
void showTemplate() {


  // See https://github.com/Nkawu/TFT_22_ILI9225/wiki

  tft.drawRectangle(0, 0, tft.maxX() - 1, tft.maxY() - 1, COLOR_WHITE);
  tft.drawRectangle(2, 2, tft.maxX() - 3, 40, COLOR_YELLOW);
  tft.drawLine(150, 0, 150, 40, COLOR_YELLOW) ;

  tft.drawLine( 0, 80, tft.maxX() - 1, 80, COLOR_YELLOW) ; //
  tft.drawLine(60, 40, 60, 80, COLOR_YELLOW) ; // Mode Block
  tft.drawLine(120, 40, 120, 80, COLOR_YELLOW) ; // Band name


}

// Just clear the buffer string array;
void clearBuffer(char * b) {
  b[0] = '\0';
}

/*
    Prevents blinking during the frequency display.
    Erases the old digits if it has changed and print the new digit values.
*/
void printValue(int col, int line, char *oldValue, char *newValue, uint16_t color, uint8_t space) {
  int c = col;
  char * pOld;
  char * pNew;

  pOld = oldValue;
  pNew = newValue;

  // prints just changed digits
  while (*pOld && *pNew)
  {
    if (*pOld != *pNew)
    {
      tft.drawChar(c, line, *pOld, COLOR_BLACK);
      tft.drawChar(c, line, *pNew, color);
    }
    pOld++;
    pNew++;
    c += space;
  }

  // Is there anything else to erase?
  while (*pOld)
  {
    tft.drawChar(c, line, *pOld, COLOR_BLACK);
    pOld++;
    c += space;
  }

  // Is there anything else to print?
  while (*pNew)
  {
    tft.drawChar(c, line, *pNew, color);
    pNew++;
    c += space;
  }

  // Save the current content to be tested next time
  strcpy(oldValue, newValue)

}

/*
    Reads encoder via interrupt
    Use Rotary.h and  Rotary.cpp implementation to process encoder via interrupt
*/
void rotaryEncoder()
{ // rotary encoder events
  uint8_t encoderStatus = encoder.process();
  if (encoderStatus)
  {
    if (encoderStatus == DIR_CW)
    {
      encoderCount = 1;
    }
    else
    {
      encoderCount = -1;
    }
  }
}


/*
   Shows frequency information on Display
*/


void showFrequency()
{
  float freq;
  int iFreq, dFreq;
  uint16_t color;


  tft.setFont(Trebuchet_MS16x21);

  if (si4735.isCurrentTuneFM())
  {
    freq = currentFrequency / 100.0;
    dtostrf(freq, 3, 1, bufferDisplay);
  }
  else
  {
    freq = currentFrequency / 1000.0;
    if ( currentFrequency < 1000 )
      sprintf(bufferDisplay, "%3d", currentFrequency);
    else
      dtostrf(freq, 2, 3, bufferDisplay);
  }
  color = (bfoOn) ? COLOR_CYAN : COLOR_YELLOW;

  printValue(10, 10, bufferFreq, bufferDisplay, color, 20);
  // strcpy(bufferFreq, bufferDisplay);  // Save the current value in another array string
}

/*
    Show some basic information on display
*/
void showStatus()
{
  char unit[5];
  si4735.getStatus();
  si4735.getCurrentReceivedSignalQuality();
  // SRN

  si4735.getFrequency();
  showFrequency();

  tft.setFont(Terminal6x8);

  if (si4735.isCurrentTuneFM()) {
    tft.drawText(155, 30, "MHz", COLOR_RED);
    showBFOTemplate(COLOR_BLACK);
    clearBufferBW();
  } else {
    sprintf(bufferDisplay, "Step: %2.2d", currentStep);
    tft.drawText(155, 10, bufferDisplay, COLOR_YELLOW);
    tft.drawText(155, 30, "KHz", COLOR_RED);
  }

  // Band information
  tft.drawText(4, 60, bufferBand, COLOR_BLACK);
  if ( band[bandIdx].bandType == SW_BAND_TYPE)
    sprintf(bufferDisplay, "%s %s", band[bandIdx].bandName, bandModeDesc[currentMode]);
  else
    sprintf(bufferDisplay, "%s", band[bandIdx].bandName);
  tft.drawText(4, 60, bufferDisplay, COLOR_CYAN);
  strcpy(bufferBand, bufferDisplay);

  // AGC
  tft.drawText(65, 60, bufferAGC, COLOR_BLACK);
  si4735.getAutomaticGainControl();
  sprintf(bufferDisplay, "AGC %s", (si4735.isAgcEnabled()) ? "ON  " : "OFF");
  tft.drawText(65, 60, bufferDisplay, COLOR_CYAN);
  strcpy(bufferAGC, bufferDisplay);


  // Bandwidth
  if (currentMode == LSB || currentMode == USB)
  {
    tft.drawText(150, 60, bufferStereo, COLOR_BLACK);
    sprintf(bufferDisplay, "BW: %s KHz", bandwitdthSSB[bwIdxSSB]);
    clearBufferBW();
    tft.drawText(124, 45, bufferDisplay, COLOR_CYAN);
    strcpy( bufferBW, bufferDisplay);
    showBFOTemplate(COLOR_CYAN);
    showBFO();
  }
  else if (currentMode == AM) {
    tft.drawText(150, 60, bufferStereo, COLOR_BLACK);
    sprintf(bufferDisplay, "BW: %s KHz", bandwitdthAM[bwIdxAM]);
    clearBufferBW();
    tft.drawText(124, 45, bufferDisplay, COLOR_CYAN);
    strcpy( bufferBW, bufferDisplay);
    showBFOTemplate(COLOR_BLACK);
  }

}

/* *******************************
   Shows RSSI status
*/


void showRSSI() {

  tft.setFont(Terminal6x8);
  if (  currentMode == FM ) {
    tft.drawText(150, 60, bufferStereo, COLOR_BLACK);
    sprintf(bufferDisplay, "%s", (si4735.getCurrentPilot()) ? "STEREO" : "MONO");
    tft.drawText(150, 60, bufferDisplay, COLOR_CYAN);
    strcpy(bufferStereo, bufferDisplay);
  }

}

// Clear the Bandwidth Filter filed
inline void clearBufferBW() {
  tft.drawText(124, 45, bufferBW, COLOR_BLACK);
}

// Displays the static area for the SSB/BFO information
void showBFOTemplate(uint16_t color) {

  tft.drawText(150, 60, bufferStereo, COLOR_BLACK); 
  
  tft.setFont(Terminal6x8);
  tft.drawText(124, 55, "BFO.:", color);
  tft.drawText(124, 65, "Step:", color);

  tft.drawText(160, 55, bufferBFO, COLOR_BLACK);
  tft.drawText(160, 65, bufferStep, COLOR_BLACK);
}

void showBFO()
{

  tft.setFont(Terminal6x8);

  // tft.drawText(160, 55, bufferBFO, COLOR_BLACK);
  // tft.drawText(160, 65, bufferStep, COLOR_BLACK);


  sprintf(bufferDisplay, "%+4d", currentBFO);
  // tft.drawText(160, 55, bufferDisplay, COLOR_CYAN);
  printValue(150, 55, bufferBFO, bufferDisplay, color, 5);

  // strcpy(bufferBFO, bufferDisplay);

  sprintf(bufferDisplay, "%4d", currentBFOStep);
  // tft.drawText(160, 65, bufferDisplay, COLOR_CYAN);
  printValue(150, 65, bufferStep, bufferDisplay, color, 5);
  
  // strcpy(bufferStep, bufferDisplay);

}

/*
   Goes to the next band (see Band table)
*/
void bandUp()
{
  // save the current frequency for the band
  band[bandIdx].currentFreq = currentFrequency;
  band[bandIdx].currentStep = currentStep;

  if (bandIdx < lastBand)
  {
    bandIdx++;
  }
  else
  {
    bandIdx = 0;
  }
  useBand();
}

/*
   Goes to the previous band (see Band table)
*/
void bandDown()
{
  // save the current frequency for the band
  band[bandIdx].currentFreq = currentFrequency;
  band[bandIdx].currentStep = currentStep;
  if (bandIdx > 0)
  {
    bandIdx--;
  }
  else
  {
    bandIdx = lastBand;
  }
  useBand();
}

/*
   This function loads the contents of the ssb_patch_content array into the CI (Si4735) and starts the radio on
   SSB mode.
*/
void loadSSB()
{
  si4735.reset();
  si4735.queryLibraryId(); // Is it really necessary here? I will check it.
  si4735.patchPowerUp();
  delay(50);
  // si4735.setI2CFastMode(); // Recommended
  si4735.setI2CFastModeCustom(500000); // It is a test and may crash.
  si4735.downloadPatch(ssb_patch_content, size_content);
  si4735.setI2CStandardMode(); // goes back to default (100KHz)

  // delay(50);
  // Parameters
  // AUDIOBW - SSB Audio bandwidth; 0 = 1.2KHz (default); 1=2.2KHz; 2=3KHz; 3=4KHz; 4=500Hz; 5=1KHz;
  // SBCUTFLT SSB - side band cutoff filter for band passand low pass filter ( 0 or 1)
  // AVC_DIVIDER  - set 0 for SSB mode; set 3 for SYNC mode.
  // AVCEN - SSB Automatic Volume Control (AVC) enable; 0=disable; 1=enable (default).
  // SMUTESEL - SSB Soft-mute Based on RSSI or SNR (0 or 1).
  // DSP_AFCDIS - DSP AFC Disable or enable; 0=SYNC MODE, AFC enable; 1=SSB MODE, AFC disable.
  si4735.setSSBConfig(bwIdxSSB, 1, 0, 0, 0, 1);
  delay(25);
  ssbLoaded = true;
}

/*
   Switch the radio to current band
*/
void useBand()
{
  if (band[bandIdx].bandType == FM_BAND_TYPE)
  {
    currentMode = FM;
    si4735.setTuneFrequencyAntennaCapacitor(0);
    si4735.setFM(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, band[bandIdx].currentStep);
    bfoOn = ssbLoaded = false;
  }
  else
  {
    if (band[bandIdx].bandType == MW_BAND_TYPE || band[bandIdx].bandType == LW_BAND_TYPE)
      si4735.setTuneFrequencyAntennaCapacitor(0);
    else
      si4735.setTuneFrequencyAntennaCapacitor(1);

    if (ssbLoaded)
    {
      si4735.setSSB(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, band[bandIdx].currentStep, currentMode);
      si4735.setSSBAutomaticVolumeControl(1);
      si4735.setSsbSoftMuteMaxAttenuation(0); // Disable Soft Mute for SSB
    }
    else
    {
      currentMode = AM;
      si4735.setAM(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, band[bandIdx].currentStep);
      si4735.setAutomaticGainControl(1, 0);
      si4735.setAmSoftMuteMaxAttenuation(0); // // Disable Soft Mute for AM
      bfoOn = false;
    }
  }
  delay(100);
  currentFrequency = band[bandIdx].currentFreq;
  currentStep = band[bandIdx].currentStep;
  showStatus();
}

void loop()
{
  // Check if the encoder has moved.
  if (encoderCount != 0)
  {
    if (bfoOn)
    {
      currentBFO = (encoderCount == 1) ? (currentBFO + currentBFOStep) : (currentBFO - currentBFOStep);
      si4735.setSSBBfo(currentBFO);
      showBFO();
    }
    else
    {
      if (encoderCount == 1)
        si4735.frequencyUp();
      else
        si4735.frequencyDown();
      // Show the current frequency only if it has changed
      currentFrequency = si4735.getFrequency();
    }
    showFrequency();
    encoderCount = 0;
  }

  // Check button commands
  if ((millis() - elapsedButton) > MIN_ELAPSED_TIME)
  {
    // check if some button is pressed
    if (digitalRead(BANDWIDTH_BUTTON) == LOW)
    {
      if (currentMode == LSB || currentMode == USB)
      {
        bwIdxSSB++;
        if (bwIdxSSB > 5)
          bwIdxSSB = 0;
        si4735.setSSBAudioBandwidth(bwIdxSSB);
        // If audio bandwidth selected is about 2 kHz or below, it is recommended to set Sideband Cutoff Filter to 0.
        if (bwIdxSSB == 0 || bwIdxSSB == 4 || bwIdxSSB == 5)
          si4735.setSBBSidebandCutoffFilter(0);
        else
          si4735.setSBBSidebandCutoffFilter(1);
      }
      else if (currentMode == AM)
      {
        bwIdxAM++;
        if (bwIdxAM > 6)
          bwIdxAM = 0;
        si4735.setBandwidth(bwIdxAM, 0);
      }
      showStatus();
      delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
    }
    else if (digitalRead(BAND_BUTTON_UP) == LOW)
      bandUp();
    else if (digitalRead(BAND_BUTTON_DOWN) == LOW)
      bandDown();
    /*else if (digitalRead(VOL_UP) == LOW)
      {
      si4735.volumeUp();
      delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
      }
      else if (digitalRead(VOL_DOWN) == LOW)
      {
      si4735.volumeDown();
      delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
      } */
    else if (digitalRead(BFO_SWITCH) == LOW)
    {
      if (currentMode == LSB || currentMode == USB) {
        bfoOn = !bfoOn;
        if (bfoOn) {
          showBFOTemplate(COLOR_CYAN);
          showBFO();
          showStatus();
        }
        else {
          showBFOTemplate(COLOR_BLACK);
        }
        clearBuffer(bufferFreq);
      } else if (currentMode == FM) {
        si4735.seekStationUp();
        currentFrequency = si4735.getFrequency();
      }
      delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
      showFrequency();
    }
    else if (digitalRead(AGC_SWITCH) == LOW)
    {
      disableAgc = !disableAgc;
      // siwtch on/off ACG; AGC Index = 0. It means Minimum attenuation (max gain)
      si4735.setAutomaticGainControl(disableAgc, 1);
      showStatus();
    }
    else if (digitalRead(STEP_SWITCH) == LOW)
    {
      if ( currentMode == FM) {
        fmStereo = !fmStereo;
        if ( fmStereo )
          si4735.setFmStereoOn();
        else
          si4735.setFmStereoOff(); // It is not working so far.
      } else {

        // This command should work only for SSB mode
        if (bfoOn && (currentMode == LSB || currentMode == USB))
        {
          currentBFOStep = (currentBFOStep == 25) ? 10 : 25;
          showBFO();
        }
        else
        {
          if (currentStep == 1)
            currentStep = 5;
          else if (currentStep == 5)
            currentStep = 10;
          else
            currentStep = 1;
          si4735.setFrequencyStep(currentStep);
          band[bandIdx].currentStep = currentStep;
          showStatus();
        }
        delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
      }
    }
    else if (digitalRead(MODE_SWITCH) == LOW)
    {
      if (currentMode != FM ) {
        if (currentMode == AM)
        {
          // If you were in AM mode, it is necessary to load SSB patch (avery time)
          loadSSB();
          currentMode = LSB;
        }
        else if (currentMode == LSB)
        {
          currentMode = USB;
        }
        else if (currentMode == USB)
        {
          currentMode = AM;
          ssbLoaded = false;
          bfoOn = false;
        }
        // Nothing to do if you are in FM mode
        band[bandIdx].currentFreq = currentFrequency;
        band[bandIdx].currentStep = currentStep;
        useBand();
      }
    }
    elapsedButton = millis();
  }

  // Show RSSI status only if this condition has changed
  if ((millis() - elapsedRSSI) > MIN_ELAPSED_RSSI_TIME * 12)
  {
    si4735.getCurrentReceivedSignalQuality();
    int aux = si4735.getCurrentRSSI();
    if (rssi != aux)
    {
      rssi = aux;
      showRSSI();
    }
    elapsedRSSI = millis();
  }

  delay(10);
}
