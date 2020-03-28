// Arduino code to turn analogue throttle position sensor readings into calculated throttle position
//
// Code is designed to 'fail safe' i.e. it should output zero throttle position in the event of a wiring or sensor fault
// If there are no errors, throttle position is set to the average of the smoothed sensor readings
//
// Error bits are set in the 'error' variable as follows:
//  000 = no error
//  001 = sensor 1 out of bounds
//  010 = sensor 2 out of bounds
//  100 = mismatch between sensors

// **************** CONFIG *****************
// sensor 1 
const int sensor1_pin = A0;
const int sensor1_idle = 42; // raw reading from sensor at idle
const int sensor1_max = 370; // raw reading from sensor at max throttle

// sensor 2 
const int sensor2_pin = A1;
const int sensor2_idle = 85; // raw reading from sensor at idle
const int sensor2_max = 755; // raw reading from sensor at max throttle

// output 
const int error_pin = 13;
const int motor_pin = 9;

// general 
const int sensor_tolerance_percent = 10; // maximum permissible error from sensor
const int throttle_min = 0;   // throttle min value - change according to application
const int throttle_max = 100; // throttle max value - change according to application
const int num_readings = 3;  // number of readings to use for rolling average
const int loop_delay = 50;    // delay between samples, in ms

// **************** END OF CONFIG *****************

// internal variables
int sensor1_input = 0;          // raw input from sensor
int sensor2_input = 0;
int sensor1 = 0;                // smoothed & scaled sensor reading
int sensor2 = 0;
int readings1[num_readings];    // array to store readings from sensor
int readings2[num_readings];     
int read_index = 0;             // index for reading array
int total1 = 0;                 // running totals
int total2 = 0;                
int sensor1_average = 0;        // averaged sensor reading
int sensor2_average = 0;
int mismatch_percent = 0;       // calculated mismatch between sensors relative to overall throttle range
int throttle_position = 0;      // calculated throttle position

// calculate upper & lower bounds for acceptable sensor readings
int sensor1_lower_bound = sensor1_idle - (sensor1_idle * sensor_tolerance_percent / 100);
int sensor1_upper_bound = sensor1_max + (sensor1_max * sensor_tolerance_percent / 100);
int sensor2_lower_bound = sensor2_idle - (sensor2_idle * sensor_tolerance_percent / 100);
int sensor2_upper_bound = sensor2_max + (sensor2_max * sensor_tolerance_percent / 100);

// output variables
int motor_speed = 0;            // motor speed sent to motor controller
int lcd_chars = 0;              // number of characters to display on LCD
char lcd_text[] = "                ";
int error = 0;
String debug_text;

// LCD library
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display

void setup() {
  
  // initialize serial communication with computer:
  Serial.begin(9600);
  
  // initialize the average arrays
  for (int this_reading1 = 0; this_reading1 < num_readings; this_reading1++) {
    readings1[this_reading1] = 0;
  }
  for (int this_reading2 = 0; this_reading2 < num_readings; this_reading2++) {
    readings2[this_reading2] = 0;
  }

  // LED output
  pinMode(error_pin, OUTPUT);

  // Motor output
  pinMode(motor_pin, OUTPUT);
  
  // LCD output
  lcd.init();
  lcd.backlight();
  lcd.setCursor(1,0);
  lcd.print("Throttle");
}

void loop() {

  error = 0; // clear any errors

  sensor1_input = analogRead(sensor1_pin);  // read raw analogue values
  sensor2_input = analogRead(sensor2_pin);

  // average the sensor readings
  total1 = total1 - readings1[read_index];  // remove the last reading from array
  total2 = total2 - readings2[read_index];

  readings1[read_index] = sensor1_input;    // add new reading to array
  readings2[read_index] = sensor2_input;
  
  total1 = total1 + readings1[read_index];  // add the reading to the total:
  total2 = total2 + readings2[read_index];
  
  read_index++; // advance to the next position in the array:

  // if we're at the end of the array...
  if (read_index >= num_readings) {
    // ...wrap around to the beginning:
    read_index = 0;
  }

  // calculate the averages:
  sensor1_average = total1 / num_readings;
  sensor2_average = total2 / num_readings;

  // sensor 1 is below idle
  if (sensor1_average < sensor1_lower_bound) {
    sensor1_average = sensor1_idle; // force sensor 1 to idle
    error = error | 1; // sensor 1 fault 
  }

  // sensor 1 is above max
  if (sensor1_average > sensor1_upper_bound) {
    sensor1_average = sensor1_idle; // force sensor 1 to idle
    error = error | 1; // sensor 1 fault
  }

  // sensor 2 is below idle
  if (sensor2_average < sensor2_lower_bound) {
    sensor2_average = sensor2_idle; // force sensor 2 to idle
    error = error | 2; // sensor 2 fault
  }

  // sensor 2 is above max
  if (sensor2_average > sensor2_upper_bound) {
    sensor2_average = sensor2_idle; // force sensor 2 to idle
    error = error | 2; // sensor 2 fault
  }

  // scale sensor 1 to throttle setting 
  sensor1 = map (sensor1_average, sensor1_idle, sensor1_max, throttle_min, throttle_max);
  sensor1 = constrain (sensor1, throttle_min, throttle_max);

  // scale sensor 2 to throttle setting
  sensor2 = map (sensor2_average, sensor2_idle, sensor2_max, throttle_min, throttle_max);
  sensor2 = constrain (sensor2, throttle_min, throttle_max);
  
  switch (error) {
    case 0: // both sensors within bounds
      mismatch_percent = abs(sensor2 - sensor1) * (throttle_max - throttle_min) / 100; 
      // sensor mismatch exceeds tolerance
      if (mismatch_percent > sensor_tolerance_percent) {
        throttle_position = 0; // idle
        error = error | 4; // sensor mismatch
      }
      else {
        // both sensors within bounds and within tolerance
        throttle_position = ((sensor1 + sensor2) / 2); // take the average of both sensors
      }
      break;      
    case 1:
      throttle_position = 0; // sensor 1 fault 
      break;
    case 2:
      throttle_position = 0; // sensor 2 fault 
      break;
    default:
      throttle_position = 0; // idle in all other cases
      break;
    }

  // set the motor speed
  SetMotor(throttle_position);

  // set the display
  SetDisplay(throttle_position, error);
  
  // send debug data
  debug_text = sensor1_input;
  debug_text += ", ";
  debug_text += sensor1_average;
  debug_text += ", ";
  debug_text += sensor1;
  debug_text += ", ";
  debug_text += sensor2_input;
  debug_text += ", ";
  debug_text += sensor2_average;
  debug_text += ", ";
  debug_text += sensor2;
  debug_text += ", ";
  debug_text += mismatch_percent;
  debug_text += ", ";
  debug_text += throttle_position;
  debug_text += ", ";
  debug_text += error;
  Serial.println(debug_text);
  
  delay(loop_delay);        // delay in between reads for stability
}

void SetMotor(int ThrottlePosition) {
  // control motor
  motor_speed = map(throttle_position, throttle_min, throttle_max, 0, 255);
  analogWrite(motor_pin, motor_speed); 
}

void SetDisplay(int throttle_position, int error) {
  // set display
  if (error > 0) {
    digitalWrite(error_pin, HIGH); 
    char lcd_text[] = "Error ";
    char error_text[1];
    itoa(error, error_text, 10); 
    strcat(lcd_text, error_text);
    lcd.setCursor(1,1);
    lcd.print(lcd_text);
  }
  else {
    digitalWrite(error_pin, LOW);
    lcd_chars = map(throttle_position, throttle_min, throttle_max, 1, 16);
    for (byte i = 0; i < 15; i++) {
      if (i < lcd_chars) { 
        lcd_text[i] = '*';
      }
      else {
        lcd_text[i] = ' ';
      }
    }    
    lcd.setCursor(1,1);
    lcd.print(lcd_text);    
  }
}
