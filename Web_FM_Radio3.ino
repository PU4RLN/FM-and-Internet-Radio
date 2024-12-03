//Revisao da lista de radios para BR e limpando a variavel do RDS ao judar de estacao
//Sem medicao sinal internet
//Ajustando para fones e luminosidade a noite
//invertendo a logica de selecao do radio
//modificacoes no template FM, novas cores
//Funcionando com LILYGO S3 - RADIO FM E INTERNET
//selecionar placa ESP32S3 Dev Mocule
//Porta cu.usbmodem1101
//sem controle de volume, WiFi via celular


#include <TFT_eSPI.h>      //https://github.com/Xinyuan-LilyGO/TTGO-T-Display
#include <SPI.h>
#include "iconradio.h"
#include <Wire.h>
#include "Orbitron_Medium_20.h"
#include "font.h"
#include <WiFiManager.h>     //https://github.com/tzapu/WiFiManager
#include "Arduino.h"
#include "Audio.h"           //https://github.com/schreibfaul1/ESP32-audioI2S
#include <radio.h>           //https://github.com/mathertel/Radio
#include <RDA5807M.h>        //http://www.mathertel.de
#include <RDSParser.h> 
#include <Preferences.h>     //https://randomnerdtutorials.com/esp32-save-data-permanently-preferences/
Preferences preferences;

// DEFINITIONS FOR MODULO FM RDA5807M AND FM PROGRAM

RDA5807M radio;
RDSParser rds;
int sda = 18;
int scl = 17;
float frequency;
#define PINRDA 43                   ///to Power the RDA5807 module
String txt;
String station;
float vol;

// PCM5102 MODULE DEFINITIONS

#define I2S_BCLK      1  // pin BCK of PCM5102
#define I2S_DOUT      2  // pin DIN of PCM5102
#define I2S_LRC       3  // pin LCK of PCM5102
Audio audio;

// ESP32 S3 DEFINITIONS

#define BUTTON1PIN 14             //BUTTON 1
#define BUTTON2PIN 0              //BUTTON 2
#define BUTTON_GPIO GPIO_NUM_14  /// Define button to deep sleep mode
char nomeWifi[15] = "My Web Radio";
char senhaWifi[15] = "12345678";


TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

#define PIN_POWER_ON        15
#define PIN_BAT_VOLT         4
#define PIN_LCD_BL          38

#define ADC_EN              14      //ADC_EN is the ADC detection enable port
#define ADC_PIN             34

#define back 0x9D91
#define middle Monospaced_bold_18
#define middle2 DSEG7_Classic_Bold_30


int nestacao = 6;  ////////to be adjusted when new station is included

const char *nome[6] = {
"1. JB FM Br",
"2. Paradiso FM",
"3. Alvorada FM",
"4. Mix FM Rio",
"5. Radio LAC CH",
"6. LFM Lausanne CH"
};

const char *webpage[6] = {
"https://27383.live.streamtheworld.com/JBFMAAC_SC",
"https://27403.live.streamtheworld.com/PARADISO_SC",
"https://27383.live.streamtheworld.com/RADIO_ALVORADAAAC.aac",
"https://26683.live.streamtheworld.com/MIXRIOAAC_SC",
"http://radiolac.ice.infomaniak.ch/radiolac-high.mp3",
"http://lausannefm.ice.infomaniak.ch/lausannefm-high.mp3"

};


// DEFINITIONS FOR BOTH PROGRAMS

int estacao = 0;
int volume = 10; //(0-20)
int brilho = 1;
String infotxt;
String stname;

unsigned long tempoInicio1 = 0;
unsigned long tempoBotao1 = 0;

bool estadoBotao1;
bool estadoBotaoAnt1;

unsigned long tempoInicio2 = 0;
unsigned long tempoBotao2 = 0;

bool estadoBotao2;
bool estadoBotaoAnt2;

unsigned long targetTime = 0;

/////////////////////////////////////////////////SETUP

void setup() {
  Serial.begin(115200);

  //////////////BOTH PROGRAMS

  preferences.begin("my-app", false); // opens a storage space with a defined namespace to save Data Permanently
  pinMode(BUTTON1PIN, INPUT_PULLUP);
  pinMode(BUTTON2PIN, INPUT_PULLUP);
  
  /////////////DISPLAY
  
  // This IO15 must be set to HIGH, otherwise nothing will be displayed when USB is not connected.
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);

  tft.begin();
  tft.init();
  tft.setRotation(1); //Landscape
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);

  ledcSetup(0, 5000, 8);
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, 170); //brightness 0-220


  ////////////////////////////////// FM RADIO SETUP

  if (digitalRead(10) == LOW) {

   tft.fillScreen(TFT_BLACK);
   tft.pushImage(96,15,128,128, radioicon2);
   tft.setTextColor(TFT_WHITE, TFT_BLACK);
   tft.drawString("FM Radio", 5, 10, 4); //(x, z, font size)
   tft.drawString("By Roge", 260, 120, 2); //(x, z, font size)
   delay(4000);


   pinMode(PINRDA, OUTPUT);
   digitalWrite (PINRDA, HIGH);
   Wire.begin(sda, scl);
   frequency = preferences.getFloat("frequency", 93.3); //read datas permanently saved in the flash memory
   radio.init();
   radio.setBandFrequency(RADIO_BAND_FM, frequency*100);
   radio.setMono(false);
   radio.setMute(false);
   radio.setBassBoost(true);
   radio.setVolume(10); 
   FMscreen ();
   radio.debugEnable();
   radio.attachReceiveRDS(RDS_process);
   rds.attachTextCallback(DisplayText);
   delay(500);
  }

    ////////////////////////////////// INTERNET RADIO SETUP

    else {
    tft.fillScreen(TFT_BLACK);
    tft.pushImage(90,21,128,128, weblog);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("WEB RADIO", 5, 10, 4); //(x, z, font size)
    tft.drawString("By Roge", 200, 110, 4); //(x, z, font size)

    Serial.println("Connecting ...");
    WiFiManager wm;

   if (digitalRead(BUTTON1PIN) == LOW) {
     Serial.println("Reseting ...");
     wm.resetSettings();
    }

   bool res;
   wm.setConfigPortalBlocking(false);
   res = wm.autoConnect(nomeWifi, senhaWifi); // password protected ap


   if (!res) {
     Serial.println("Failed to connect");
     Serial.println("ready to new WiFi connection!");
     Serial.print("WiFi:  ");
     Serial.println(nomeWifi);
     Serial.print("Passworld: ");
     Serial.println(senhaWifi);
     Serial.print("IP:    ");
     Serial.println(WiFi.softAPIP());

     tft.fillScreen(TFT_BLACK);
     tft.setTextColor(TFT_GREEN, TFT_BLACK);  
     tft.setTextSize(2);
     tft.setCursor(0,0);
     tft.println("Read to new WiFi:");  
     tft.print("WiFi:  ");
     tft.println(nomeWifi);
     tft.print("Passworld: ");
     tft.println(senhaWifi);
     tft.print("IP:    ");
     tft.println(WiFi.softAPIP());

     while (true) {
     wm.process();
     }

    }
    else {
     //if you get here you have connected to the WiFi
     Serial.println("CONNECTED!");
     Serial.print("IP: ");
     Serial.println(WiFi.localIP());

     tft.println("CONNECTED!");
     tft.print("IP: ");
     tft.println(WiFi.localIP());
    }

   tft.fillScreen(TFT_BLACK);
   ledcWrite(0, 3); //brightness 0-220
   tft.setFreeFont(&Orbitron_Light_24);
   tft.fillRect (0, 0, 320, 38, TFT_BLUE); //tft.fillRect (X0, Y0, Long, High, color0)  
   tft.setTextColor(TFT_WHITE, TFT_BLUE);
   tft.setCursor(10, 30);
   tft.println("Web Radio");
   //tft.drawLine(0, 38, 320, 38, color4);
   estacao = preferences.getInt("estacao", 1);
   audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
   audio.setVolume(8); // VOLUME FROM 0...21
   audio.connecttohost(webpage[estacao]);
   printwebstation ();
   
  }

} 
/////////////////////////////////////////END SETUP

//////////////////////////////////////////START LOOP

void loop() {


  esp_sleep_enable_ext0_wakeup(BUTTON_GPIO, LOW); /// function to wake up the system when press button 1
  estadoBotao1 = !digitalRead(BUTTON1PIN);
  estadoBotao2 = !digitalRead(BUTTON2PIN);
  if (digitalRead(10) == LOW) {FMRadio();}
  else {webstation ();}
  //battcheck();

}
//////////////////////////////////////////END LOOP

//////////////////////////////////////////WEB RADIO

void  webstation () {
audio.loop();
//// When button 1 is pressed

if (estadoBotao1 && !estadoBotaoAnt1) {
  if (tempoInicio1 == 0) {
    tempoInicio1 = millis();
  }
}  

//// When button 1 is released

if (tempoInicio1 > 100){                                // Debounce filter
  if (!estadoBotao1 && estadoBotaoAnt1) {
    tempoBotao1 = millis() - tempoInicio1;
    tempoInicio1 = 0;  
  }
}

///// First function of button 1: STATION UP

if ((tempoBotao1 > 100) && (tempoBotao1 <= 500)) {
  tempoBotao1 = 0;
  if (estacao < nestacao - 1) {estacao++;} 
  else estacao = 0;
  audio.connecttohost(webpage[estacao]);
  tft.fillRect (0, 39, 320, 64, TFT_BLACK); //tft.fillRect (X0, Y0, Long, High, color0)   
  printwebstation ();
}

///// Second function of button 1: brightness


if (((millis() > tempoInicio1 + 1000) && (millis() <= tempoInicio1 + 1030))) {
  tempoBotao1 = 0;
    if (brilho == 0) {brilho = 1; ledcWrite(0, 3);audio.setVolume(7); }
    else {brilho = 0; ledcWrite(0, 150); audio.setVolume(20);}
}

//// When button 2 is pressed

if (estadoBotao2 && !estadoBotaoAnt2) {
  if (tempoInicio2 == 0) {
    tempoInicio2 = millis();
  }
}  

//// When button 2 is released 
if (tempoInicio2 > 100){                                // Debounce filter
  if (!estadoBotao2 && estadoBotaoAnt2) {
    tempoBotao2 = millis() - tempoInicio2;
    tempoInicio2 = 0;  
  }
}

///// First function of button 2: STATION DW

if ((tempoBotao2 > 100) && (tempoBotao2 <= 500)) {
  tempoBotao2 = 0;
  if (estacao > 0) {estacao--;} 
  else estacao = nestacao - 1 ;
  audio.connecttohost(webpage[estacao]);
  tft.fillRect (0, 39, 320, 64, TFT_BLACK); //tft.fillRect (X0, Y0, Long, High, color0)   
  printwebstation ();
}

///// Second function of button 2: Deep sleep

if (((millis() > tempoInicio2 + 1010) && (millis() <= tempoInicio2 + 1030))) {
  tempoBotao2 = 0;
  if  (estacao != preferences.getInt("estacao", 1)){preferences.putInt("estacao", estacao);} //save the station Permanently
  preferences.end();
  tft.setTextSize(1);  
  tft.fillScreen(TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString("  Charge battery", 0, 40, 4);
  tft.drawString("  or Turn off", 0, 70, 4);
  delay(4000);
  tft.fillScreen(TFT_BLACK);
  esp_deep_sleep_start();  
}
estadoBotaoAnt1 = estadoBotao1;
estadoBotaoAnt2 = estadoBotao2;
}

void audio_showstreamtitle(const char *info){
  infotxt = info;
  Serial.print("streamtitle ");Serial.println(info);
  tft.fillRect (0, 65, 320, 76, TFT_BLACK); //tft.fillRect (X0, Y0, Long, High, color0)
  tft.setFreeFont(&Orbitron_Medium_20);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  //tft.setCursor(0, 90);
  //tft.println(info);
  tft.setCursor(10, 90);
  tft.println(infotxt.substring(0,18));
  tft.setCursor(10, 115);
  tft.println(infotxt.substring(18,37));
  tft.setCursor(10, 140);
  tft.println(infotxt.substring(37,55));
}

void audio_showstation(const char *info){
  Serial.print("station     ");Serial.println(info);
  tft.fillRect (0, 140, 320, 30, TFT_BLACK); //tft.fillRect (X0, Y0, Long, High, color0)
  tft.setFreeFont(&Orbitron_Medium_20);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(10, 165);
  tft.println(info);
}

void audio_lasthost(const char *info){                  //stream URL played
  Serial.print("lasthost    ");Serial.println(info);
}

void printwebstation ()

{    
  tft.fillRect (0, 39, 320, 131, TFT_BLACK); //tft.fillRect (X0, Y0, Long, High, color0)
  tft.setFreeFont(&Orbitron_Medium_20);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(40, 63);
  tft.println (nome[estacao]);
 
}

/////////////////////////////////////////////////END WEB RADIO

/////////////////////////////////////////////////FM RADIO

void FMscreen () {      //display full template

  sprite.createSprite(320,170);
  sprite.fillSprite(back);
  sprite.setFreeFont(&middle);
  sprite.setTextColor(back, TFT_BLACK);
  sprite.drawString("  FM RADIO  ",20,10);
  sprite.setFreeFont(&middle);
  sprite.setTextColor(TFT_BLACK, back);
  sprite.drawString(" Freq:  ",4,40);
  sprite.drawString("MHz ",250,40);
  sprite.setFreeFont(&middle2);
  sprite.drawFloat(frequency,2,100,40);
  sprite.fillRect(0,78,320,2,TFT_BLACK);
  sprite.setFreeFont(&middle);
  sprite.drawString(txt.substring(0,24),10,83);
  sprite.drawString(txt.substring(24,48),10,105);
  sprite.drawString(txt.substring(48,72),10,127);
  //sprite.drawString("St: ",10,149);
  //sprite.drawString(stname.substring(0,24),50,149);
  vol = (analogRead(4) * 2 * 3.3) / 4096;
  sprite.drawString("V: ",10,149);
  sprite.drawString(String(vol),50,149); 
  sprite.pushSprite(0,0);

}

void FMRadio() {  // to check buttons in frequency mode

radio.checkRDS();

//// When button 1 is pressed

if (estadoBotao1 && !estadoBotaoAnt1) {
  if (tempoInicio1 == 0) {
    tempoInicio1 = millis();

  }
}  

//// When button 1 is released 

if (tempoInicio1 > 100){                                // Debounce filter
  if (!estadoBotao1 && estadoBotaoAnt1) {
    tempoBotao1 = millis() - tempoInicio1;
    tempoInicio1 = 0;  
  }
}

///// 1st function of button 1: FREQ UP

if ((tempoBotao1 > 100) && (tempoBotao1 <= 500)) {
  tempoBotao1 = 0;
  if (frequency < 108) {frequency = frequency + 0.10;}
  else {frequency = 88;}
  radio.setBandFrequency(RADIO_BAND_FM, frequency*100);
  txt = "";
  stname = "";
  station = "";
  FMscreen ();
}

///// 2nd function of button 1: brightness


if (((millis() > tempoInicio1 + 1000) && (millis() <= tempoInicio1 + 1010))) {
  tempoBotao1 = 0;
    if (brilho == 0) {brilho = 1; ledcWrite(0, 3);}
    else {brilho = 0; ledcWrite(0, 150);}
}

//// When button 2 is pressed

if (estadoBotao2 && !estadoBotaoAnt2) {
  if (tempoInicio2 == 0) {tempoInicio2 = millis();}
}  

//// When button 2 is released

if (tempoInicio2 > 100) {                    // Debounce filter
  if (!estadoBotao2 && estadoBotaoAnt2) {
    tempoBotao2 = millis() - tempoInicio2;
    tempoInicio2 = 0;       
  }  
}

//// 1st function of button 2: FREQ DOWN

if ((tempoBotao2 > 100) && (tempoBotao2 <= 500)){
  tempoBotao2 = 0;
  if (frequency > 88) {frequency = frequency - 0.10;}
  else {frequency = 108;}
  radio.setBandFrequency(RADIO_BAND_FM, frequency*100);
  txt = "";
  station = "";
  stname = "";
  FMscreen ();
}

//// 2nd function of button 2: Enter deep sleep mode

if (((millis() > tempoInicio2 + 1010) && (millis() <= tempoInicio2 + 1030))){
  tempoBotao2 = 0;
  if  (frequency != preferences.getFloat("frequency", 93.3)){preferences.putFloat("frequency", frequency);} //save the frequency Permanently
  preferences.end();
  tft.setTextSize(1); 
  tft.fillScreen(TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString("  Charge battery", 0, 40, 4);
  tft.drawString("  or Turn off", 0, 70, 4);
  delay(4000);
  tft.fillScreen(TFT_BLACK);
  esp_deep_sleep_start(); ////////////. Start deep sleep mode
}

estadoBotaoAnt1 = estadoBotao1;
estadoBotaoAnt2 = estadoBotao2;

} /// Finish Freq function 

void DisplayText(char *text)
{
  txt = text;
  char s[12];
  radio.formatFrequency(s, sizeof(s));
  Serial.print("text:");
  Serial.println(text);
  Serial.print("Station:"); Serial.println(s);
  stname = s;
  Serial.print("Radio:"); radio.debugRadioInfo();
  Serial.print("Audio:"); radio.debugAudioInfo();

  FMscreen ();

}

void DisplayServiceName(char *name)
{
  station = name;
  Serial.print("RDS: ");
  Serial.println(station);
  FMscreen ();

}

void RDS_process(uint16_t block1, uint16_t block2, uint16_t block3, uint16_t block4) 
{rds.processData(block1, block2, block3, block4);}

/////////////////////////////////////////////////END FM RADIO



