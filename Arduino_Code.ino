//Include libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

//Define OLED parameters
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define BUZZER 5
#define LED_1 15
#define PB_CANCEL 34
#define PB_OK 32
#define PB_UP 33
#define PB_DOWN 35
#define DHTPIN 12
#define LDRPIN 36
#define SERVOPIN 18

#define NTP_SERVER     "pool.ntp.org"
#define UTC_OFFSET_DST 0

//Declare objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
Servo servo;

//Global variales
int years = 0;
int months = 0;
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

unsigned long timeNow = 0;
unsigned long timeLast = 0;

bool alarm_enable = true;
int n_alarms = 3;
int alarm_hours[] = {0, 0, 0};
int alarm_minutes[] = {1, 1, 1};
bool alarm_triggered[] = {false, false, false};

int n_notes = 8;
int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;
int notes[] = {C, D, E, F, G, A, B, C_H};

int current_mode = 0;
int max_modes = 5;
String modes[] = {"Set Time  Zone", "Set Alarm1", "Set Alarm2", "Set Alarm3", "Disable   Alarms"};

const float GAMMA = 0.7;
const float RL10 = 50;
const float MAXLUX = 85168.02;

char tempAr[6] = "30";
char LDR_chr[6];
float scaledLdrVal = 0.0;
int minAngle = 30;
float ctrlFactor = 0.75;
int theta = 0;
bool isScheduledON = false;
unsigned long scheduleOnTime;

float current_zone = 5.5;
float UTC_OFFSET = current_zone * 3600;
String time_zones[] = {"-12:00", "-11:00", "-10:00", "-09:30", "-09:00", "-08:00", "-07:00", "-06:00", "-05:00", "-04:00", "-03:30", "-03:00", "-02:00", "-01:00", "+00:00", "+01:00", "+02:00", "+03:00", "+03:30", "+04:00", "+04:30", "+05:00", "+05:30", "+05:45", "+06:00", "+06:30", "+07:00", "+08:00", "+08:45", "+09:00", "+09:30", "+10:00", "+10:30", "+11:00", "+12:00", "+12:45", "+13:00", "+14:00"};

void setup() {

  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);
  pinMode(DHTPIN, INPUT);
  pinMode(LDRPIN, INPUT);
  pinMode(SERVOPIN, OUTPUT);

  dhtSensor.setup(DHTPIN, DHTesp::DHT22);

  Serial.begin(9600);

  //SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  //Show the display buffer on the screen. You must call display() after
  //drawing commands to make them visible on screen!
  display.display();
  delay(500);


  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    print_line("Connectingto WIFI...", 0, 0, 2);
  }

  display.clearDisplay();
  print_line(" Connected  to WIFI", 0, 0, 2);
  delay(1000);
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

  display.clearDisplay();

  print_line("Welcome to Medibox!", 5, 20, 2);
  delay(500);
  display.clearDisplay();
  servo.attach(SERVOPIN, 500, 2400);
  setupMqtt();
  timeClient.begin();
}

void loop() {
  update_time_with_check_alarm();
  if (digitalRead(PB_OK) == LOW) {
    delay(200);
    go_to_menu();
  }
  check_temp();

  if (!mqttClient.connected())
    connectToBroker();

  mqttClient.loop();

  int ldrVal = analogRead(LDRPIN);
  ldrVal = map(ldrVal, 4095, 0, 1024, 0);
  float voltage = ldrVal / 1024. * 5;
  float resistance = 2000 * voltage / (1 - voltage / 5);

  float scaledLdrVal = pow(RL10 * 1e3 * pow(10, GAMMA) / resistance, (1 / GAMMA)) / MAXLUX;
  String(scaledLdrVal, 2).toCharArray(LDR_chr, 6);

  mqttClient.publish("CURRENT-TEMP", tempAr);
  mqttClient.publish("CURRENT-INTENSITY", LDR_chr);

  checkSchedule();
  delay(1000);

  theta = minAngle + (180 - minAngle) * ctrlFactor * scaledLdrVal;
  servo.write(theta);
  delay(50);
}

void checkSchedule() {
  if (isScheduledON) {
    unsigned long currentTime = getTime();
    if (currentTime > scheduleOnTime) {
      buzzerOn(true);
      isScheduledON = false;
      mqttClient.publish("MAIN-ON-OFF", "1");
      mqttClient.publish("SCHEDULE-ON", "0");
      Serial.println("Scheduled ON");
    }
  }
}

unsigned long getTime() {
  timeClient.update();
  return timeClient.getEpochTime();
}

void buzzerOn(bool on) {
  if (on)
    tone(BUZZER, 255);
  else
    noTone(BUZZER);
}

void connectToBroker() {
  while (!mqttClient.connected()) {
    Serial.println("Attempting MQTT connect");
    if (mqttClient.connect("ESP32-7658987465")) {
      Serial.println("Connected");
      mqttClient.subscribe("MIN-ANGLE");
      mqttClient.subscribe("CONTROL-FACTOR");
      mqttClient.subscribe("MAIN-ON-OFF-BUZZER");
      mqttClient.subscribe("SCH-ON-BUZZER");
    }
    else
    {
      Serial.print("Failed");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

void setupMqtt() {
  mqttClient.setServer("test.mosquitto.org", 1883);
  mqttClient.setCallback(receiveCallback);
}

void receiveCallback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char payloadCharAr[length];
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
    payloadCharAr[i] = (char)payload[i];
  }

  Serial.println();

  if (strcmp(topic, "MIN-ANGLE") == 0)
  {

    minAngle = atoi(payloadCharAr);
  }
  else if (strcmp(topic, "CONTROL-FACTOR") == 0)
  {

    ctrlFactor = atoi(payloadCharAr);
  } else if (strcmp(topic, "MAIN-ON-OFF-BUZZER") == 0) {
    buzzerOn(payloadCharAr[0] == '1'  );

  } else if (strcmp(topic, "SCH-ON-BUZZER") == 0) {
    if (payloadCharAr[0] == 'N') {
      isScheduledON = false;
    } else {
      isScheduledON = true;
      scheduleOnTime = atol(payloadCharAr);
    }

  }
}

void print_line(String text, int column, int row, float text_size) {
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(text);

  display.display();
}

void print_time_now(void) {
  display.clearDisplay();
  print_line("20" + String(years) + "/" + String(months) + "/" + String(days) + "  " + String(hours) + ":" + String(minutes) + ":" + String(seconds), 12, 0, 2);
}

void update_time() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  char timeHour[3];
  strftime(timeHour, 3, "%H", &timeinfo);
  hours = atoi(timeHour);

  char timeMinute[3];
  strftime(timeMinute, 3, "%M", &timeinfo);
  minutes = atoi(timeMinute);

  char timeSecond[3];
  strftime(timeSecond, 3, "%S", &timeinfo);
  seconds = atoi(timeSecond);

  char timeDay[3];
  strftime(timeDay, 3, "%d", &timeinfo);
  days = atoi(timeDay);

  char timeMonth[3];
  strftime(timeMonth, 3, "%m", &timeinfo);
  months = atoi(timeMonth);

  char timeYear[3];
  strftime(timeYear, 3, "%y", &timeinfo);
  years = atoi(timeYear);
}

void ring_alarm() {
  display.clearDisplay();
  print_line("Medicine  Time!", 0, 0, 2);

  digitalWrite(LED_1, HIGH);

  bool break_happened = false;

  //Ring the buzzer
  while (break_happened == false  && digitalRead(PB_CANCEL) == HIGH) {
    for (int i = 0; i < n_notes; i++) {
      if (digitalRead(PB_CANCEL) == LOW) {
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

  digitalWrite(LED_1, LOW);
  display.clearDisplay();

}

void update_time_with_check_alarm(void) {
  update_time();
  print_time_now();

  if (alarm_enable == true) {
    for (int i = 0; i < n_alarms; i++) {
      if (alarm_triggered[i] == false && alarm_hours[i] == hours && alarm_minutes[i] == minutes) {
        ring_alarm();
        alarm_triggered[i] = true;
      }
    }
  }
}

int wait_for_button_press() {
  while (true) {
    if (digitalRead(PB_UP) == LOW) {
      delay(200);
      return PB_UP;
    }

    else if (digitalRead(PB_DOWN) == LOW) {
      delay(200);
      return PB_DOWN;
    }

    else if (digitalRead(PB_OK) == LOW) {
      delay(200);
      return PB_OK;
    }

    else if (digitalRead(PB_CANCEL) == LOW) {
      delay(200);
      return PB_CANCEL;
    }

    update_time();
  }
}

void go_to_menu() {
  while (digitalRead(PB_CANCEL) == HIGH) {
    display.clearDisplay();
    print_line(modes[current_mode], 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      current_mode += 1;
      current_mode = current_mode % max_modes;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      current_mode -= 1;
      if (current_mode < 0) {
        current_mode = max_modes - 1;
      }
    }

    else if (pressed == PB_OK) {
      delay(200);
      run_mode(current_mode);
    }

    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }
}

//Select time zone
void set_time() {
  int temp_zone = 22;
  while (digitalRead(PB_CANCEL) == HIGH) {
    display.clearDisplay();
    print_line(time_zones[temp_zone], 0, 0, 2);

    int pressed = wait_for_button_press();

    if (pressed == PB_UP) {
      delay(200);
      temp_zone += 1;
      temp_zone = temp_zone % 38;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_zone -= 1;
      if (temp_zone < 0) {
        temp_zone = 37;
      }
    }

    else if (pressed == PB_OK) {
      delay(200);
      String zone_left = time_zones[temp_zone].substring(0, 3);
      String zone_right = time_zones[temp_zone].substring(4, 6);
      current_zone = (float)zone_left.toInt();

      if (zone_right == "30" && current_zone > 0) {
        current_zone += 0.5;
      }
      else if (zone_right == "45" && current_zone > 0) {
        current_zone += 0.75;
      }
      else if (zone_right == "30" && current_zone < 0) {
        current_zone -= 0.5;
      }
      else if (zone_right == "45" && current_zone < 0) {
        current_zone -= 0.75;
      }

      UTC_OFFSET = current_zone * 3600;
      configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
      break;
    }

    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }
  display.clearDisplay();
  print_line("Time zone is set      " + time_zones[temp_zone], 0, 0, 2);
  delay(1000);
}

//Set alarm time
void set_alarm(int alarm) {

  int temp_hour = alarm_hours[alarm];
  while (true) {
    display.clearDisplay();
    print_line("Enter     hours:" + String(temp_hour), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_hour += 1;
      temp_hour = temp_hour % 24;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_hour -= 1;
      if (temp_hour < 0) {
        temp_hour = 23;
      }
    }

    else if (pressed == PB_OK) {
      delay(200);
      alarm_hours[alarm] = temp_hour;
      break;
    }

    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  int temp_minute = alarm_minutes[alarm];
  while (true) {
    display.clearDisplay();
    print_line("Enter     minutes:" + String(temp_minute), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_minute += 1;
      temp_minute = temp_minute % 60;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_minute -= 1;
      if (temp_minute < 0) {
        temp_minute = 59;
      }
    }

    else if (pressed == PB_OK) {
      delay(200);
      alarm_minutes[alarm] = temp_minute;
      break;
    }

    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  display.clearDisplay();
  print_line("Alarm  " + String(alarm + 1) + "  is set", 0, 0, 2);
  alarm_enable = true;
  alarm_triggered[alarm] = false;
  delay(1000);
}

//Select mode
void run_mode(int mode) {
  if (mode == 0) {
    set_time();
  }

  else if (mode == 1 || mode == 2 || mode == 3) {
    set_alarm(mode - 1);
  }

  else if (mode == 4) {
    alarm_enable = false;
    display.clearDisplay();
    print_line("Alarms are disable", 0, 0, 2);
    delay(1000);
  }

}

//Check temperature and Humidity & warning
void check_temp() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.temperature, 2).toCharArray(tempAr, 6);
  if (data.temperature > 32) {
    print_line("      TEMP HIGH", 0, 40, 1.7);
    digitalWrite(LED_1, HIGH);
    delay(500);
    digitalWrite(LED_1, LOW);
    delay(500);
  }
  else if (data.temperature < 26) {
    print_line("      TEMP LOW", 0, 40, 1.7);
    digitalWrite(LED_1, HIGH);
    delay(500);
    digitalWrite(LED_1, LOW);
    delay(500);
  }

  if (data.humidity > 80) {
    print_line("    HUMIDITY HIGH", 0, 50, 1.7);
    digitalWrite(LED_1, HIGH);
    delay(500);
    digitalWrite(LED_1, LOW);
    delay(500);
  }
  else if (data.humidity < 60) {
    print_line("    HUMIDITY LOW", 0, 50, 1.7);
    digitalWrite(LED_1, HIGH);
    delay(500);
    digitalWrite(LED_1, LOW);
    delay(500);
  }
}