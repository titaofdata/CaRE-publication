#include <CurieBLE.h>
#include <cstdlib>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

BLEPeripheral blePeripheral; 
BLEService CAREService("293f365c-d247-4426-9ceb-a466378d457e"); // BLE CaRE Service
BLEIntCharacteristic DataCharacteristic("293f365c-d247-4426-9ceb-a466378d457e", BLERead | BLEWrite);

LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

//GUI
char t_addr = 'G';      //Borg
char u_addr = 'A';      //ECG
char v_addr = 'B';      //HR
char w_addr = 'C';      //SpO2
char x_addr = 'H';      //BP
char y_addr = 'E';      //Cadence
char z_addr = 'F';      //Resistance Level

//BP
char command;
char endcuff;
char report;
char bp_data[33];
char c_systole[4];
char c_diastole[4];
int systole;
int diastole;
int i = 0;
int j = 0;
char blood_pressure[4];
char buff[3];
bool activate_quickcheck;


//Pulse Ox data
unsigned int PULSEOX_DATA = 0;
unsigned int O2_SAT = 0;
int l=0;
char buff1[2];
int spctr=0;


char timeelapsed_addr = 'Y';
char end_addr = 'Z';
char command_addr;

int previousdata;
long duration;
int rhr;
int thr;
int rpe;
int flag;
String input, stringduration, stringrhr, stringthr, stringrpe, stringload_value, stringflag;
char charduration[2],charrhr[2], charthr[2], charrpe[2], charload_value[2], charflag[2];
int n=0;

//workout duration
int startbit;
float timer=0;
char time_elapsed[6]="00:00";
int seconds=0;
int minutes=0;
float warm_up;
float main_phase;
float cool_down;
float cool_down_rate;
float start_time;
int duration_seconds;
int duration_minutes;
float delta_time;
int clkctr = 0;
float current;


int reslevel = 0;
int cadence = 0;
int bp_sys = 0;       
int bp_dias = 0;     
int oxy = 0;          
unsigned int hr = 0;           
int ecg = 0;

String somebin;

//Borg's Constants
int borg9 = 13;
int borg8 = 12;
int borg7 = 11;
int borg6 = 10;
int borg5 = 9;
int borg4 = 8;
int borg3 = 7;
int borg2 = A2;
int borg1 = A1;
int borgvalue;
char buff2[2];
int buttonpressed;

//Motor Constants
int pwm = 5;
int dir = 4;
int pot = A0;
int res_value = 0;
const int motor_pwm = 255;
float load_value;
int received_data;
float target_load;

//Cadence
const byte REED_PIN = 6; // Pin connected to reed switch
float rpm;
int int_rpm;
float timeold = 0;
float timenew = 0;
char buff3[2];
bool zero_rpm;

//Mux
const int selector_a = 2;
const int selector_b = 3;

void setup() {
  
  Serial.begin(115200);
  /*
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Native USB only
  }
  */
  Serial.println("Setting up BLE...");
  BLE.begin();

  warm_up = duration/3;
  main_phase = duration*0.75;
  cool_down = duration*0.25;

  // set advertised local name and service UUID:
  blePeripheral.setLocalName("CaRE");
  blePeripheral.setAdvertisedServiceUuid(CAREService.uuid());

  // add service and characteristic:
  blePeripheral.addAttribute(CAREService);
  blePeripheral.addAttribute(DataCharacteristic);

  // set the initial value for the characeristic:
  DataCharacteristic.setValue(0);
  previousdata = DataCharacteristic.value();

  // begin advertising BLE service:
  blePeripheral.begin();
  Serial.println("CaRE System: waiting for connection...");


  Serial.println("Setting up BP and SpO2 module connection...");
  //BP Setup
  Serial1.begin(4800); //BP
  while (!Serial1) {
    ;
  }

 Serial.println("Initializing BP Module...");
 Serial1.print("X"); //Abort BP Measuring
 Serial1.write(2);          //Set BP to manual mode
 Serial1.print("03;;D9");
 Serial1.write(3);
  
  Serial.println("Setting up Multiplexer...");
  pinMode(selector_a, OUTPUT);
  pinMode(selector_b, OUTPUT);
  digitalWrite(selector_a, HIGH);
  digitalWrite(selector_b, LOW);

  //Initialize Borg's
  Serial.println("Setting up Borg's Buttons...");
  pinMode(borg1, INPUT);
  pinMode(borg2, INPUT);
  pinMode(borg3, INPUT);
  pinMode(borg4, INPUT);
  pinMode(borg5, INPUT);
  pinMode(borg6, INPUT);
  pinMode(borg7, INPUT);
  pinMode(borg8, INPUT);
  pinMode(borg9, INPUT);

  //Initialize LCD
  Serial.println("Setting up LCD...");
  lcd.init();            
  lcd.clear();
  lcd.backlight();

  //Initialize Motor
  Serial.println("Initializing Motor...");
  pinMode(pwm, OUTPUT);
  pinMode(dir, OUTPUT);
  delay(10);
  init_motor();

  //Initialize Reed Switch
  pinMode(REED_PIN, INPUT_PULLUP);  //cadence
  attachInterrupt(digitalPinToInterrupt(REED_PIN), blink, LOW);
  
  Serial.println("Setup Complete");
  
}

void loop() {
  // listen for BLE peripherals to connect:
  BLECentral central = blePeripheral.central();
  // if a central is connected to peripheral:
  if (central) {
    Serial.print("Connected to central: ");
    // print the central's MAC address:
    Serial.println(central.address());

    // while the central is still connected to peripheral:
    while (central.connected()) {
      //adjust_motor();
      
      
      //Receive Input Parameters
      Serial.println(previousdata);
      while(DataCharacteristic.value() == previousdata){
          //do nothing
      }
      previousdata = DataCharacteristic.value(); //save received value
      DataCharacteristic.setValue(0); //initialize to 0
      input = String(previousdata, HEX);
      //input = String(DataCharacteristic.value(), HEX); 
      Serial.print("Received data:");
      Serial.println(input);
      //Print Received Input Parameters
      stringrpe = input.substring(6);
      stringrpe.toCharArray(charrpe,5);
      rpe = strtol(charrpe, 0, 16);
      Serial.print("Borg's RPE:");
      Serial.println(rpe);
      
      stringduration = input.substring(4,6);
      stringduration.toCharArray(charduration,5);
      duration = strtol(charduration, 0, 16);
      Serial.print("Exercise Duration:");
      Serial.println(duration);
      
      stringrhr = input.substring(2,4);
      stringrhr.toCharArray(charrhr,5);
      rhr = strtol(charrhr, 0, 16);
      Serial.print("Resting Heart Rate:");
      Serial.println(rhr);
      
      stringthr = input.substring(0,2);
      stringthr.toCharArray(charthr,5);
      thr = strtol(charthr, 0, 16);;
      Serial.print("Target Heart Rate:");
      Serial.println(thr);
      
     
      
      int n=0;
      duration = duration*60*1000;
      
      while (DataCharacteristic.value() != 1){
      }
      start_time = millis();
      duration_seconds = floor(duration/1000);
      duration_seconds = duration_seconds % 60;
      duration_minutes = floor(duration/1000)/60;
      duration_minutes = floor(duration_minutes);
      n++;
      
      while(1){
        if(n==0){
             Initialize_BP();
         }
        adjust_motor();
        Measure_PulseOx();
        current_time();
        startstopDisplay();
        clockDisplay();
        promtDisplay();
        cadence_indicator();
        if(((minutes % 3) == 0 || (minutes == 0)) && (seconds <=10)){
           Serial1.end();
           delay(100);
           digitalWrite(selector_a, HIGH);
           digitalWrite(selector_b, LOW);
           Serial1.begin(4800);
           while (!Serial1) {
              ;
           }
          Serial.println("Measuring Blood Pressure...");
          Serial1.write(2);          //Set BP Start of Measurement
          Serial1.print("01;;D7");
          Serial1.write(3);
          activate_quickcheck = true;
          Measure_BP();
          Serial.println("BP Check done");
          Serial.println(minutes);
          Serial.println(seconds); 
          Serial1.end();
          delay(100);
           digitalWrite(selector_a, LOW);
          digitalWrite(selector_b, HIGH);
          Serial1.begin(4800, SERIAL_8E1);
          while (!Serial1) {
              ;
          }
         }
          adjust_motor();
          Measure_PulseOx(); 
          check_borg();
          transmit_cadence_resistance();
      }
     
    }

    // when the central disconnects, print it out:
    Serial.print(F("Disconnected from central: "));
    Serial.println(central.address());
    
  }
}

void Initialize_BP(){
 Serial.println("Initializing BP Module...");
 Serial1.print("X"); //Abort BP Measuring
 Serial1.write(2);          //Set BP to manual mode
 Serial1.print("03;;D9");
 Serial1.write(3);
 Serial.println("Measuring Blood Pressure...");
 Serial1.write(2);          //Set BP Start of Measurement
 Serial1.print("01;;D7");
 Serial1.write(3);

}


void Measure_BP(){
  while(activate_quickcheck == true){
    check_borg();
    //Display Functions
    current_time();
    startstopDisplay();
    clockDisplay();
    promtDisplay();
    cadence_indicator();
    transmit_cadence_resistance();
    if(Serial1.available()>0){
    endcuff = Serial1.read();
    if(endcuff == '9'){
      if(Serial1.available()>0){
        endcuff = Serial1.read();
        if(endcuff == '9'){
          if(Serial1.available()>0){
            endcuff = Serial1.read();
            if(endcuff == '9'){
              activate_quickcheck = false;
              break;
          }
        }
      }
    }
   }
  }
 }
  Serial1.write(2);
  Serial1.print("18;;DF");
  Serial1.write(3);
  while(!Serial1.available());
  while(Serial1.read() != 2);
  while(i != 34){
    bp_data[i] = Serial1.read();
    while(!Serial1.available());
    i++;
  }
  i=0;
  for(j = 15; j<18; j++){
    blood_pressure[i]=bp_data[j];
    i++;
  }
  systole = atoi(blood_pressure);
  Serial.print("Blood Pressure:");
  Serial.print(systole);
  Serial.print("/");
  i=0;
  for(j = 18; j<21; j++){
    blood_pressure[i]=bp_data[j];
    i++;
  }
  diastole = atoi(blood_pressure);
  Serial.println(diastole);
  if(systole == 0 || diastole == 0){
      systole = 33;
      diastole = 33;
  }
  sprintf(buff, "%x%x%x",x_addr,systole,diastole);
  unsigned long bp_data = std::strtoul(buff, 0, 16);
  DataCharacteristic.setValue(0);  
  DataCharacteristic.setValue(bp_data);
  delay(20);
  
  i = 0;
}

void Measure_BP_quickcheck(){
  if(activate_quickcheck == true){
    if(Serial1.available()>0){
    endcuff = Serial1.read();
    if(endcuff == '9'){
      if(Serial1.available()>0){
        endcuff = Serial1.read();
        if(endcuff == '9'){
          if(Serial1.available()>0){
            endcuff = Serial1.read();
            if(endcuff == '9'){
              activate_quickcheck = false;
              Serial1.write(2);
              Serial1.print("18;;DF");
              Serial1.write(3);
              while(!Serial1.available());
              while(Serial1.read() != 2);
              while(i != 34){
                bp_data[i] = Serial1.read();
                while(!Serial1.available());
                i++;
              }
             i=0;
             for(j = 15; j<18; j++){
                blood_pressure[i]=bp_data[j];
                i++;
             }
             systole = atoi(blood_pressure);
             Serial.print("Blood Pressure:");
             Serial.print(systole);
             Serial.print("/");
             i=0;
             for(j = 18; j<21; j++){
                blood_pressure[i]=bp_data[j];
                i++;
              }
             diastole = atoi(blood_pressure);
             Serial.println(diastole);
             if(systole == 0 || diastole == 0){
              systole = 33;
              diastole = 33;
             }
             sprintf(buff, "%x%x%x",x_addr,systole,diastole);
             unsigned long bp_data = std::strtoul(buff, 0, 16);
             DataCharacteristic.setValue(0);  
             DataCharacteristic.setValue(bp_data);
             delay(20);
             
             i = 0;
          }
        }
      }
    }
   }
  } 
  }
}  


void Measure_PulseOx(){
  spctr = 0;
  while(spctr<=5){
    if(Serial1.available() > 0){
    l=0;
    PULSEOX_DATA = Serial1.read();
    if((PULSEOX_DATA >= 128) && (Serial1.available() > 0)){
      while((l<4) && (Serial1.available() > 0)){
        l++;
        O2_SAT = Serial1.read();
        if(O2_SAT >= 128){
          break;
        }
        delay(5);
      }
      PULSEOX_DATA = Serial1.read();
      if(PULSEOX_DATA >= 128 && l==4 && (Serial1.available() > 0) && (O2_SAT <127)){
          Serial.print("SpO2:");
          Serial.print("\t");
          Serial.println(O2_SAT);
          //Prepare SpO2 Transmission
          sprintf(buff1, "%02x%02x",w_addr,O2_SAT);
          unsigned long pulseox_data = std::strtoul(buff1, 0, 16);
          DataCharacteristic.setValue(0);  
          DataCharacteristic.setValue(pulseox_data);
          delay(20);
          //printBits(O2_SAT);
      }
      else if(O2_SAT == 127){
        O2_SAT = 0;
        Serial.print("SpO2:");
          Serial.print("\t");
          Serial.println(O2_SAT);
          //Prepare SpO2 Transmission
          sprintf(buff1, "%02x%02x",w_addr,O2_SAT);
          unsigned long pulseox_data = std::strtoul(buff1, 0, 16);
          DataCharacteristic.setValue(0);  
          DataCharacteristic.setValue(pulseox_data);
          delay(20);
      }
    }
  }
    spctr++; 
  } 
}  

void check_borg(){
  Measure_BP_quickcheck();
  if(digitalRead(borg9)){
    Serial.println("Borg's Scale Button 9 Pressed");
    borgvalue = 9;
    //Prepare Borg Transmission
    sprintf(buff2, "%02x%02x",t_addr,borgvalue);
    unsigned long borg_data = std::strtoul(buff2, 0, 16);
    DataCharacteristic.setValue(0);  
    DataCharacteristic.setValue(borg_data); 
    delay(20);
  }
  else if(digitalRead(borg8)){
    Serial.println("Borg's Scale Button 8 Pressed");
    borgvalue = 8;
    //Prepare Borg Transmission
    sprintf(buff2, "%02x%02x",t_addr,borgvalue);
    unsigned long borg_data = std::strtoul(buff2, 0, 16);
    DataCharacteristic.setValue(0);  
    DataCharacteristic.setValue(borg_data);
    delay(20);
  }
  else if(digitalRead(borg7)){
    Serial.println("Borg's Scale Button 7 Pressed");
    borgvalue = 7;
    //Prepare Borg Transmission
    sprintf(buff2, "%02x%02x",t_addr,borgvalue);
    unsigned long borg_data = std::strtoul(buff2, 0, 16);
    DataCharacteristic.setValue(0);  
    DataCharacteristic.setValue(borg_data); 
    delay(20);
  }
  else if(digitalRead(borg6)){
    Serial.println("Borg's Scale Button 6 Pressed");
    borgvalue = 6;
    //Prepare Borg Transmission
    sprintf(buff2, "%02x%02x",t_addr,borgvalue);
    unsigned long borg_data = std::strtoul(buff2, 0, 16);
    DataCharacteristic.setValue(0);  
    DataCharacteristic.setValue(borg_data);
    delay(20);
  }
  else if(digitalRead(borg5)){
    Serial.println("Borg's Scale Button 5 Pressed");
    borgvalue = 5;
    //Prepare Borg Transmission
    sprintf(buff2, "%02x%02x",t_addr,borgvalue);
    unsigned long borg_data = std::strtoul(buff2, 0, 16);
    DataCharacteristic.setValue(0);  
    DataCharacteristic.setValue(borg_data); 
    delay(20);
  }
  else if(digitalRead(borg4)){
    Serial.println("Borg's Scale Button 4 Pressed");
    borgvalue = 4;
    //Prepare Borg Transmission
    sprintf(buff2, "%02x%02x",t_addr,borgvalue);
    unsigned long borg_data = std::strtoul(buff2, 0, 16);
    DataCharacteristic.setValue(0);  
    DataCharacteristic.setValue(borg_data);
    delay(20); 
  }
  else if(digitalRead(borg3)){
    Serial.println("Borg's Scale Button 3 Pressed");
    borgvalue = 3;
    //Prepare Borg Transmission
    sprintf(buff2, "%02x%02x",t_addr,borgvalue);
    unsigned long borg_data = std::strtoul(buff2, 0, 16);
    DataCharacteristic.setValue(0);  
    DataCharacteristic.setValue(borg_data); 
    delay(20);
  }
  else if(analogRead(borg2)>500){
    Serial.println("Borg's Scale Button 2 Pressed");
    borgvalue = 2;
    //Prepare Borg Transmission
    sprintf(buff2, "%02x%02x",t_addr,borgvalue);
    unsigned long borg_data = std::strtoul(buff2, 0, 16);
    DataCharacteristic.setValue(0);  
    DataCharacteristic.setValue(borg_data); 
    delay(20);
  }
  else if(analogRead(borg1)>500){
    Serial.println("Borg's Scale Button 1 Pressed");
    borgvalue = 1;
    //Prepare Borg Transmission
    sprintf(buff2, "%02x%02x",t_addr,borgvalue);
    unsigned long borg_data = std::strtoul(buff2, 0, 16);
    DataCharacteristic.setValue(0);  
    DataCharacteristic.setValue(borg_data); 
    delay(20);
  }
  else{
    borgvalue = 0;
    Measure_BP_quickcheck();
    //Serial.println("No button pressed!");
  }
}

void clockDisplay(){
  Measure_BP_quickcheck();
  if(delta_time <= duration){
    if(minutes <= 9 && seconds <= 9){
      lcd.setCursor(0,1);
      lcd.print("0");
      lcd.setCursor(1,1);
      lcd.print(minutes);
      lcd.setCursor(2,1);
      lcd.print(":");   
      lcd.setCursor(3,1);
      lcd.print("0");
      lcd.setCursor(4,1);
      lcd.print(seconds);
    }
    else if(minutes > 9 && seconds <= 9){
      lcd.setCursor(0,1);
      lcd.print(minutes);
      lcd.setCursor(2,1);
      lcd.print(":");   
      lcd.setCursor(3,1);
      lcd.print("0");
      lcd.setCursor(4,1);
      lcd.print(seconds);
    }
    else if(minutes <= 9 && seconds > 9){
      lcd.setCursor(0,1);
      lcd.print("0");
      lcd.setCursor(1,1);
      lcd.print(minutes);
      lcd.setCursor(2,1);
      lcd.print(":");   
      lcd.setCursor(3,1);
      lcd.print(seconds);
    }
    else if(minutes > 9 && seconds > 9){
      lcd.setCursor(0,1);
      lcd.print(minutes);
      lcd.setCursor(2,1);
      lcd.print(":");   
      lcd.setCursor(3,1);
      lcd.print(seconds);
    }
  }
  else if(clkctr == 0){
    if(duration_minutes <= 9 && duration_seconds <= 9){
      lcd.setCursor(0,1);
      lcd.print("0");
      lcd.setCursor(1,1);
      lcd.print(duration_minutes);
      lcd.setCursor(2,1);
      lcd.print(":");   
      lcd.setCursor(3,1);
      lcd.print("0");
      lcd.setCursor(4,1);
      lcd.print(duration_seconds);
    }
    else if(duration_minutes > 9 && duration_seconds <= 9){
      lcd.setCursor(0,1);
      lcd.print(duration_minutes);
      lcd.setCursor(2,1);
      lcd.print(":");   
      lcd.setCursor(3,1);
      lcd.print("0");
      lcd.setCursor(4,1);
      lcd.print(duration_seconds);
    }
    else if(duration_minutes <= 9 && duration_seconds > 9){
      lcd.setCursor(0,1);
      lcd.print("0");
      lcd.setCursor(1,1);
      lcd.print(duration_minutes);
      lcd.setCursor(2,1);
      lcd.print(":");   
      lcd.setCursor(3,1);
      lcd.print(duration_seconds);
    }
    else if(duration_minutes > 9 && duration_seconds > 9){
      lcd.setCursor(0,1);
      lcd.print(duration_minutes);
      lcd.setCursor(2,1);
      lcd.print(":");   
      lcd.setCursor(3,1);
      lcd.print(duration_seconds);
    }
    clkctr++;
  }
  Measure_BP_quickcheck();
}

void startstopDisplay(){
  Measure_BP_quickcheck();
  if(delta_time >= duration){
    lcd.setCursor(0,1);
    lcd.print("     ");
    lcd.setCursor(5,1);
    lcd.print(" STOP      "); 
  }
}

void promtDisplay(){
  Measure_BP_quickcheck();
  if(delta_time <= duration){
    if((seconds <=15) && (minutes > 0) && (buttonpressed == 0)){
      lcd.setCursor(5,1);
      lcd.print("Push Button");
      check_borg();
      if(borgvalue>0){
        if(borgvalue > rpe){
            load_value = res_value - 52;
        if (load_value >= 500){
          motor_control(0, load_value);
        }
      }
        buttonpressed++;
        lcd.setCursor(5,1);
        lcd.print(" Keep going");
      }
      else{
        buttonpressed = 0;
      }
    }
    else if(seconds>15){
      buttonpressed = 0;
      lcd.setCursor(5,1);
      lcd.print(" Keep going");
    }
    else{
      lcd.setCursor(5,1);
      lcd.print(" Keep going");
    }
  }
  Measure_BP_quickcheck();
}

void current_time(){
  Measure_BP_quickcheck();
  current = millis();
  delta_time = current - start_time;
  seconds = floor(delta_time/1000);
  seconds = seconds % 60;
  minutes = floor(delta_time/1000)/60;
  minutes = floor(minutes);
  sprintf(time_elapsed, "%02d:%02d", minutes,seconds);
  Measure_BP_quickcheck();
}

void cadence_indicator(){
  Measure_BP_quickcheck();
  if(int_rpm > 95){
    lcd.setCursor(3,0);
    lcd.print("TOO FAST ");
    load_value = res_value + 52;
    if ((load_value <= 1020) && ((seconds % 10) == 0)){
      motor_control(1, load_value);
    }
  }
  else if(int_rpm < 85){
    lcd.setCursor(3,0);
    lcd.print("TOO SLOW ");
  }
  else if(int_rpm > 85 && int_rpm < 95){
    lcd.setCursor(3,0);
    lcd.print("ON TARGET");
  }
  Measure_BP_quickcheck();
}

void init_motor(){
  //INITIALIZE TO LOW LOAD
  res_value = analogRead(pot);
  Serial.println(res_value);
  while(res_value >= 500){
      analogWrite(pwm, 255);
      digitalWrite(dir, HIGH);
      res_value = analogRead(pot);
      Serial.println(res_value);
  }
    analogWrite(pwm, 0);
}

void motor_control(int motor_direction, float motor_resistance){
  res_value = analogRead(pot);
  Serial.println(res_value);
  if(motor_direction == 1){
    while(res_value < motor_resistance){
      res_value = analogRead(pot);
      Serial.println(res_value);
      analogWrite(pwm, motor_pwm); 
      digitalWrite(dir, LOW);
      if(res_value >= motor_resistance){
        analogWrite(pwm, 0);
      }  
    }
    if(res_value >= motor_resistance){
        analogWrite(pwm, 0);
    }
  }
  else{
    while(res_value > motor_resistance){
      res_value = analogRead(pot);
      //Serial.println(res_value);
      analogWrite(pwm, motor_pwm); 
      digitalWrite(dir, HIGH);
      if(res_value <= motor_resistance){
        analogWrite(pwm, 0);
      }
    }
    if(res_value <= motor_resistance){
        analogWrite(pwm, 0);
   }
  }
}

void transmit_cadence_resistance(){
  Measure_BP_quickcheck();
  if(millis() - timenew >= 1000){
    int_rpm = 0;
  }
  Serial.print("RPM:");
  Serial.println(int_rpm);
  //Send Cadence
  sprintf(buff3, "%02x%02x",y_addr, int_rpm);
  unsigned long cadence_data = std::strtoul(buff3, 0, 16);
  DataCharacteristic.setValue(0);  
  DataCharacteristic.setValue(cadence_data);
  zero_rpm = true;
  delay(10);
  delay(5);
  Measure_BP_quickcheck();
  //Send Resistance
  res_value = analogRead(pot);
  sprintf(buff3, "%02x%04x",z_addr, res_value);
  unsigned long res_data = std::strtoul(buff3, 0, 16);
  DataCharacteristic.setValue(0);  
  DataCharacteristic.setValue(res_data);
  delay(10);
  Measure_BP_quickcheck();
}

void blink(){
    zero_rpm = false;
    timenew = millis();
    rpm = 60000/(timenew - timeold);
  
    if((timenew - timeold) > 20){
      int_rpm = rpm;
    }
    timeold = timenew;  
}

void adjust_motor(){
      received_data = DataCharacteristic.value(); //save received value
      DataCharacteristic.setValue(0); //initialize to 0
      input = String(received_data, HEX);
      //input = String(DataCharacteristic.value(), HEX); 
      Serial.print("Received data:");
      Serial.println(input);
      //Print Received Input Parameters
      stringflag = input.substring(2);
      stringflag.toCharArray(charflag,5);
      flag = strtol(charflag, 0, 16);
      Serial.print("Motor Flag:");
      Serial.println(flag);
      
      stringload_value = input.substring(0,2);
      stringload_value.toCharArray(charload_value,5);
      load_value = strtol(charload_value, 0, 16);
      Serial.print("Load Value:");
      Serial.println(load_value);

  if(flag == 101){
    Serial.print("Motor adjustment:");
    Serial.print("\t");
    Serial.println(load_value);
    target_load = float(load_value/100);
    Serial.println(target_load);
    target_load = float(target_load*520);
    Serial.println(target_load);
    load_value = (int(target_load) + 500);
    Serial.print("Target Resistance:");
    Serial.print("\t");
    Serial.println(load_value);
  if(load_value <= res_value){
    if (load_value >= 500){
      motor_control(0, load_value);
      load_value = 0;
    }
  }
  else if(load_value > res_value){
    if (load_value <= 1020){
      motor_control(1, load_value);
    }
  }
  }
}
