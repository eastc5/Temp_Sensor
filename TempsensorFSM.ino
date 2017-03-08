/* 
 
 WebClientFiniteStateMachine
 
 Author: Arduinomstr/eastc5
 Date: 15-01-2012
 Version: 2.2
 Changes: 
 28-10-2011 Counters successes / failures increased in size
 31-12-2011 Updated to Arduino version 1.0
 15-01-2012 Increased size of success / failure variables
 			Increased time before reboot to survive power cuts
23-02-2015	eastc5 updated for thingspeak as pachube is pay now		
 
 Web client implementing Finite State Machine model
 Samples sensor data, sends HTTP PUT request to thingspeak
 and displays response on serial monitor displaying 
 current state using LED's
 
 Some Ethernet Shields don't initialise correctly on power up
 - fixed by bending Shield reset pin out of header and
 connecting to I/O pin on Arduino - provides hardware
 reset to the Shield 
 
 Now uses DHCP and DNS to configure Ethernet
 
 Tested on: 
 UNO with latest Atmega8U2 firmware update,
 DFRobot Ethernet Shield with reset fix,
 Arduino 1.0
 
 */
#include <SPI.h>
#include <Ethernet.h>
#include <OneWire.h>

// Thingspeak settings for your feed here...
#define thingSpeakAddress "api.thingspeak.com";
#define writeAPIKey "YOURAPIKEY";
 
// Ethernet stuff
byte mac[] = { 
  0x90, 0xA2, 0xDA, 0x0D, 0x04, 0x21 };
EthernetClient client;

// Pachube update interval 
#define UPDATE_INTERVAL 60000 // (every 60 seconds)

// States defined as I/O pins
#define STATE_IDLE 2 // Pin 2 IDLE
#define STATE_SAMPLING 3 // Pin 3 SAMPLING
#define STATE_CONNECTING 4 // Pin 4 CONNECTING
#define STATE_FAILING 5 // Pin 5 FAILING!!
#define STATE_RECEIVING 6 // Pin 6 RECEIVING
#define GOOD_LED 2      // Everythings normal LED
#define BAD_LED 5	// Problem LED

#define RESET 8 // Ethernet reset pin 8

// Events
#define EVENT_IDLE 2
#define EVENT_SAMPLE 3
#define EVENT_CONNECT 4
#define EVENT_FAIL 5
#define EVENT_RECEIVE 6
#define EVENT_RECOVER 7
#define EVENT_DISCONNECT 8

// OneWire Sensor stuff
#define ONEWIRE_BUS 7 // OneWire Bus pin 7
#define NUM_SENS 1 // Number of sensors

OneWire ds(ONEWIRE_BUS); // Construct OneWire bus

// 8 byte Sensor addresses obtained from OneWire example....
static byte sensors[NUM_SENS][8] = { 
  0x28, 0x80, 0xB3, 0x49, 0x3, 0x0, 0x0, 0x62
  
    // Add more sensors here...
};

String pachube_str;	// Stores Pachube string

// Counters
unsigned long successes = 0; // Number of successful connections
unsigned long failures = 0; // Number of failed connections

// Initial state / event
byte state = STATE_IDLE;
byte event = EVENT_IDLE;

int flag = 0; // Problem indicator

/*******************************************************************/
/* SETUP                                                           */
/* Runs once to initialise I/O pins and Ethernet stuff             */
/*******************************************************************/
void setup() {
  // Setup I/O pins corresponding to states
  pinMode(GOOD_LED, OUTPUT);
  pinMode(BAD_LED, OUTPUT);
  pinMode(RESET, OUTPUT);
  
  delay(10000); // allow router to connect

  // Ethernet Shield reset fix
  /////////////////////////////////////////////////////////
  digitalWrite(RESET, LOW);	// Take reset line low
  delay(200);
  digitalWrite(RESET, HIGH); // Take reset line high again
  /////////////////////////////////////////////////////////
  // End of fix

  delay(2000);	// Allow Shield time to boot

  Serial.begin(9600);  
  if (Ethernet.begin(mac) == 0){
    Serial.println("Failed to configure Ethernet using DHCP");
    flag = 1;
    show_state();
    while(true); // no point going on, do nothing forever
  }  
  delay(1000);

} // End setup


/*******************************************************************/
/* MAIN LOOP                                                       */
/* Runs for ever - Displays system state before running FSM        */
/*******************************************************************/
void loop() {
  show_state();
  check_state();
  Serial.println(successes);
  Serial.println(failures);
} // End loop

/*******************************************************************/
/* FINITE STATE MACHINE                                            */
/* Implements the Finite State M/C - Checks current state and      */
/* event to determine next state transition followed by            */
/* corresponding action. Actions trigger next event and so on.     */
/* System waits a set interval before starting each cycle by       */
/* triggering SAMPLE events. If buffered data is available RECEIVE */
/* events occurs and data is read. Connection errors trigger FAIL  */
/* events which clean-up before returning to IDLE state to recover */
/* from network problems                                           */
/*******************************************************************/
void check_state() {
  switch( state )  {
  case STATE_IDLE:
    Serial.println("IDLE STATE...");    
    switch( event ) {

    case EVENT_IDLE:
      Serial.println("IDLE EVENT...");
      state = STATE_IDLE;          
      action_idle();
      break;      

    case EVENT_SAMPLE:
      Serial.println("SAMPLE EVENT...");
      state = STATE_SAMPLING;          
      action_sample();
      break;

    case EVENT_RECEIVE:
      Serial.println("RECEIVE EVENT...");
      state = STATE_RECEIVING;          
      action_receive();
      break;
    }
    break; // End IDLE state

  case STATE_SAMPLING:
    Serial.println("SAMPLING STATE...");    
    switch( event ) {
    case EVENT_CONNECT:
      Serial.println("CONNECT EVENT...");
      state = STATE_CONNECTING;
      action_connect();
      break;
    }
    break; // End SAMPLING state

  case STATE_CONNECTING:
    Serial.println("CONNECTING STATE...");    
    switch( event ) {

    case EVENT_RECEIVE:
      Serial.println("RECEIVE EVENT...");
      state = STATE_RECEIVING;          
      action_receive();
      break;

    case EVENT_FAIL:
      Serial.println("FAIL EVENT...");
      state = STATE_FAILING;          
      action_fail();
      break;
    }
    break; // End CONNECTING state

  case STATE_FAILING:
    Serial.println("FAILING STATE...");    
    switch( event ) {
    case EVENT_RECOVER:
      Serial.println("RECOVER EVENT...");
      state = STATE_IDLE;          
      action_recover();
      break;
    }
    break; // End FAILING state

  case STATE_RECEIVING:
    Serial.println("RECEIVING STATE...");    
    switch( event ) {
    case EVENT_DISCONNECT:
      Serial.println("DISCONNECT EVENT...");
      state = STATE_IDLE;          
      action_disconnect();
      break;
    }
    break;  // End RECEIVING state   

  } // End outer switch
} // End check_state

/*******************************************************************/
/* SHOW STATE                                                      */
/* Controls I/O pins according to current state                    */
/*******************************************************************/
void show_state() {
  if (!flag) {
    flashLED(GOOD_LED);
  }
  else {
    flashLED(BAD_LED);
  }
}

/*******************************************************************/
/* FLASH LED                                                       */
/* Flash given LED, tuning otherone off                            */
/*******************************************************************/
void flashLED(int LED){
    digitalWrite(BAD_LED, LOW);// everythings OK
    digitalWrite(GOOD_LED, LOW);
    delay(1000);
    digitalWrite(LED, HIGH);
    delay(1000);  
}

/*
  WebClientFiniteStateMachine
  
  Author: Arduinomstr
  Date: 31-12-2011
  Version: 2.1
  
  Finite State Machine Actions
*/  

  static unsigned long previousMillis = 0;

/*******************************************************************/
/* IDLE ACTION                                                     */
/* Keeps firing IDLE events unless either buffer has data when     */
/* RECEIVE events occur or interval has elapsed causing SAMPLE     */
/* events to fire                                                  */
/*******************************************************************/
void action_idle() {
  Serial.println("IDLE ACTION...");
  
  unsigned long currentMillis = millis();
  
  event = EVENT_IDLE; // Default

  // Check for millis overflow and reset
  if (currentMillis < previousMillis) previousMillis = 0; 

  if (client.available()) {
    event = EVENT_RECEIVE;
  } else {  
    // Delay for interval without blocking
    if(currentMillis - previousMillis > UPDATE_INTERVAL) {
      previousMillis = currentMillis;  // Update comparison
      event = EVENT_SAMPLE;
    } 
  }
}

/*******************************************************************/
/* SAMPLE  ACTION                                                  */
/* Always fires CONNECT events - All sensor sampling goes in here  */
/*******************************************************************/
void action_sample() {
  Serial.println("SAMPLE ACTION...");  
  event = EVENT_CONNECT; // Default
 
  sampleSensors(); // get readings and generate pachube string
  
}

/*******************************************************************/
/* CONNECT ACTION                                                  */
/* Fires FAIL events by default unless successful connection is    */
/* established with server when http PUT is requested sending      */
/* sensor readings - updates counter with each success before      */
/* firing RECEIVE events                                           */
/*******************************************************************/
void action_connect()
{
  Serial.println("CONNECT ACTION...");
  
  event = EVENT_FAIL; // Default

 /* if (client.connect("api.cosm.com", 80)) { // new connect method
    client.print("PUT /v2/feeds/"); // Construct PUT request
    client.print(PACHUBE_FEED_ID);
    client.println(".csv HTTP/1.1"); // Use csv format
    client.println("Host: api.cosm.com");
    client.print("X-ApiKey: ");
    client.println(PACHUBE_API_KEY);
    client.print("User-Agent: ");
    client.println(USERAGENT);
    client.print("Content-Length: ");
    client.println(pachube_str.length(), DEC); // Request length    
    client.println("Content-Type: text/csv");
    client.println("Connection: close");    
    client.println();
    client.print("sensor1,");
    client.println(pachube_str); // Add in temperature readings      
    Serial.println("Connected...");
    flag = 0; // Everythings OK again
    successes++;  // Update success counter
    event = EVENT_RECEIVE;
    } 
 
*/
if (client.connect("api.thingspeak.com", 80))
  {         
    
    client.print("POST /update? HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: YOURAPIKEY \n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    //client.print("Content-Length: ");
    //client.print(pachube_str.length());
    client.print("\n");

    //client.print("api_Key=YOURAPIKEY&");
    client.print("field1=");
    client.print(pachube_str);
    client.print("\n");
    
        Serial.println("Connected...");
    flag = 0; // Everythings OK again
    successes++;  // Update success counter
    event = EVENT_RECEIVE;
    } 
}

/*******************************************************************/
/* FAIL ACTION                                                     */
/* Always fires RECOVER events                                     */
/* Clears buffer and disconnects client on system failure and      */
/* updates failure counter                                         */
/*******************************************************************/
void action_fail() {
  Serial.println("FAIL ACTION...");

  event = EVENT_RECOVER; // Default
  
  if (client.connected()) {
    Serial.println("Failed, still connected...");
  } else Serial.println("Failed to connect..."); 
  flag = 1; // Problems
  failures++; // Update failure counter
  client.flush();
  client.stop();  
}

/*******************************************************************/
/* RECOVER ACTION                                                  */
/* Always just fires IDLE events - supports separate FAILING state */
/*******************************************************************/
void action_recover() {
  Serial.println("RECOVER ACTION...");  

  event = EVENT_IDLE; // Default

  // Do nothing
}

/*******************************************************************/
/* RECEIVE ACTION                                                  */
/* Always fires DISCONNECT events                                  */
/* Reads data until buffer empties - outputs response to serial    */
/* monitor for debugging                                           */
/*******************************************************************/
void action_receive() {
  Serial.println("RECEIVE ACTION...");   

  event = EVENT_DISCONNECT; // Default

  while (client.available()){
    char c = client.read();
    Serial.print(c);
  }
}

/*******************************************************************/
/* DISCONNECT ACTION                                               */
/* Always fires IDLE events                                        */
/* Disconnects client when connection closes on completion of http */
/* response                                                        */
/* Note - client is considered connected if connection has been    */
/* closed but there is still unread data in buffer.                */
/*******************************************************************/
void action_disconnect() {
  Serial.println("DISCONNECT ACTION...");   

  event = EVENT_IDLE;
  
  if (!client.connected()) {
    client.stop();
    Serial.println("Disconnected...");
  } else Serial.println("Still connected...");
}

/*
  WebClientFiniteStateMachine
 
 Author: Arduinomstr
 Date: 31-12-2011
 Version: 2.1
 
 Sensor helper functions
 */

/*******************************************************************/
/* GET CELSIUS TEMPERATURE                                                 */
/* Helper function - reads sensor at given address and returns     */
/* temperature in Degrees Celsius as float                         */
/*******************************************************************/
float getCTemp(byte address[8]){
  float celsius = -100.0; // (-148.0 F) Error condition
  byte present = 0;
  byte type_s;
  byte data[12];

  Serial.print("  ROM =");
  for( byte i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(address[i], HEX);
  }

  if (OneWire::crc8(address, 7) != address[7]) {
    Serial.println("CRC is not valid!");
    return celsius;
  }
  Serial.println();

  // the first ROM byte indicates which chip
  switch (address[0]) {
  case 0x10:
    Serial.println("  Chip = DS18S20");  // or old DS1820
    type_s = 1;
    break;
  case 0x28:
    Serial.println("  Chip = DS18B20");
    type_s = 0;
    break;
  case 0x22:
    Serial.println("  Chip = DS1822");
    type_s = 0;
    break;
  default:
    Serial.println("Device is not a DS18x20 family device.");
    return celsius;
  } 

  ds.reset();
  ds.select(address);
  ds.write(0x44,1);         // start conversion, with parasite power on at the end

  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.

  present = ds.reset();
  ds.select(address);    
  ds.write(0xBE);         // Read Scratchpad

  Serial.print("  Data = ");
  Serial.print(present,HEX);
  Serial.print(" ");
  for (byte i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.print(" CRC=");
  Serial.print(OneWire::crc8(data, 8), HEX);
  Serial.println();

  // convert the data to actual temperature

  unsigned int raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // count remain gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } 
  else {
    byte cfg = (data[4] & 0x60);
    if (cfg == 0x00) raw = raw << 3;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw << 2; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw << 1; // 11 bit res, 375 ms
    // default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;

  if (celsius > 1000.0) { //Error condition
    celsius = -100.0;
  }
  return celsius;
}


/*******************************************************************/
/* sampleSensors                                                   */
/* Helper function - samples sensors and builds pachube string     */
/*******************************************************************/
void sampleSensors() {
  // Do sampling here . . .
  pachube_str = ""; // reset string
  for(byte i = 0; i < NUM_SENS; i++) {
    char strbuff[10];
    float temp = getCTemp(sensors[i]);
    dtostrf (temp, 3, 2, strbuff); // Read sensors
    Serial.println(strbuff);

    pachube_str += String(strbuff);
    //pachube_str += ",";
  }
  //pachube_str += (successes + 1);	// Append successful attempts
  //pachube_str += ",";  
  //pachube_str += failures;	// Append failed attempts  
  Serial.println(pachube_str.length(), DEC); // Request length
  Serial.println(pachube_str); // Print data string   
}


