int incomingByte = 0;   // for incoming serial data

void setup() {
        Serial.begin(9600);     // opens serial port, sets data rate to 9600 bps
        Serial3.begin(9600);
        Serial.write("hello");
        delay(1000);
        Serial3.write(0xFF);
        Serial3.write(0x01);
        Serial3.write(0x78);
        Serial3.write(0x03);
        Serial3.write(0x00);
        Serial3.write(0x00);
        Serial3.write(0x00);
        Serial3.write(0x00);
        Serial3.write(0x84);
}

//void loop() {
//        // send data only when you receive data:
//        //Serial3.write(0x02);
//        if (Serial3.available() > 0) {
//                // read the incoming byte:
//                incomingByte = Serial3.read();
//                // say what you got:
//                Serial.print("I received: ");
//                Serial.println(incomingByte, DEC);
//        }
//}

void loop() {
        // send data only when you receive data:
        //Serial3.write(0x02);
        if (Serial3.available() > 0) {
                byte measure[8];
                Serial3.readBytes(measure,9);
                incomingByte = Serial3.read();
                // say what you got:
                Serial.print(measure[0],HEX);
                Serial.print(" ");    
                Serial.print(measure[1],HEX);
                Serial.print(" ");
                Serial.print(measure[2],HEX);
                Serial.print(" ");
                Serial.print(measure[3],HEX);
                Serial.print(" ");
                Serial.print(measure[4],HEX);
                Serial.print(" ");
                Serial.print(measure[5],HEX);
                Serial.print(" ");
                Serial.print(measure[6],HEX);
                Serial.print(" ");
                Serial.print(measure[7],HEX);
                Serial.print(" ");
                Serial.print(measure[8],HEX);
                int ppm = measure[2]*256+measure[3];
                Serial.print("[C]: ");
                Serial.println(ppm);
                
        }
}
