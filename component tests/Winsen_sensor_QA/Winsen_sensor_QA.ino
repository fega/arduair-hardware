/**
 * WInsen ZE03 module Q/A mode simple usage code
 * By: Fabian Enrique Gutierrez Angulo, fega.hg@gmail.com
 * March 17 2017
 */

void setup(){
  winsenBegin();
  Serial.begin(9600);
}

void loop(){
  int result = winsenRead();
  Serial.write(result);
}
/**
 * Setup the Winsen sensor
 */
void winsenBegin(){
  Serial1.begin(9600);
  byte setConfig[] = {0xFF,0x01, 0x78, 0x04, 0x00, 0x00, 0x00, 0x00, 0x83};//Set 1 Q/A configuration
  Serial1.write(setConfig,sizeof(setConfig)); //
  delay(1000);// Wait for a response
  while(Serial1.available()>0){ // Flush the response
    byte c = Serial1.read();
  }
}
/**
 * Read the sensor
 * @return {float} sensor [C]
 */
int winsenRead(){
  byte petition[] = {0xFF,0x01, 0x78, 0x03, 0x00, 0x00, 0x00, 0x00, 0x84};// Petition to get a single result
  byte measure[8]={0x00,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};// Space for the response
  float ppm;
  Serial1.write(petition,sizeof(petition));

  delay(2000);

  if (Serial1.available() > 0) {
    Serial1.readBytes(measure,9);
    if (measure[0]==0xff && measure[1]==0x78){
      Serial1.readBytes(measure,9);
    }
    if (measure[0]==0xff && measure[1]==0x86){
      ppm = measure[2]*256+measure[3];// this formula depends of the sensor is in the dataSheet

    }else{
      ppm=-1;
    }
  }
  return ppm;
}
