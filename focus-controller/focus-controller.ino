/* 
Control stepper motor (defined as 200 steps/rev). 
Control via commands in the Arduino Serial Monitor (or other serial interface).
Move to absolute position. 
Saves current position, speed to memory & loads on startup.
Does not account for physical slippage of motor or object - need to 
implement fancy hardware limit switches etc for that
170724 Add a move up move down buttons

Device commands:
move val - moves device to val
speed val - sets speed to val (valid #s 0 - 255
pos? - returns current device position
speed? - return current device speed
setzero - sets current position as zero
stepsize
stepsize?
home
setmax
setmin
poslim   
*/


#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include "utility/Adafruit_MS_PWMServoDriver.h"

#include <SoftwareSerial.h>   // We need this even if we're not using a SoftwareSerial object
                              // Due to the way the Arduino IDE compiles
#include "src/ArduinoSerialCommand/SerialCommand.h"
#include <EEPROM.h>
#define ADDRESS_POS 0
#define ADDRESS_SPEED 2
#define ADDRESS_MINPOS 3
#define ADDRESS_MAXPOS 5
#define STEP_PER_REV 200
#define MIN_STORE_CHANGE 100


// Create the motor shield object with the default I2C address
Adafruit_MotorShield AFMS = Adafruit_MotorShield(); 

// Connect a stepper motor with 200 steps per revolution (1.8 degree)
// to motor port #2 (M3 and M4)
Adafruit_StepperMotor *myMotor = AFMS.getStepper(STEP_PER_REV, 2);
int curPos,eepromPos;
byte curSpeed;
int stepSize = 5;
int  val; // Data received from the serial port
int maxPos,minPos;
SerialCommand SCmd;   // The demo SerialCommand object
const int kPinButton1 = 2;
const int kPinButton2 = 3;
const unsigned long kEEPROM_update_time = 5*1000;//10s
unsigned long timeCheck;

void setup() {
  Serial.begin(9600);           // set up Serial library at 9600 bps
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  Serial.println("Stepper connected");

  EEPROM.get(ADDRESS_POS, curPos);
  eepromPos = curPos;
  Serial.println("Current position:");
  Serial.println(curPos, DEC);    //This may print 'ovf, nan' if the data inside the EEPROM is not a valid float.
  EEPROM.get(ADDRESS_SPEED, curSpeed);
  Serial.println("Current speed:");
  Serial.println(curSpeed, DEC);    //This may print 'ovf, nan' if the data inside the EEPROM is not a valid float.
  EEPROM.get(ADDRESS_MINPOS, minPos);
  EEPROM.get(ADDRESS_MAXPOS, maxPos);
  Serial.println("A");
   
  AFMS.begin();  // create with the default frequency 1.6KHz
  myMotor->setSpeed(curSpeed);  // 10 rpm   

   // Setup callbacks for SerialCommand commands // Have to change define in serial command max # of commands to 20
  SCmd.addCommand("move",move);       // move
  SCmd.addCommand("speed",speed);       
  SCmd.addCommand("pos?",reportPos);   
  SCmd.addCommand("speed?",reportSpeed);    
  SCmd.addCommand("setzero",setzero);    
  SCmd.addCommand("stepsize",relStepSize);    
  SCmd.addCommand("stepsize?",reportStepSize);  
  SCmd.addCommand("home",home);  
  SCmd.addCommand("setmax",setmax);
  SCmd.addCommand("setmin",setmin);
  SCmd.addCommand("poslim?",getPosLim);          
  SCmd.addCommand("help",help);  
          
  SCmd.addDefaultHandler(unrecognized);  // Handler for command that isn't matched  (says "?") 

  //setup the push buttons 
  pinMode(kPinButton1, INPUT);
  pinMode(kPinButton2, INPUT);
  digitalWrite(kPinButton1, HIGH);
  digitalWrite(kPinButton2, HIGH);

  //setup the time check
  timeCheck = millis();
}

void loop()
{  
  SCmd.readSerial();     // We don't do much, just process serial commands
  if(digitalRead(kPinButton1)==LOW){
     //Serial.println("Motor UP");
     relmove(stepSize);
     //delay(20);//optional delay if button pressed too much
  }
  if(digitalRead(kPinButton2)==LOW){
     //Serial.println("Motor DOWN");
     relmove(-1*stepSize);
     //delay(20);//optional delay if button pressed too much
  }

  // update the eeprom every N s 
  // since we dont update it during manual moves to avoid 
  // too many eeprom writes
  //this only updates the eeprom if value has actually changed
  if (millis() - timeCheck > kEEPROM_update_time){
    updateEEPROMPos();
    timeCheck = millis();
  }
  
  
}

void move(){
  int pos;  
  char *arg; 

  arg = SCmd.next(); 
  if (arg != NULL){
    pos =atol(arg); 
    moveMotor(pos);
    updateEEPROMPos();
    Serial.println("A"); 

  } else{
    Serial.println("?"); 
  }
}

void moveMotor(int pos){
    pos = constrain(pos,minPos,maxPos);// software hard limits to avoid motor crash
    int nStep = pos-curPos;
    if (nStep>=0){
      myMotor->step(nStep, FORWARD, SINGLE); 
      
    }
    else {
      myMotor->step(-nStep, BACKWARD, SINGLE); 
    }
     
    curPos=pos;
}

void updateEEPROMPos(){// update the eeprom, as long as its actually changed
  if (curPos != eepromPos){
    eepromPos = curPos;
    EEPROM.put(ADDRESS_POS, eepromPos);  
  }
}


// this is the move function for manual moves
// doesnt update the eeprom as dont want to do too many writes
// instead we have a function that checks every N s if we need to update the 
// eeprom
void relmove(int relStep){
  int posFinal;
  posFinal = curPos + relStep;
  moveMotor(posFinal);
}

void reportPos(){ 
  Serial.println(curPos);
}

void reportSpeed(){ 
  Serial.println(curSpeed);
}

void speed(){
  int s;  
  char *arg; 

  arg = SCmd.next(); 
  if (arg != NULL){ 
    s =atol(arg); 
    myMotor->setSpeed(s);

    curSpeed=s;
    EEPROM.put(ADDRESS_SPEED, curSpeed);
    Serial.println("A"); 
  } 
}

void relStepSize(){
  int s;  
  char *arg; 

  arg = SCmd.next(); 
  if (arg != NULL){ 
    s =atol(arg); 
    stepSize = s;

    Serial.println("A"); 
  } 
}
void reportStepSize(){ 
  Serial.println(stepSize);
}
void setzero(){// set current position as zero location
  curPos=0;
  EEPROM.put(ADDRESS_POS, curPos);
  Serial.println("A"); 
}

void home(){
  moveMotor(0);
  Serial.println("A"); 
}

// This gets set as the default handler, and gets called when no other command matches. 
void unrecognized()
{
  Serial.println("?"); 
}

void setmax(){
  int s;  
  char *arg; 

  arg = SCmd.next(); 
  if (arg != NULL){ 
    s =atol(arg); 
    maxPos = s;
  } else  {
    maxPos = curPos;
  }
  
  EEPROM.put(ADDRESS_MAXPOS, maxPos);
  Serial.println("A"); 
}

void getPosLim(){
   Serial.println("Min pos:");
   Serial.println(minPos); 
   Serial.println("Max pos:");
   Serial.println(maxPos);
}

void setmin(){
  int s;  
  char *arg; 

  arg = SCmd.next(); 
  if (arg != NULL){ 
    s =atol(arg); 
    minPos = s;
  } else  {
    minPos = curPos;
  }
  
  EEPROM.put(ADDRESS_MINPOS, minPos);
  Serial.println("A"); 
}

void help(){
  Serial.println("Device commands:");
  Serial.println("move val - moves device to val");
  Serial.println("speed val - sets speed to val (valid #s 0 - 255)");
  Serial.println("pos? - returns current device position");
  Serial.println("speed? - return current device speed");
  Serial.println("setzero - sets current position as zero");
  Serial.println("stepsize");
  Serial.println("stepsize?");
  Serial.println("home");
  Serial.println("setmax");
  Serial.println("setmin");
  Serial.println("poslim");
}
