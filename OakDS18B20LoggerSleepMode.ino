/*
 OAK DS18B20 temperature sensor by Rinié Romain
 Read the value of one or more DS18B20 OneWire temperature sensor plus the battery level and send the
 value to thingspeak.
 The Oak will be put into deepsleep and read the value every 5 minutes to alllow the Oak to run several
 days on a battery (a 18650 battery for exemple).
 To allow the OAK to wake up you have to connect the Reset pin to the Wake pin (pin 10) - to achieve this 
 you can use a jumper or solder the two pre-contact at the back of the Oak board (marked with "WAKE-RST").
 You can also disable the power LED by cuting the bridge at the back of the board (just under VVC, cut the
 track between the 2 pad).
 Connect the data line of the DS18B20 to pin 3 (and a 4.7kOhm resistor between VCC and pin 3).
 To disable the deepSleep mode, you have to connect PIN 5 to ground (thus allowing to reprogramm the board).

 DON'T FORGET TO REPLACE YOUR THINGSPEAK KEY AT THE END OF THIS SKETCH (7 lines from the end)
*/
#include <OneWire.h> // Using OneWire lib from the Oak package
#include <ESP8266WiFi.h>

int ledPin = 1;      // choose the pin for the status LED, 1 for the OAK
int sleepPin = 5;    //will only enter normal run when pin 5 pull down (connect pin 5 to ground for reflash/normal operation)
int DS18S20_Pin = 3; // DS18S20 Signal on pin 3

int sleepTime = 300; //300 seconds, 5 minutes deep sleep.

float tempC = 0;
int tempCasInt = 0;
int ledState = LOW; 

unsigned long previousMillis = 0;
const long interval = 300000; //delay between two temperature value update

uint16_t powerSource;

ADC_MODE(ADC_VCC);
OneWire ds(DS18S20_Pin);
WiFiClient client;

String getMultipleTemp(){
  // Returns the temperature from a single 1-Wire DS18S20 in DEG Fahrenheit

  
  byte addr[8];
  String result = ""; //("field1=" + temperature + "&field2=" + voltage 
  int actualFieldIndex = 2; //field 1 is reserved for voltage reading
  delay(1000);
  
  while(ds.search(addr)) {
    if ( OneWire::crc8( addr, 7) != addr[7]) {
      //Serial.println("CRC is not valid!");
      Particle.publish("DS18B20 Read", "CRC is not valid, ignoring device.", 60, PRIVATE);
    } else if ( addr[0] != 0x10 && addr[0] != 0x28) {
      Particle.publish("DS18B20 Read", "Device is not recognized, ignoring device.", 60, PRIVATE);
    } else {
      ds.reset();
      ds.select(addr);
      ds.write(0x44,1); // start conversion, with parasite power on at the end
    
      byte present = ds.reset();
      ds.select(addr);    
      ds.write(0xBE); // Read Scratchpad

      byte data[12];
      for (int i = 0; i < 9; i++) { // we need 9 bytes
        data[i] = ds.read();
      }
      
      byte MSB = data[1];
      byte LSB = data[0];
    
      float tempRead = ((MSB << 8) | LSB); //using two's compliment
      tempC = tempRead / 16;
      tempCasInt = (int) tempC;
      String readTempAsString = String(tempC, DEC);
      result = result + "&field" + String(actualFieldIndex, DEC) + "=" + readTempAsString;
      actualFieldIndex = actualFieldIndex + 1;
    }
  }
  if (sizeof(result) == 0) {
      Particle.publish("DS18B20 Read", "No valide device or error.", 60, PRIVATE);
  }
  ds.reset_search();
  return result;
} //END getTemp()


void setup() {
  pinMode(sleepPin, INPUT_PULLUP); // Sleep mode input set as pullup to default in sleep mode
  pinMode(ledPin, OUTPUT);         // Initialize the BUILTIN_LED pin as an output
  digitalWrite(ledPin, HIGH);      // Turn on onboard led
  if (!ds.reset()) {
      Particle.publish("Oak Setup", "DS18B20 not found", 60, PRIVATE);
  } else {
      Particle.publish("Oak Setup", "DS18B20 present", 60, PRIVATE);
      getMultipleTemp(); //ignore first reading, or 80 is isualy read
      delay(1000);
      String tempAsString = getMultipleTemp();
      uint16_t powerSource = ESP.getVcc();
      sendToThingspeak(tempAsString, String(powerSource, DEC)); 
  }
  digitalWrite(ledPin, LOW); //Turn off onboard led to indicate end of deepSleep operation
  if (digitalRead(sleepPin) == HIGH) { //Pin sleepPin not connected to ground, going to sleep
     Particle.publish("Oak Setup", "Entering Deep Sleep", 60, PRIVATE);
     ESP.deepSleep(sleepTime*1000000, WAKE_RF_DEFAULT); // Sleep
  } else {
       Particle.publish("Oak Setup","Entering Non Sleep Mode",60,PRIVATE);
  }
}


//Time storage is preffered over pause function to create a delay, avoiding by this way to slow down the rest of the execution
// the loop function runs over and over again forever
void loop() {
  if (previousMillis == 0) {
    previousMillis = millis();
    getMultipleTemp();
    delay(1000);
    String tempAsString = getMultipleTemp();
    uint16_t powerSource = ESP.getVcc();
    sendToThingspeak(tempAsString, String(powerSource, DEC));
  }
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    String tempAsString = getMultipleTemp();
    powerSource = ESP.getVcc();
    sendToThingspeak(tempAsString, String(powerSource, DEC));

    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }
    // set the LED with the ledState of the variable:
    digitalWrite(ledPin, ledState);
  }
}

char thingSpeakAddress[] = "api.thingspeak.com";

void sendToThingspeak(String temperatureFields, String voltage) {
 // updateThingSpeak("field1=" + temperatureFields + "&field2=" + voltage);
  updateThingSpeak("field1=" + voltage + temperatureFields);
  return;
}


void updateThingSpeak(String tsData) {
  if ( !client.connect(thingSpeakAddress, 80) ) {
    return;
  }
  client.println("GET /update?key=XXXXXXXXXXXXXXXX&" + tsData + " HTTP/1.1");
  client.print("Host: ");
  client.println(thingSpeakAddress);
  client.println("Connection: close");
  client.println();
  return;
}



