#include <SD.h>
#include <Wire.h>
#include "RTClib.h"

// A simple data logger for the Arduino analog pins

// how many milliseconds between grabbing data and logging it. 1000 ms is once a second
#define LOG_INTERVAL  1000 // mills between entries (reduce to take more/faster data)

// how many milliseconds before writing the logged data permanently to disk
// set it to the LOG_INTERVAL to write each time (safest)
// set it to 10*LOG_INTERVAL to write all data every 10 datareads, you could lose up to 
// the last 10 reads if power is lost but it uses less power and is much faster!
#define SYNC_INTERVAL 10000 // mills between calls to flush() - to write data to the card

// The analog pins that connect to the sensors
#define tempSensorPin 0           // analog 0
#define pHSensorPin 1                // analog 1

//Added by jaime
#define WHAT_ANALOG_REFERENCE DEFAULT
//Note on the Choices:
//Configures the reference voltage used for analog input (i.e. the value used as the top of the input range). The options are:
//DEFAULT: the default analog reference of 5 volts (on 5V Arduino boards) or 3.3 volts (on 3.3V Arduino boards)
//INTERNAL: an built-in reference, equal to 1.1 volts on the ATmega168 or ATmega328 and 2.56 volts on the ATmega8 (not available on the Arduino Mega)
//INTERNAL1V1: a built-in 1.1V reference (Arduino Mega only)
//INTERNAL2V56: a built-in 2.56V reference (Arduino Mega only)
//EXTERNAL: the voltage applied to the AREF pin (0 to 5V only) is used as the reference.

/////////////////////////////////////////////////

uint32_t syncTime = 0; // time of last sync() (prob don't change this)

#define ECHO_TO_SERIAL   1 // echo data to serial port
//(if you make this zero, it will only write to card instead of also Serial.print ... but it's ok to
// leave it as 1 all the time, even without a computer plugged in.)

#define WAIT_TO_START    0 // Wait for serial input in setup()
// IMO should leave this as zero all the time. Why would you want to freeze the program while waiting for
// a serial connection?


//Jaime says: I think these two lines have to do with the Arduino trying to measure its own supply voltage.
// the idea is that you can compensate for the effect on your readings that comes from
// fluctuations in the supply voltage. Probably refer to Adafruit on how exactly to do that.
#define BANDGAPREF 14            // special indicator that we want to measure the bandgap
#define bandgap_voltage 1.1      // this is not super guaranteed but its not -too- off

RTC_DS1307 RTC; // define the Real Time Clock object

// for the data logging shield, we use digital pin 10 for the SD cs line
const int chipSelect = 10;

// the logging file
File logfile;



void error(char *str)
{
  Serial.print("Stopping! error: ");
  Serial.println(str);

  while(1); // jaime: This is a stupid way (IMO) to freeze the program upon an error. 
}

void setup(void)
{
  Serial.begin(9600);
  Serial.println();
  
#if WAIT_TO_START
  Serial.println("Type any character to start");
  while (!Serial.available()); // Jaime: Freeze til serial connects
#endif //WAIT_TO_START

  // initialize the SD card
  Serial.print("Initializing SD card...");
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(10, OUTPUT);
  
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    error("Card failed, or not present");
  }
  Serial.println("card initialized.");
  
  // create a new file
  char filename[] = "LOGGER00.CSV";
  
  // jaime: NOTICE!! THIS FOR LOOP REALLY ONLY CREATES 1 FILE.
  
  for (uint8_t i = 0; i < 100; i++) { //uint8_t is a fancy integer. Unsigned, 8 bit, not sure what the "t" is.
    // So this was above my head for a while, but I just now sat down and looked at it.
    // Really, it's taking the ASCII code for '0' (something like 48) and adding i / 10. But i/10 will be
    // truncated (because... that's how C/C++ works). So for i=5,
    // i/10 = 0, 0 + '0' = '0'.
    // i%10 = 5, 5 + '0' = '5'.
    // And, for 37 (arbitrary other number),
    // i/10 = 3, 3 + '0' = '3'.
    // i%10 = 7, 7 + '0' = '7'.
    filename[6] = i/10 + '0';
    filename[7] = i%10 + '0';
    if (! SD.exists(filename)) {
      // only open a new file if it doesn't exist
      logfile = SD.open(filename, FILE_WRITE); 
      break;  // leave the loop! 
      // So -- only creating ONE FILE!
    }
  }
  
  if (! logfile) {
    error("couldnt create file");
  }
  
  Serial.print("Logging to: ");
  Serial.println(filename);

  // connect to RTC
  Wire.begin();  
  if (!RTC.begin()) {
    logfile.println("RTC failed");
#if ECHO_TO_SERIAL
    Serial.println("RTC failed");
#endif  //ECHO_TO_SERIAL
  }
  

  logfile.println("datetime,A0Raw,A1Raw,vcc,Temp,pH,AnalogRef");    
#if ECHO_TO_SERIAL
  Serial.println("datetime,A0Raw,A1Raw,vcc,Temp,pH,AnalogRef");
#endif //ECHO_TO_SERIAL
 
  // If you want to set the aref to something other than 5v
  analogReference(WHAT_ANALOG_REFERENCE);
}

void loop(void)
{
  DateTime now;

  // delay for the amount of time we want between readings
  delay((LOG_INTERVAL -1) - (millis() % LOG_INTERVAL));
  
  
  // log milliseconds since starting
  uint32_t m = millis();
//  logfile.print(m);           // milliseconds since start
//  logfile.print(", ");    
#if ECHO_TO_SERIAL
//  Serial.print(m);         // milliseconds since start
//  Serial.print(", ");  
#endif

  // fetch the time
  now = RTC.now();
  // log time
  //logfile.print(now.unixtime()); // seconds since 1/1/1970
  //logfile.print(", ");
  logfile.print('"');
  logfile.print(now.year(), DEC);
  logfile.print("/");
  logfile.print(now.month(), DEC);
  logfile.print("/");
  logfile.print(now.day(), DEC);
  logfile.print(" ");
  logfile.print(now.hour(), DEC);
  logfile.print(":");
  logfile.print(now.minute(), DEC);
  logfile.print(":");
  logfile.print(now.second(), DEC);
  logfile.print('"');
#if ECHO_TO_SERIAL
  //Serial.print(now.unixtime()); // seconds since 1/1/1970
  //Serial.print(", ");
  Serial.print('"');
  Serial.print(now.year(), DEC);
  Serial.print("/");
  Serial.print(now.month(), DEC);
  Serial.print("/");
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(":");
  Serial.print(now.minute(), DEC);
  Serial.print(":");
  Serial.print(now.second(), DEC);
  Serial.print('"');
#endif //ECHO_TO_SERIAL

  analogRead(tempSensorPin);
  delay(10); 
  int tempSensorReading = analogRead(tempSensorPin);  
  
  analogRead(pHSensorPin); 
  delay(10);
  int pHSensorReading = analogRead(pHSensorPin);    
  
  // converting that reading to voltage, for 3.3v arduino use 3.3, for 5.0, use 5.0
  // float voltage = pHSensorReading * aref_voltage / 1024;  
  // float temperatureC = (voltage - 0.5) * 100 ;
  // float temperatureF = (temperatureC * 9 / 5) + 32;
  
  logfile.print(", ");    
  logfile.print(tempSensorReading);
  logfile.print(", ");
  logfile.print(pHSensorReading);
  //logfile.print(temperatureF);
#if ECHO_TO_SERIAL
  Serial.print(", ");   
  Serial.print(tempSensorReading);
  Serial.print(", ");  
  Serial.print(pHSensorReading);
  //Serial.print(temperatureF);
#endif //ECHO_TO_SERIAL

  // Log the estimated 'VCC' voltage by measuring the internal 1.1v ref
  analogRead(BANDGAPREF); 
  delay(10);
  int refReading = analogRead(BANDGAPREF); 
  float supplyvoltage = (bandgap_voltage * 1024) / refReading; 
  
  logfile.print(", ");
  logfile.print(supplyvoltage);
#if ECHO_TO_SERIAL
  Serial.print(", ");   
  Serial.print(supplyvoltage);
#endif // ECHO_TO_SERIAL


//math here
  float voltageOftempReading = tempSensorReading /  204.6;
  float actualTemp = voltageOftempReading*0.101*100.0;  
  
  float voltageOfpHReading = pHSensorReading /  204.6;
  float actualpH = (4.6083-voltageOfpHReading)/.3;

  logfile.print(", ");
  logfile.print(actualTemp);
  
  logfile.print(", ");
  logfile.print(actualpH);

  logfile.print(", ");
  logfile.print(WHAT_ANALOG_REFERENCE);
  
#if ECHO_TO_SERIAL
  Serial.print(", ");   
  Serial.print(actualTemp);

  Serial.print(", ");   
  Serial.print(actualpH);

  Serial.print(", ");   
  Serial.print(WHAT_ANALOG_REFERENCE);

#endif // ECHO_TO_SERIAL






  logfile.println();
#if ECHO_TO_SERIAL
  Serial.println();
#endif // ECHO_TO_SERIAL


  // Now we write data to disk! Don't sync too often - requires 2048 bytes of I/O to SD card
  // which uses a bunch of power and takes time
  if ((millis() - syncTime) < SYNC_INTERVAL) return;
  syncTime = millis();
  
  //  we are syncing data to the card & updating FAT!

  logfile.flush();

  
}


