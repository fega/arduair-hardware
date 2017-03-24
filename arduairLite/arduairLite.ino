/******************************************************************************
Arduair project:
Fabian Gutierrez @ Universidad Pontificia Bolivariana
https://github.com/fega/arduair

Arduair-lite version source code.

# Wiring Scheme.
## Arduino
Pin   Feature
2       LED Red
3       LED Green
4       WiFiShield Reserved Pin
5       DHT Pin
7       WifiShield Pin Reserved Pin
8       Shinyei Pin 1
9       Shinyei Pin 2 //TODO: check what pin is
10      SD/SS Pin Reserved Pin
RX/TX1  Ze-Co  Sensor
RX/TX2  Ze-NO2 Sensor
RX/TX3  Ze-SO2 Sensor
SDA/SCL BMP180, RTC, Light Module

## Shinyei PPD42ns
1     Ground
2     P2 OutPut
3     Input 5v
4     P1 Output
******************************************************************************/
//libraries
#include "Wire.h"      //I2C communication
#include "DHT.h"       //DHT sensor
#include <SPI.h>       //Serial communication (with the SD card)
#include <SD.h>        //SD card Library
#include <SFE_BMP180.h>//Sparkfun BMP180 pressure Sensor Library
#include <SparkFunTSL2561.h>//light sensor library SparkFun TSL2561 Breakout
#include <WiFi.h>      //wifi shield Library

#define DEVMODE true  //uncomment to get Serial ouput
//#define TABLESERIAL true

//Default configuration
#define RED_LED_PIN 2
#define GREEN_LED_PIN 3
#define YELLOW_LED_PIN 13
#define DS1307_ADDRESS 0x68 //clock ADRESS
byte zero = 0x00; //work around for an Issue found  in bildr
#define WIFIPIN 4
#define DHTPIN 5
#define SHINYEI_P1 8
#define SHINYEI_P2 9
#define SDPIN 10
#define CONFIGPIN 23
#define DHTTYPE DHT22  //dht type

#define MQ131_VIN 5 //MQ131 input voltaje
#define MQ131_RL 5 //MQ131 Load resistance
float MQ131_RO=5; //MQ131 Load resistance

#define CO  1 //Ze sensors serials
#define NO2 2
#define SO2 3

//Constructors
File myFile;              //FIle constructor
WiFiClient client;        //WiFiClient Constructor
DHT dht(DHTPIN, DHTTYPE); //DHT constructor , m
SFE_BMP180 bmp;           //bmp constructor
SFE_TSL2561 light;        //TSL2561 constructor

//Wifi and device config
char ssid[20]; //  your network SSID (name)
char pass[20];    // your network password (use for WPA, or use as key for WEP)
char server[25];
char device[20];
char password[20];
bool wifi = false;
bool resetClock=false;
int status = WL_IDLE_STATUS;

//Global variables for measuring
float pm10,pm25;
float p,h,t,l;
float co,o3,so2,no2;
unsigned int second, minute,hour,weekDay,monthDay,month,year;

/**
 * Arduair configuration initialization
 */
void setup() {
  digitalWrite(GREEN_LED_PIN,HIGH); // Setup Light On
  pinMode(CONFIGPIN,INPUT);//check config button
  int config = digitalRead(CONFIGPIN);

  #if defined(DEVMODE)
    Serial.begin(9600);
    Serial.println(F("****ARDUAIR START****"));
    if (digitalRead(CONFIGPIN)==HIGH) Serial.println(F("CONFIG PIN: HIGH"));
  #endif

  #if defined(TABLE_TO_SERIAL_MODE)
    Serial.begin(9600);
  #endif
  delay(1000);
  getDate(DS1307_ADDRESS);
  sdBegin();
  arduairSetup();
  if (wifi){wifiBegin();}
  if (config==HIGH) requestConfig();
  if (resetClock==true) timeConfig();
  Wire.begin();
  dht.begin();
  bmp.begin();
  light.begin();
  winsenBegin();
  digitalWrite(GREEN_LED_PIN,LOW); // Setup Light Off
}
/**
* Arduair measuring cycle, it control all measuring cycle and ends writing the
* results in an SD card and sending a request to the server
* the measuring order is:
* 1) 30 s :   measuring PM10 and PM2.5.
* 2) 30 s :   measuring O3
* 3) 30 s :   measuring light, humidity, temperature and Pressure
* 4) 1 s :   Detach Winsen sensor interrupts (to avoid unexpected results during SD writing and HTTP request)
* 5) 1 m :   writing in the SD and sending the request
* 6) 1 s :   clear variables
* Also, asynchronously, Arduair handles the interrupt for the Winsen sensors,
* that comes every second per sensor.
 *
 */
void loop() {
  meteorologyRead();
  delay(1000);
  pmRead();
  mq131Read();
  winsenRead(CO);
  winsenRead(NO2);
  winsenRead(SO2);
  getDate(DS1307_ADDRESS);
  tableWrite();
  if(wifi){request();}
}
/**
 * Perform a request to the given server variable. in the form:
 * http://myserver.com/device/password/monthDay/month/year/hour/minute
 * ?h=humidity&t=temperature&p=pressure&l=luminosity&co=[co]&o3=[o3]&&
 */
void request(){
  // close any connection before send a new request.This will free the socket on the WiFi shield
  client.stop();

 // if there's a successful connection:
 if (client.connect(server, 80)) {
   #if defined(DEVMODE)
   Serial.println(F("connecting..."));
   Serial.print(F("GET "));
   Serial.print(F("/api"));
   Serial.print(F("/")); Serial.print(device); Serial.print(F("/")); Serial.print(password);
   Serial.print(monthDay); Serial.print(month);Serial.print(year); Serial.print(hour);Serial.print(minute);
   #endif
   //String getRequest ="GET"+"hola"+" "
   // send the HTTP GET request:
   client.print(F("GET "));
   client.print(F("/api"));
   client.print(F("/")); client.print(device); client.print("/"); client.print(password);
   // HTTP time
   client.print(monthDay);client.print(month);client.print(year);client.print(hour);client.print(minute);
   //http GET end
   ; client.print(F(" HTTP/1.1"));
   client.println(F(""));
   // parameters:
   client.print(F("h=")); client.print(h); client.print(F(","));
   client.print(F("t=")); client.print(t); client.print(F(","));
   client.print(F("l=")); client.print(l); client.print(F(","));
   client.print(F("co=")); client.print(pm10); client.print(F(","));
   client.print(F("o3=")); client.print(pm10); client.print(F(","));
   client.print(F("pm10=")); client.print(pm10); client.print(F(","));
   client.print(F("pm25=")); client.print(pm25); client.print(F(","));
   client.print(F("so2=")); client.print(so2); client.print(F(","));
   client.print(F("no2=")); client.print(no2); client.print(F(","));
   client.println("");
   //server
   client.print(F("Host: "));client.print(server);
   //client.println("User-Agent: Arduair");
   client.println(F("Connection: close"));
   client.println();
   #if defined(DEVMODE)
   Serial.println(F("Request done"));
   #endif
 }else{
  #if defined(DEVMODE)
  Serial.println(F("Conecction fail"));
   #endif
 }
 #if defined(DEVMODE)
 int timeout = millis() + 20000;
  while (client.available() == 0) {
   if (timeout - millis() < 0) {
     #if defined(DEVMODE)
     Serial.println(F("client Timeout !"));
     #endif
     client.stop();
     warn();
     log(F("client Timeout !"));
   }
 }
 while(client.available()) {
   String response=client.readStringUntil('}');
     Serial.println(response);
 }
 #endif
}
/**
 * Writes the data in the SD.
 * This function act in the following form: first, inactive the wifi-shield
 * and active the SD shield, next writes the data in the SD and inactive
 * the SD.
 */
void tableWrite(){
  //write data in SD
  myFile = SD.open(F("DATA.txt"), FILE_WRITE); //open SD data.txt file

  if (myFile){
    //write ISO date ex: 1994-11-05T08:15:30-05:00
    // myFile.print(year);myFile.print("-");
    // myFile.print(month);myFile.print("-");
    // myFile.print(monthDay);myFile.print("T");
    // myFile.print(hour);myFile.print(":");
    // myFile.print(minute);myFile.print(":");
    // myFile.print(second);
    // myFile.print("+5:00,");


    myFile.print(year);  myFile.print(F(","));
    myFile.print(month); myFile.print(F(","));
    myFile.print(monthDay);myFile.print(F(","));
    myFile.print(hour);  myFile.print(F(","));
    myFile.print(minute);myFile.print(F(","));
    myFile.print(second);myFile.print(F(","));

    myFile.print(h);    myFile.print(F(","));
    myFile.print(t);    myFile.print(F(","));
    myFile.print(p);    myFile.print(F(","));
    myFile.print(l);    myFile.print(F(","));
    myFile.print(co);   myFile.print(F(","));
    myFile.print(so2);  myFile.print(F(","));
    myFile.print(no2);  myFile.print(F(","));
    myFile.print(pm10); myFile.print(F(","));
    myFile.print(pm25); myFile.print(F(","));


    myFile.println(F(" "));
    myFile.close();
  }

}
/**
 * Reads  MQ-131 O3 low concentration sensor
 * @return [description]
 */
float mq131Read(){
  float sensorValue = analogRead(0);// read analog input pin 0
  float Rs = ((MQ131_VIN/sensorValue)/sensorValue)*MQ131_RL;
  float t = temperatureRead();
  float Rs_Ro = Rs/MQ131_RO - 0.0134*t + 0.2356;
  float finalValue = pow(11.434*Rs_Ro,2.1249);

  #if defined(DEVMODE)
    Serial.print(F("  [O3]: "));
    Serial.println(sensorValue);
    //Serial.println(Rs);
    //Serial.println(t);
    //Serial.println(Rs_Ro);
    //Serial.println(finalValue, DEC);
  #endif

  delay(100);// wait 100ms for next reading

  return finalValue;
}
/**
 * Reads pm10 and pm2.5 concentration from Shinyei PPD42,, this function is
 * based on the dustduino project
 */
void pmRead(){
  #if defined(DEVMODE)
  Serial.println(F("Started  PM read"));
  #endif

  unsigned long triggerOnP10, triggerOffP10, pulseLengthP10, durationP10;
  boolean P10 = HIGH, triggerP10 = false;
  unsigned long triggerOnP25, triggerOffP25, pulseLengthP25, durationP25;
  boolean P25 = HIGH, triggerP25 = false;
  float ratioP10 = 0, ratioP25 = 0;
  unsigned long sampletime_ms = 30000;
  float countP10, countP25;
  unsigned long starttime=millis();

  for( ;sampletime_ms > millis() - starttime; ){
    P10 = digitalRead(9);
    P25 = digitalRead(8);
    if(P10 == LOW && triggerP10 == false){
      triggerP10 = true;
      triggerOnP10 = micros();
    }
    if (P10 == HIGH && triggerP10 == true){
        triggerOffP10 = micros();
        pulseLengthP10 = triggerOffP10 - triggerOnP10;
        durationP10 = durationP10 + pulseLengthP10;
        triggerP10 = false;
    }
    if(P25 == LOW && triggerP25 == false){
      triggerP25 = true;
      triggerOnP25 = micros();
    }
    if (P25 == HIGH && triggerP25 == true){
      triggerOffP25 = micros();
      pulseLengthP25 = triggerOffP25 - triggerOnP25;
      durationP25 = durationP25 + pulseLengthP25;
      triggerP25 = false;
    }
  }
  ratioP10 = durationP10/(sampletime_ms*10.0);  // Integer percentage 0=>100
  ratioP25 = durationP25/(sampletime_ms*10.0);
  countP10 = 1.1*pow(ratioP10,3)-3.8*pow(ratioP10,2)+520*ratioP10+0.62;
  countP25 = 1.1*pow(ratioP25,3)-3.8*pow(ratioP25,2)+520*ratioP25+0.62;
  float PM10count = countP10; ////confirmmm!!!
  float PM25count = countP25 - countP10;

  // first, PM10 count to mass concentration conversion
  double r10 = 2.6*pow(10,-6);
  double pi = 3.14159;
  double vol10 = (4/3)*pi*pow(r10,3);
  double density = 1.65*pow(10,12);
  double mass10 = density*vol10;
  double K = 3531.5;
  float concLarge = (PM10count)*K*mass10;

  // next, PM2.5 count to mass concentration conversion
  double r25 = 0.44*pow(10,-6);
  double vol25 = (4/3)*pi*pow(r25,3);
  double mass25 = density*vol25;
  float concSmall = (PM25count)*K*mass25;

  pm10 = concLarge;
  pm25 = concSmall;

  #if defined(DEVMODE)
  Serial.print(F("  PM 10: "));
  Serial.println(pm10);
  Serial.print(F("  PM 2.5: "));
  Serial.println(pm25);
  #endif
}
/**
 * Reads pressure from BMP pressure Sensor
 * @return pressure
 */
float pressureRead(){
 char state;
 double T,P,p0,a;
  // Loop here getting pressure readings every 10 seconds.
  state = bmp.startTemperature();
  if (state != 0)
  {
    delay(state);
    state = bmp.getTemperature(T);
    if (state != 0)
    {
      state = bmp.startPressure(3);
      if (state != 0)
      {
        delay(state);
        state = bmp.getPressure(P,T);
        if (state != 0)
        {
          // Print out the measurement:
          //Serial.print("absolute pressure: ");
          //Serial.print(P*0.750061561303,2);
          //Serial.println(" mmHg");
          return P;
        }
        #if defined(DEVMODE)
        else Serial.println(F("error retrieving pressure measurement\n"));
        #endif defined(DEVMODE)
      }
      #if defined(DEVMODE)
      else Serial.println(F("error starting pressure measurement\n"));
      #endif
    }
    #if defined(DEVMODE)
    else Serial.println(F("error retrieving temperature measurement\n"));
    #endif
  }
  #if defined(DEVMODE)
  else Serial.println(F("error starting temperature measurement\n"));
  #endif
}
/**
 * Reads the Luminosity sensor TSL2561 and calculates the Lux units
 */
float lightRead(){
  boolean gain;     // Gain setting, 0 = X1, 1 = X16;
  unsigned int ms;  // Integration ("shutter") time in milliseconds
  unsigned char time = 2;
  light.setTiming(gain,time,ms);
  light.setPowerUp();
  delay(ms);
  unsigned int data0, data1;

  if (light.getData(data0,data1))
  {
    // getData() returned true, communication was successful

    //    Serial.print("data0: ");
    //    Serial.print(data0);
    //    Serial.print(" data1: ");
    //    Serial.print(data1);

    // To calculate lux, pass all your settings and readings
    // to the getLux() function.

    // The getLux() function will return 1 if the calculation
    // was successful, or 0 if one or both of the sensors was
    // saturated (too much light). If this happens, you can
    // reduce the integration time and/or gain.
    // For more information see the hookup guide at: https://learn.sparkfun.com/tutorials/getting-started-with-the-tsl2561-luminosity-sensor

    double lux;    // Resulting lux value
    boolean good;  // True if neither sensor is saturated

    // Perform lux calculation:

    good = light.getLux(gain,ms,data0,data1,lux);

    if (good) l=lux; else l=-1;

    return l;
  }
}
/**
 * Reads Temperature from DHT
 */
float temperatureRead(){
  return dht.readTemperature();
}
/**
 * Reads Temperature from DHT
 */
float humidityRead(){
  return dht.readHumidity();

}
/**
 * Convert binary coded decimal to normal decimal numbers
 * @param  val byte value to be converteted
 * @return     Resulting DEC Value
 */
byte bcdToDec(byte val)  {
  return ( (val/16*10) + (val%16) );
}
byte decToBcd(byte val)
{
  return ( (val/10*16) + (val%10) );
}
/**
 * This code Get the date from DS1307_ADDRESS,  RTC based on http://bildr.org/2011/03/ds1307-arduino/
 * @param {int} adress Adress of DS1307 real time clock
 */
void getDate(int adress){
  #if defined(DEVMODE)
  Serial.print(F("Getting Date: "));
  #endif

  // Reset the register pointer
  Wire.beginTransmission(adress);
  byte zero = 0x00;
  Wire.write(zero);
  Wire.endTransmission();
  Wire.requestFrom(adress, 7);

  second = bcdToDec(Wire.read());
  minute = bcdToDec(Wire.read());
  hour = bcdToDec(Wire.read() & 0b111111); //24 hour time
  weekDay = bcdToDec(Wire.read()); //0-6 -> sunday - Saturday
  monthDay = bcdToDec(Wire.read());
  month = bcdToDec(Wire.read());
  year = bcdToDec(Wire.read());

  #if defined(DEVMODE)
    Serial.print(year);    Serial.print(F("-"));
    Serial.print(month);   Serial.print(F("-"));
    Serial.print(monthDay);Serial.print(F("T"));
    Serial.print(hour);    Serial.print(F(":"));
    Serial.print(minute);  Serial.print(F(":"));
    Serial.print(second);
    Serial.println(F("+5:00"));
  #endif
}
/**
 * SD card begin function
 */
void sdBegin(){
  if (!SD.begin(4)) {
    log(F("SD failed!"));warn();
    return;
  }
  log(F("SD done."));
  #if defined(DEVMODE)
  Serial.println(F("SD done"));
  #endif
}
/**
 * Meteorology read function
 */
void meteorologyRead(){
  p = pressureRead();
  l = lightRead();
  h = humidityRead();
  t = temperatureRead();
  #if defined(DEVMODE)
    Serial.print(F("  p: "));
    Serial.println(p);
    Serial.print(F("  l: "));
    Serial.println(l);
    Serial.print(F("  h: "));
    Serial.println(h);
    Serial.print(F("  t: "));
    Serial.println(t);
  #endif
}
/**
 * Setup all of the configuration from the SD to the arduair
 */
void arduairSetup(){
 #if defined(DEVMODE)
 Serial.println(F("start arduairSetup..."));
 #endif

 char character;
 String settingName;
 String settingValue;
 myFile = SD.open(F("CONFIG.txt"));
 if (myFile) {
   while (myFile.available()) {
     character = myFile.read();
       while((myFile.available()) && (character != '[')){
         character = myFile.read();
        }
      character = myFile.read();
      while((myFile.available()) && (character != '=')){
        settingName = settingName + character;
        character = myFile.read();
      }
      character = myFile.read();
      while((myFile.available()) && (character != ']')){
        settingValue = settingValue + character;
        character = myFile.read();
      }
      if(character == ']'){

      #if defined(DEVMODE)
      Serial.print(F("  "));
      Serial.print(settingName);
      Serial.print(F(": "));
      Serial.println(settingValue);
      #endif


       // Apply the value to the parameter
       applySetting(settingName,settingValue);
       // Reset Strings
       settingName = "";
       settingValue = "";
     }
   }
 // close the file:
 myFile.close();
 } else {
 // if the file didn't open, print an error:
 #if defined(DEVMODE)
 Serial.println(F("error opening settings.txt"));
 #endif

 log(F("error opening settings.txt"));
 warn();
 }
 #if defined(DEVMODE)
 Serial.println(F("End ArduairSetup"));
 #endif
}
/**
* Apply the given setting from the SD
* @param settingName  Setting to be set
* @param settingValue Value to set
*/
void applySetting(String settingName, String settingValue) {
  if (settingName==F("network")){
    settingValue.toCharArray(ssid,20);
  }
  if (settingName==F("networkpass")){
    settingValue.toCharArray(pass,20);
  }
  if (settingName==F("server")){
    settingValue.toCharArray(server,25);
  }
  if (settingName==F("device")){
    settingValue.toCharArray(device,20);
  }
  if (settingName==F("password")){
    settingValue.toCharArray(password,20);
  }
  if (settingName==F("wifi")){
    wifi==toBoolean(settingValue);
  }
  if (settingName==F("resetclock")){
    resetClock=toBoolean(settingValue);
  }
  if (settingName==F("year")){
    year=settingValue.toInt();
  }
  if (settingName==F("month")){
    month=settingValue.toInt();
  }
  if (settingName==F("day")){
    monthDay=settingValue.toInt();
  }
  if (settingName==F("hour")){
    hour=settingValue.toInt();
  }
  if (settingName==F("minute")){
    minute=settingValue.toInt();
  }
  if (settingName==F("second")){
    second=settingValue.toInt();
  }
 }

 // converting string to Float
 float toFloat(String settingValue){
 char floatbuf[settingValue.length()+1];
 settingValue.toCharArray(floatbuf, sizeof(floatbuf));
 float f = atof(floatbuf);
 return f;
 }

 long toLong(String settingValue){
 char longbuf[settingValue.length()+1];
 settingValue.toCharArray(longbuf, sizeof(longbuf));
 long l = atol(longbuf);
 return l;
 }

 // Converting String to integer and then to boolean
 // 1 = true
 // 0 = false
 boolean toBoolean(String settingValue) {
 if(settingValue.toInt()==1){
 return true;
 } else {
 return false;
 }
}
/**
 * This function begins wifi connection
 */
void wifiBegin(){
    // check for the presence of the shield:
    if (WiFi.status() == WL_NO_SHIELD) {
    #if defined(DEVMODE)
    Serial.println(F("WiFi shield not present"));
    #endif
    log(F("WiFi shield not present"));
    warn();
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv != F("1.1.0")) {
    #if defined(DEVMODE)
    Serial.println(F("Please upgrade the firmware"));
    #endif
    warn();
    log(F("Please upgrade the firmware"));
  }
  while (status != WL_CONNECTED) {
    #if defined(DEVMODE)
    Serial.print(F("Attempting to connect to SSID: "));
    Serial.println(ssid);
    #endif

    status = WiFi.begin(ssid, pass);// Connect to WPA/WPA2 network. Change this line if using open or WEP network
    delay(10000);// wait 10 seconds for connection
  }
  #if defined(DEVMODE)
  Serial.println(F("Connected to wifi"));
  #endif
}
/**
 * Disables automatic concentration of ZE sensors and flushes Serials Buffers to prevent unexpectects behaviors from interrupts
 */
void winsenBegin(){

  Serial1.begin(9600); //ZE CO-sensor
  Serial2.begin(9600); //ZE NO2-sensor
  Serial3.begin(9600); //ZE SO2-sensor

  byte message[] = {0xFF,0x01, 0x78, 0x04, 0x00, 0x00, 0x00, 0x00, 0x83};//TODO: change bye array to "manual form"
  Serial1.write(message,sizeof(message));
  Serial2.write(message,sizeof(message));
  Serial3.write(message,sizeof(message));
  delay(1000);//Avoid problems with sensors response
  while(Serial1.available()>0){
    byte c = Serial1.read();
  }
  while(Serial2.available()>0){
    byte c = Serial2.read();
  }
  while(Serial3.available()>0){
    byte c = Serial3.read();
  }
  delay(1000);
}
/**
 * Reads the given contaminant from their respective Winsen Sensor
 * @param cont Contaminant to be read, could be CO, NO2 or SO2
 */
void winsenRead(int cont){
  #if defined(DEVMODE)
  //Serial.println("Winsen Sensor Reading");
  #endif

  byte message[] = {0xFF,0x01, 0x78, 0x03, 0x00, 0x00, 0x00, 0x00, 0x84};
  unsigned long sampletime_ms = 30000;
  unsigned long starttime=millis();
  byte measure[8]={0x00,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  float ppm=0;
  switch (cont) {
    case 1:
      Serial1.write(message,sizeof(message));
      delay(2000);
      //for(;sampletime_ms > millis() - starttime;){
      if (Serial1.available() > 0) {
        Serial1.readBytes(measure,9);
        if (measure[0]==0xff && measure[1]==0x78){
          Serial1.readBytes(measure,9);
        }
        if (measure[0]==0xff && measure[1]==0x86){
          ppm = measure[2]*256+measure[3];
          co=ppm;
          #if defined(DEVMODE)
          Serial.print(F("  [CO]:  "));
          Serial.println(ppm);
          #endif

        }else{
          co=-1;
        }
      }
      break;
    case 2:
      Serial2.write(message,sizeof(message));
      delay(2000);
      //for(;sampletime_ms > millis() - starttime;){
      if (Serial2.available() > 0) {
        Serial2.readBytes(measure,9);
        if (measure[0]==0xff && measure[1]==0x78){
          Serial2.readBytes(measure,9);
        }
        if (measure[0]==0xff && measure[1]==0x86){
          ppm = measure[2]*256+measure[3];
          no2==ppm;

          #if defined(DEVMODE)
          Serial.print(F("  [NO2]: "));
          Serial.println(ppm);
          #endif
        }else{
          no2=-1;
        }
      }
      break;
    case 3:
    Serial3.write(message,sizeof(message));
    delay(2000);
    //for(;sampletime_ms > millis() - starttime;){
    if (Serial3.available() > 0) {
      Serial3.readBytes(measure,9);
      if (measure[0]==0xff && measure[1]==0x78){
        Serial3.readBytes(measure,9);
      }
      if (measure[0]==0xff && measure[1]==0x86){
        ppm = measure[2]*256+measure[3];
        so2=ppm;

        #if defined(DEVMODE)
        Serial.print(F("  [SO2]: "));
        Serial.println(ppm);
        #endif
      }else{
        so2=-1;
      }
    }
    break;
  }
  winsenBegin(); //disable sensors.
}
/**
 * Perform a simple requesto
 */
void simple_request(){
  // close any connection before send a new request.
  // This will free the socket on the WiFi shield
   client.stop();
   String timezone;
  // if there's a successful connection:
  if (client.connect(server, 80)) {
    #if defined(DEVMODE)
    Serial.println(F("connecting..."));
    #endif

    //String getRequest ="GET"+"hola"+" "
    // send the HTTP GET request:
    client.print(F("GET "));
    client.print(F("/api"));
    client.print(F("/")); client.print(device);
    client.print(F("/")); client.print(password); client.print("/timezone"); client.print(" HTTP/1.1");
    client.println(F(""));
    // parameters:
    client.print(F("?z=")); client.print(timezone); client.print(F(","));
    client.print(F("h=")); client.print(h); client.print(F(","));
    client.print(F("t=")); client.print(t); client.print(F(","));
    client.print(F("l=")); client.print(l); client.print(F(","));
    client.print(F("co=")); client.print(pm10); client.print(F(","));
    client.print(F("o3=")); client.print(pm10); client.print(F(","));
    client.print(F("pm10=")); client.print(pm10); client.print(F(","));
    client.print(F("pm25=")); client.print(pm25); client.print(F(","));
    client.print(F("so2=")); client.print(so2); client.print(F(","));
    client.print(F("no2=")); client.print(no2); client.print(F(","));
    client.println(F(""));
    //server
    client.print(F("Host: "));client.print(server);
    client.println(F("User-Agent: Arduair"));
    client.println(F("Connection: close"));
    client.println();
  }
 }
/**
 * request config file from server and update it
 */
void requestConfig(){
  String config;
  #if defined(DEVMODE)
  Serial.println(F("Resquesting Config file"));
  #endif
  client.stop();
  Serial.println(device);
  Serial.println(password);
  Serial.println(server);
  if (client.connect(server, 80)) {
    client.print(F("GET "));
    client.print(F("/api"));
    client.print(F("/")); client.print(device);
    client.print(F("/")); client.print(password);
    client.print(F("/config"));
    //http GET end
    client.print(F(" HTTP/1.1"));
    client.println(F(""));
    client.println(F(""));
    //server
    client.print(F("Host: "));client.print(server);
    client.println(F("User-Agent: Arduair"));
       client.println(F("Connection: close"));
    #if defined(DEVMODE)
    Serial.println(F("Request done"));
    #endif
  } else {
    #if defined(DEVMODE)
    Serial.println(F("connection failed"));
    #endif
    warn();
  }
  int timeout = millis() + 20000;
  while (client.available() == 0) {
   if (timeout - millis() < 0) {
     #if defined(DEVMODE)
     Serial.println(F("client Timeout !"));
     #endif
     client.stop();
     warn();
     log(F("client Timeout !"));
   }
 }
 while(client.available()) {
   config=client.readStringUntil('\r');
   #if defined(DEVMODE)
     Serial.println(config);
   #endif
 }
 //write config
  SD.remove(F("CONFIG.txt"));
  myFile = SD.open(F("CONFIG.txt"), FILE_WRITE); //open SD data.txt file
  if (myFile){
    myFile.print(config);
    myFile.close();
  }
  arduairSetup();
}
/**
 * Get time config from Server
 */
void timeConfig(){
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(zero);

  Wire.write(decToBcd(second));
  Wire.write(decToBcd(minute));
  Wire.write(decToBcd(hour));
  Wire.write(decToBcd(weekDay));
  Wire.write(decToBcd(monthDay));
  Wire.write(decToBcd(month));
  Wire.write(decToBcd(year));

  Wire.write(zero); //start

  Wire.endTransmission();

  myFile = SD.open(F("CONFIG.txt"), FILE_WRITE);
  myFile.print(F("["));
  myFile.print(F("resetclock="));
  myFile.print(F("false"));
  myFile.println(F("]"));
  myFile.close();
  log(F("Clock updated"));
}
/**
 * Log function, it writes a message in a log file.
 * @param message Message to be
 */
void log(String message){

  myFile = SD.open(F("LOG.txt"), FILE_WRITE); //open SD data.txt file
  if (myFile){
    //write ISO date ex: 1994-11-05T08:15:30-05:00
    myFile.print(year);myFile.print(F("-"));
    myFile.print(month);myFile.print(F("-"));
    myFile.print(monthDay);myFile.print(F("T"));
    myFile.print(hour);myFile.print(F(":"));
    myFile.print(minute);myFile.print(F(":"));
    myFile.print(second);
    myFile.print(F("+5:00,   "));

    myFile.print(message);

    myFile.println(F(" "));
    myFile.close();
  }
}
/**
 * turn the warning light on
 */
void warn(){
    digitalWrite(RED_LED_PIN,HIGH);
}
