#include <Wire.h>
#include <Adafruit_INA219.h>
#include <LiquidCrystal.h>
#include <math.h>

Adafruit_INA219 ina219;

// LCD pins: rs, en, d4, d5, d6, d7
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

bool button1 = 0;
bool button2 = 0;
bool button3 = 0;

bool LCD_state = 0;
bool charging_state = 0;
bool discharging_state = 0;

int Vo;
float R1 = 10000;
float logR2, R2, T, Tc;
float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;

// Auto switching control
unsigned long lowVoltageStart = 0;
unsigned long lowCurrentStart = 0;
const unsigned long switchDelay = 60000; // 1 minute
const float MIN_VOLTAGE = 3.2;
const float MIN_CURRENT = 1.0;
const float MIN_VOLTAGE_FOR_FULL = 4.13;

void setup() {
  pinMode(13, OUTPUT);  // Charging gate
  pinMode(8, OUTPUT);   // Discharging gate
  pinMode(10, INPUT);   // Button2: manual charging
  pinMode(9, INPUT);    // Button3: manual discharging
  pinMode(7, INPUT);    // Button1: LCD toggle

  Serial.begin(115200);
  while (!Serial) { delay(1); }

  if (!ina219.begin()) {
    Serial.println("Failed to find INA219 chip");
    while (1) { delay(10); }
  }

  ina219.setCalibration_32V_2A();
  lcd.begin(16, 2);
  lcd.clear();
}

void loop() {
  // --- Sensor Readings ---
  float shuntvoltage = ina219.getShuntVoltage_mV();
  float busvoltage = ina219.getBusVoltage_V();
  float current_mA = ina219.getCurrent_mA();
  float power_mW = ina219.getPower_mW();
  int sensorValue = analogRead(A1);
  float loadvoltage = (sensorValue * 5.0) / 1023;

  button2 = digitalRead(10);
  button3 = digitalRead(9);
  button1 = digitalRead(7);

  // --- Manual Controls ---
  if (button2 == 1) {
    charging_state = 1;
    discharging_state = 0;
  }
  if (button3 == 1) {
    charging_state = 0;
    discharging_state = 1;
  }

  unsigned long now = millis();

  // --- Auto Switching Logic ---
  // Start charging if voltage too low
  if (loadvoltage <= MIN_VOLTAGE) {
    if (lowVoltageStart == 0) lowVoltageStart = now;
    if (now - lowVoltageStart >= switchDelay) {
      charging_state = 1;
      discharging_state = 0;
    }
  } else {
    lowVoltageStart = 0;
  }
  // Start discharging if current is very low and voltage is high (battery full)
  if (charging_state == 1 && current_mA <= MIN_CURRENT && loadvoltage >= MIN_VOLTAGE_FOR_FULL) {
    if (lowCurrentStart == 0) lowCurrentStart = now;
    if (now - lowCurrentStart >= switchDelay) {
      discharging_state = 1;
      charging_state = 0;
    }
  } else {
    lowCurrentStart = 0;
  }

  // --- Actuate Charge/Discharge Gates ---
  digitalWrite(13, charging_state ? HIGH : LOW);
  digitalWrite(8, discharging_state ? HIGH : LOW);

  // --- Thermistor Temperature Calculation ---
  Vo = analogRead(A5);
  R2 = R1 * (1023.0 / (float)Vo - 1.0);
  logR2 = log(R2);
  T = (1.0 / (c1 + c2 * logR2 + c3 * pow(logR2, 3)));
  Tc = T - 273.15 - 22;  // Adjusted calibration offset

  // --- LCD Real-Time Display ---
  lcd.setCursor(0, 0);
  lcd.print("I:");
  lcd.print(current_mA, 1);
  lcd.print("mA ");

  lcd.setCursor(10, 0);  // Status on top-right
  if (charging_state) {
    lcd.print("C ");
  } else if (discharging_state) {
    lcd.print("D ");
  } else {
    lcd.print("  ");  // Clear if neither
  }

  lcd.setCursor(0, 1);
  lcd.print("V:");
  lcd.print(loadvoltage, 2);
  lcd.print("V ");

  lcd.print(Tc, 1);
  lcd.print((char)223);
  lcd.print("C ");

  // --- Serial Output for Python Logging ---
  Serial.print("BusVoltage:"); Serial.print(busvoltage); Serial.print("\t");
  Serial.print("ShuntVoltage:"); Serial.print(shuntvoltage); Serial.print("\t");
  Serial.print("LoadVoltage:"); Serial.print(loadvoltage); Serial.print("\t");
  Serial.print("Current_mA:"); Serial.print(current_mA); Serial.print("\t");
  Serial.print("Temperature:"); Serial.print(Tc); Serial.print("\t");
  Serial.print("Power_mW:"); Serial.println(power_mW);
  //delay(5000); // Log every 5 seconds
 // delay(1000); // Log every 1 seconds
}
