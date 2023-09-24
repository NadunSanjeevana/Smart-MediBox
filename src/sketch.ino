#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>

// Define constants for the OLED screen
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Define constants for the NTP server
#define NTP_SERVER "pool.ntp.org"
#define UTC_OFFSET     0
#define UTC_OFFSET_DST 0

// Define constants for the pins
#define BUZZER 5
#define LED_1 15
#define LED_2 2
#define CANCEL 34
#define UP 33
#define OK 32
#define DOWN 35
#define DHT 12

// Define the OLED display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Define the DHT22 sensor object
DHTesp dht_sensor;

// Define variables for the notes used in the alarm
int num_notes = 8;
int note_A = 220;
int note_B = 294;
int note_C = 330;
int note_D = 349;
int note_E = 440;
int note_F = 494;
int note_G = 450;
int note_C_H = 523;
int notes[] = {note_A, note_B, note_C, note_D, note_F, note_E, note_G, note_C_H};

// Define global variables
int days = 0;
int month = 0;
int year = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;
unsigned long time_now = 0;
unsigned long time_last = 0;
bool alarm_enabled = true;
int num_alarms = 3;
int alarm_hours[] = {16,1,0};
int alarm_minutes[] = {38,5,10};
bool alarm_triggered[] = {false, false, false};
int max_temp = 32;
int min_temp = 26;
int max_humidity = 80;
int min_humidity = 60;
int current_mode = 0;
int max_modes = 5;
String options[] = {"1 - Set Time", "2 - Set Alarm 1", "3 - Set Alarm 2","4 - Set Alarm 3", "5 - Remove Alarm"};
int utc_offset = 19800;

void setup() {
  // Set up the pins
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  pinMode(CANCEL, INPUT);
  pinMode(UP, INPUT);
  pinMode(OK, INPUT);
  pinMode(DOWN, INPUT);

  // Set up the DHT sensor
  dht_sensor.setup(DHT, DHTesp::DHT22);

  // Start serial communication
  Serial.begin(9600);

  // Set up the OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("ERROR"));
    for (;;);
  }

  // Wait for the OLED display to initialize
  display.display();
  delay(2000);

  // Clear the OLED display and display a welcome message
  display.clearDisplay();
  print_line("Welcome to medibox!",0,0,2);
  delay(3000);

  // Connect to the WiFi network
  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    print_line("Connecting to WIFI", 2, 0 ,0);
  }
  display.clearDisplay();
  print_line("Connected to WIFI", 2, 0 ,0);

  configTime(UTC_OFFSET,UTC_OFFSET_DST, NTP_SERVER);
}

void loop() {
  // main code here
  update_time_with_check_alarm();
  if(digitalRead(OK) == LOW){
    delay(1000);
    Serial.println("MENU");
    go_to_menu();
  }

  check_temp();
}


// Function to print a message on a given row and column of the display
void print_line(String message,int text_size, int row, int column){
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(message);
  
  display.display();

}


// Function to print the current time on the display
void print_time_now(){
  display.clearDisplay();
  print_line(String(days), 1, 0, 0);
  print_line("/", 1, 0, 10);
  print_line(String(month), 1, 0, 20);
  print_line("/", 1, 0, 30);
  print_line(String(year), 1, 0, 40);
 
  print_line(String(hours), 2, 10, 0);
  print_line(":", 2, 10, 20);
  print_line(String(minutes), 2, 10, 30);
  print_line(":", 2, 10, 50);
  print_line(String(seconds), 2, 10, 60);
}





void update_time_over_wifi(){
  // Set the time zone and NTP server to get the correct UTC time
  configTime(utc_offset, UTC_OFFSET_DST, NTP_SERVER); 

  // Get the local time information
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  // Convert timeinfo to string format for extracting day, hour, minute, and second
  char date_str[8];
  char month_str[8];
  char year_str[20];
  char hour_str[28];
  char min_str[8];
  char sec_str[8];
  strftime(month_str, 20, "%m %d %Y", &timeinfo); // format date as "Mon DD, YYYY"
  strftime(date_str, 20, "%d", &timeinfo); 
  strftime(year_str, 20, "%Y", &timeinfo); 
  strftime(sec_str, 8, "%S", &timeinfo);
  strftime(hour_str, 8, "%H", &timeinfo);
  strftime(min_str, 8, "%M", &timeinfo);

  // Convert the extracted time information from string to integer format
  days = atoi(date_str);
  month = atoi(month_str);
  year = atoi(year_str);
  hours = atoi(hour_str);
  minutes = atoi(min_str);
  seconds = atoi(sec_str);
}



// Function to update time and check if alarm needs to be triggered
void update_time_with_check_alarm(){
  display.clearDisplay(); // Clear the display before updating the time
  update_time_over_wifi(); // Update the time over WiFi
  print_time_now(); // Print the updated time on the display

  // Check if alarm is enabled and if it's time to trigger the alarm
  if(alarm_enabled){
    for (int i = 0; i < num_alarms; i++){
      if(alarm_triggered[i] == false && hours == alarm_hours[i] && minutes == alarm_minutes[i]){
        ring_alarm(); // Trigger the alarm
        alarm_triggered[i] = true;
      }
    }
  }
}

// Function to trigger the alarm and play a tone until cancelled
void ring_alarm(){
  display.clearDisplay(); // Clear the display
  print_line("Medicine Time", 2, 0, 0); // Print the title on the display

  digitalWrite(LED_1, HIGH); // Turn on LED 1

  bool break_happened = false;

  // Loop until alarm is cancelled
  while(digitalRead(CANCEL) == HIGH){
    for( int i = 0; i < num_notes; i++){
      if(digitalRead(CANCEL) == LOW){
        delay(200);
        break_happened = true;
        break;
      }
      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }

  delay(200);
  digitalWrite(LED_1,  LOW); // Turn off LED 1

}

// Function to wait for a button press and return the button pressed
int wait_for_button_press(){
  while(true){
    if (digitalRead(UP) == LOW){
      delay(200);
      return UP;
    }

    else if (digitalRead(DOWN) == LOW){
      delay(200);
      return DOWN;
    }

    else if (digitalRead(CANCEL) == LOW){
      delay(200);
      return CANCEL;
    }

    else if (digitalRead(OK) == LOW){
      delay(200);
      return OK;
    }
  }
}


// function to navigate through the menu
void go_to_menu(void) { 
  while (digitalRead(CANCEL) == HIGH) {
    display.clearDisplay(); 
    print_line(options [current_mode], 2, 0, 0);

    int pressed = wait_for_button_press();

    if (pressed == UP) {
      current_mode += 1;
      current_mode %= max_modes; 
      delay(200);
    }

    else if (pressed == DOWN) { 
      current_mode -= 1;
      if (current_mode < 0) { 
        current_mode = max_modes - 1;
      } 
      delay(200);
    } 

    else if (pressed == OK) {
      Serial.println(current_mode); 
      delay(200);
      run_mode(current_mode);
    }
  }
}




// The UTC offset is calculated from the user's inputs and saved in the global variable 'utc_offset'.

void set_time() {
  int temp_UTC = 0;
  int temp_hour = 0;

  // Prompt user to enter hour
  while (true){
    display.clearDisplay();
    print_line("Enter hour: " + String(temp_hour) , 0, 0, 2);

    

    // Wait for button press
    int pressed = wait_for_button_press();

    // Adjust hour value based on button press
    if (pressed == UP){
      delay(200);
      temp_hour += 1;
      if (temp_hour > 14){
        temp_hour = -12;
      }
    }
    else if (pressed == DOWN){
      delay(200);
      temp_hour -= 1;
      if (temp_hour < -12){
        temp_hour = 14;
      }   
    }
    // Save UTC offset and exit loop on OK button press
    else if (pressed == OK){
      delay(200);
      temp_UTC = temp_hour*3600;
      break;
    }
    // Exit loop on CANCEL button press
    else if (pressed == CANCEL){
      delay(200);
      break;
    }

  }

  int temp_minute = 0;

  // Prompt user to enter minute
  while (true){
    display.clearDisplay();
    print_line("Enter minute: " + String(temp_minute) , 0, 0, 2);



    // Wait for button press
    int pressed = wait_for_button_press();

    // Adjust minute value based on button press
    if (pressed == UP){
      delay(200);
      temp_minute += 15;
      temp_minute = temp_minute % 60;
    }
    else if (pressed == DOWN){
      delay(200);
      temp_minute -= 15;
      if (temp_minute < 0){
        temp_minute = 45;
      }   
    }
    // Calculate and save UTC offset and exit loop on OK button press
    else if (pressed == OK){
      delay(200);
      if (temp_minute < 0){
        temp_minute = temp_minute*(-1);
      }
      temp_UTC += (temp_minute*60);
      utc_offset = temp_UTC;
      break;
    }
    // Exit loop on CANCEL button press
    else if (pressed == CANCEL){
      delay(200);
      break;
    }

  }

  // Display confirmation message
  display.clearDisplay();
  print_line("Time is set", 0, 0, 2);
  delay(1000);
}



void set_alarm(int alarm){

  // Get the current hour for the selected alarm
  int temp_hour = alarm_hours[alarm];

  // Loop until the user sets the alarm hour or cancels the operation
  while (true){
    // Clear the display and show the current hour
    display.clearDisplay();
    print_line("Enter hour: " + String(temp_hour) , 0, 0, 2);

    // Wait for a button press
    int pressed = wait_for_button_press();

    // If the UP button is pressed, increment the hour
    if (pressed == UP){
      delay(200);
      temp_hour += 1;
      temp_hour = temp_hour % 24;
    }

    // If the DOWN button is pressed, decrement the hour
    else if (pressed == DOWN){
      delay(200);
      temp_hour -= 1;
      if (temp_hour < 0){
        temp_hour = 23;
      }
    }

    // If the OK button is pressed, update the alarm hour and break the loop
    else if (pressed == OK){
      delay(200);
      alarm_hours[alarm]  = temp_hour;
      break;
    }

    // If the CANCEL button is pressed, break the loop and cancel the operation
    else if (pressed == CANCEL){
      delay(200);
      break;
    }
  }

  // Get the current minute for the selected alarm
  int temp_minute = alarm_minutes[alarm];

  // Loop until the user sets the alarm minute or cancels the operation
  while (true){
    // Clear the display and show the current minute
    display.clearDisplay();
    print_line("Enter minute: " + String(temp_minute) , 0, 0, 2);

    // Wait for a button press
    int pressed = wait_for_button_press();

    // If the UP button is pressed, increment the minute
    if (pressed == UP){
      delay(200);
      temp_minute += 1;
      temp_minute = temp_minute % 60;
    }

    // If the DOWN button is pressed, decrement the minute
    else if (pressed == DOWN){
      delay(200);
      temp_minute -= 1;
      if (temp_minute < 0){
        temp_minute = 59;
      }
    }

    // If the OK button is pressed, update the alarm minute and break the loop
    else if (pressed == OK){
      delay(200);
      alarm_minutes[alarm]  = temp_minute;
      break;
    }

    // If the CANCEL button is pressed, break the loop and cancel the operation
    else if (pressed == CANCEL){
      delay(200);
      break;
    }
  }

  // Clear the display, show a confirmation message, and wait for 1 second
  display.clearDisplay();
  print_line("Alarm is set", 0, 0, 2);
  delay(1000);

}


// This function allows the user to set the time or an alarm depending on the mode selected.
// It takes an integer mode as input which specifies the current mode of operation of the device.
void run_mode(int mode){
  // If the mode is 0, set_time() function is called to set the current time.
  if (mode == 0){
    set_time();
  }
  // If the mode is between 1 and 3 (inclusive), set_alarm() function is called to set the corresponding alarm.
  else if (mode == 1 || mode == 2 || mode == 3){
    set_alarm(mode-1);
  }
  // If the mode is 4, the alarm_enabled flag is set to false, disabling all alarms.
  else if (mode == 4){
    alarm_enabled = false;
  }
}

// This function reads the temperature and humidity data from the DHT11 sensor and checks if they are within acceptable ranges.
void check_temp(){
  // The temperature and humidity data is read using the getTempAndHumidity() function of the DHT11 library.
  TempAndHumidity data = dht_sensor.getTempAndHumidity();
  bool all_good = true;

  // If the temperature is greater than max_temp(32 degrees Celsius), the LED_2 is turned on and the "Temperature High" message is displayed.
  if (data.temperature > max_temp){
    all_good = false;
    digitalWrite(LED_2, HIGH);
    print_line("Temperature High", 1, 40, 0);
  }
  // If the temperature is less than min_temp(26 degrees Celsius), the LED_2 is turned on and the "Temperature Low" message is displayed.
  else if (data.temperature < min_temp){
    all_good = false;
    digitalWrite(LED_2, HIGH);
    print_line("Temperature Low", 1, 40, 0);
  }

  // If the humidity is greater than max_humidity(80%), the LED_2 is turned on and the "Humidity High" message is displayed.
  if (data.humidity > max_humidity){
    all_good = false;
    digitalWrite(LED_2, HIGH);
    print_line("Humidity High", 1, 50, 0);
  }
  // If the humidity is less than min_humidity(60%), the LED_2 is turned on and the "Humidity Low" message is displayed.
  else if (data.humidity < min_humidity){
    all_good = false;
    digitalWrite(LED_2, HIGH);
    print_line("Humidity Low", 1, 50, 0);
  }

  // If both temperature and humidity are within acceptable ranges, the LED_2 is turned off.
  if (all_good){
    digitalWrite(LED_2, LOW);
  }
}

