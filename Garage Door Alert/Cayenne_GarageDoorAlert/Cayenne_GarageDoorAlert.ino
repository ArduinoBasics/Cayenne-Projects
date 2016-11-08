//================================================================================================
//       Title: Garage Door Alert Project using Cayenne and a Magnetometer (HMC5883L on GY-80)
//      Author: ScottC,  http://arduinobasics.blogspot.com
//     Version: 1.0
//        Date: 8th Nov 2016
// Arduino IDE: 1.6.9
// Arduino Board: Seeeduino Cloud  (or Arduino Yun)
//                
// Description: The following sketch (in combination with Cayenne) will alert you when the
//              direction of the magnetometer has changed by a pre-determined amount.
//              The amount is defined by a "threshold" value. 
//              You have the option to enable or disable your monitoring of this sensor with
//              a master switch. This option can be controlled remotely.
//              You can also request a calibration of the sensor from the Cayenne app on the
//              internet or on your smartphone.
//
//Libraries Required:
//              Cayenne Library: is available directly from the Arduino IDE Libraries list. 
//              To install the library, select Sketch > Include library > Manage Libraries. 
//              The Library Manager dialog will appear. From here, search for the Cayenne library and install it.
//
//              Wire Library: inbuilt into the Arduino IDE. No download or installation required.
//================================================================================================

//Libraries
#include <CayenneYun.h>       //Install Cayenne Library as described above. 
#include <Wire.h>             //Required for I2C communication with the Magnetometer

//Global variables
#define address 0x1E          //I2C address of the Magnetometer
int x,y,z;                    //Variables used to hold the triple axis data
int xCal=0, yCal=0, zCal=0;   //Variables used to calibrate the triple axis data.
int calValue = 1000;          //All axis variables will equal this number upon calibration request.
int threshold = 50;           //Alert when ANY axis changes by more than 50.
int doorState = 0;            //The state of the door.  0=closed,  1=open.
int calEnabled = 0;           //When equal to 1, this will calibrate or change all axis variables to 1000 (i.e. the calValue)
int masterSwitch = 0;         //This will turn "monitoring" on/off. 0=off, 1=on.

// Cayenne authentication token. This should be obtained from the Cayenne Dashboard. This identifies this particular Arduino from all others.
char token[] = "l6tqqd7qee";

//=====================================================================================================
// setup(): 
//    This runs once only. 
//    Setup I2C communication. 
//    Configure the Magnetometer to use 8 samples (averaged) per measurement output. (Register A)
//    Set the gain to +/- 2.5 Ga, which changes the digital resolution to 1.52 (Register B)
//    Calibrate all axis values to 1000. Which means that all axis variables will equal 1000 on startup, no matter what position the sensor is in.
//-----------------------------------------------------------------------------------------------------
void setup()
{
  //Initialise I2C communications. On Yun, SDA = D2, SCL = D3.
  Wire.begin();

  //Setup the magnetometer 
  magSetting(0x00, B01101000); // Magnetometer settings associated with Register A. See datasheet for acceptable values. 
  magSetting(0x01, B11100000); // Magnetometer settings associated with Register B. See datasheet for acceptable values. 
  calibrateMag();              // Calibrate each axis to a value of 1000

  //Begin communication with the Cayenne Server.
  Cayenne.begin(token);
}


//=====================================================================================================
// loop():
//    This code runs in an endless loop
//    getReadings(): is used to read the data for each axis and assign these values to variables x,y and z.
//    Synchronize the axis readings with the Cayenne server, and compare them against the threshold.
//    If exceeds the threshold, the doorState changes to a value of 1, indicating that the door is open.
//    A trigger setup within the Cayenne service allows you to perform an action based on the changed state of the door.
//    In this case an SMS or email notification is sent.
//    If a calibration is requested from the Cayenne app, the calibrateMag() function is called.
//    The sensor is checked once per second. Decrease the delay for more frequent readings.
//-----------------------------------------------------------------------------------------------------
void loop(){
  Cayenne.run();
  getReadings();

  if(masterSwitch){
    if(calEnabled){
      calibrateMag();
    }
    checkReadings();
    updateCayenne();
  }
  delay(1000);
}


//=====================================================================================================
// magSetting(regLoc, setting):
//    regLoc: is the register address location that we want to interact with (RegA = 0x00, RegB = 0x01, Mode = 0x02)
//    setting: is the 8bit code used to configure the magnetometer (see datasheet)
//
//    This function allows you to configure the magnetometer to suit your specific application
//    A delay of 10 is used, but anything above 7 is recommended.
//-----------------------------------------------------------------------------------------------------
void magSetting(byte regLoc, byte setting){
  Wire.beginTransmission(address);
  Wire.write(regLoc); 
  Wire.write(setting);
  Wire.endTransmission();
  delay(10);
}


//=====================================================================================================
// getReadings():
//    It is necessary to send the bytes (0x02 and 0x01) to instruct the magnetometer to prepare for single measurement mode
//    Request 6 bytes from the magnetometer and then read 2 bytes per axis (assigned to variables x, y and z).
//    The xCal, yCal, and zCal variables adjust the magnetometer's readings relative to it's initial state.
//    The magnetometer should send back 6 bytes of positional data.
//-----------------------------------------------------------------------------------------------------
void getReadings(){
  magSetting(0x02, 0x01);        //prepare to take reading (Single measurement mode) - this populates the registers with data
  Wire.requestFrom(address, 6);  //Request 6 bytes. Each axis uses 2 bytes.
  if (Wire.available()>5){
     x = readValue()- xCal;
     z = readValue()- zCal;
     y = readValue()- yCal;    
  } else {
    //Error exists
  }
}


//=====================================================================================================
// readValue():
//    This reads the magnetometer's data registers - 2 bytes at a time.
//    All 6 data registers must be read properly before new data can be placed into any of these data registers
//    This function reads two bytes (8bit + 8bit) and joins them together to make a 16bit integer value.
//    This value is returned and assigned to one of the axis variables x, y and/or z.
//    With every read() called, the data register pointer is incremented.
//    When magSetting(0x02, 0x01) is called in the getReadings() function, the pointer is reset to the first data register (0x03).
//-----------------------------------------------------------------------------------------------------
int readValue(){
  int val = Wire.read()<<8; 
      val |= Wire.read();

  return val;
}


//=====================================================================================================
// calibrateMag():
//    This function is used to calibrate or set each axis value to calValue.
//    calValue is defined in the global variables section (=1000)
//    After calibration, each axis variable (x, y and z) will equal the calValue, no matter what position the magnetometer is in.
//-----------------------------------------------------------------------------------------------------
void calibrateMag(){
  xCal=0, yCal=0, zCal=0;
  getReadings();              // update values x,y,z with new data
  xCal = x-calValue;
  yCal = y-calValue;
  zCal = z-calValue;
  
  calEnabled=0;               // reset this variable to zero to prevent multiple calibations per request.
  Cayenne.virtualWrite(V4, calEnabled);  // notify the change in the state of this variable to Cayenne.
}


//=====================================================================================================
// checkReadings():
//    Compare the axis values against the threshold limits
//    If ANY axis changes by more that the threshold value - the doorState will change to a 1 (open)
//    If within the threshold limits, the doorState will change/remain as 0 (closed)
//-----------------------------------------------------------------------------------------------------
void checkReadings(){
  if(x<(calValue-threshold)||y<(calValue-threshold)||z<(calValue-threshold)||x>(calValue+threshold)||y>(calValue+threshold)||z>(calValue+threshold)){
    doorState = 1;  //Door open
  }else {
    doorState = 0;  //Door closed
  }
}



//=====================================================================================================
// CAYENNE_IN(V4):
//    This function ensures that the value of the calEnabled variable reflects that of the Cayenne app.
//    When the button on the Cayenne app is pressed, calEnabled = 1, which triggers a calibration of the sensor.
//    This variable is represented within the Cayenne app as virtual pin 4.
//-----------------------------------------------------------------------------------------------------
CAYENNE_IN(V4){
  calEnabled = getValue.asInt();
}


//=====================================================================================================
// CAYENNE_IN(V5):
//    This function ensures that the value of the masterSwitch variable reflects that of the Cayenne app.
//    When the button on the Cayenne app is pressed, masterSwitch = 1, which starts the "monitoring process" for changes in doorState.
//    This variable is represented within the Cayenne app as virtual pin 5.
//-----------------------------------------------------------------------------------------------------
CAYENNE_IN(V5){
  masterSwitch = getValue.asInt();
}


//=====================================================================================================
// updateCayenne:
//    Synchronise all of the sensor data, including the doorState value with the Cayenne Server/app
//    This allows for a visual representation of sensor data
//-----------------------------------------------------------------------------------------------------
void updateCayenne(){
  Cayenne.virtualWrite(V0, x);
  Cayenne.virtualWrite(V1, y);
  Cayenne.virtualWrite(V2, z);
  Cayenne.virtualWrite(V3, doorState);
}

