#include <Wire.h>

// ---------------- Flex sensor pins ----------------
const uint8_t thumbPin  = 1;
const uint8_t indexPin  = 2;
const uint8_t middlePin = 4;
const uint8_t ringPin   = 5;
const uint8_t pinkyPin  = 6;

const uint8_t flexPins[5] = {
  thumbPin,
  indexPin,
  middlePin,
  ringPin,
  pinkyPin
};

// ---------------- Touch sensor pins ----------------
// T-I, I-M, M-R, R-P (between-finger contact sensors)
const uint8_t tiPin = 7;
const uint8_t imPin = 15;
const uint8_t mrPin = 16;
const uint8_t rpPin = 17;

const uint8_t touchPins[4] = {
  tiPin,
  imPin,
  mrPin,
  rpPin
};

// ---------------- MPU6050 pins ----------------
const uint8_t sdaPin = 8;
const uint8_t sclPin = 9;

// ---------------- Flex calibration ----------------
float acdStraight[5] = {0, 0, 0, 0, 0};
float acdBend[5] = {0, 0, 0, 0, 0};
float bendPercentage[5] = {0, 0, 0, 0, 0};

// ---------------- Flex filter ----------------
float filteredAdc[5] = {0, 0, 0, 0, 0};
const float alpha = 0.15;

// ---------------- MPU6050 registers ----------------
const uint8_t MPU_ADDR     = 0x68;
const uint8_t WHO_AM_I     = 0x75;
const uint8_t PWR_MGMT_1   = 0x6B;
const uint8_t ACCEL_XOUT_H = 0x3B;

// ---------------- MPU6050 scale factors ----------------
const float ACCEL_SCALE = 16384.0;
const float GYRO_SCALE  = 131.0;

// ---------------- Raw / converted MPU6050 values ----------------
int16_t accelRaw[3] = {0, 0, 0};
int16_t gyroRaw[3]  = {0, 0, 0};

float accelG[3]  = {0.0, 0.0, 0.0};
float gyroDps[3] = {0.0, 0.0, 0.0};
float accelOffset[3] = {0.0, 0.0, 0.0};

// ---------------- MPU6050 helpers ----------------
void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

uint8_t readRegister(uint8_t reg) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);

  Wire.requestFrom(MPU_ADDR, (uint8_t)1);

  if (Wire.available()) {
    return Wire.read();
  }

  return 0;
}

int16_t combineBytes(uint8_t highByte, uint8_t lowByte) {
  return (int16_t)((highByte << 8) | lowByte);
}

bool readMPU6050Burst() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(ACCEL_XOUT_H);
  Wire.endTransmission(false);

  Wire.requestFrom(MPU_ADDR, (uint8_t)14);

  if (Wire.available() < 14) {
    return false;
  }

  uint8_t data[14];

  for (int i = 0; i < 14; i++) {
    data[i] = Wire.read();
  }

  accelRaw[0] = combineBytes(data[0], data[1]);
  accelRaw[1] = combineBytes(data[2], data[3]);
  accelRaw[2] = combineBytes(data[4], data[5]);

  gyroRaw[0] = combineBytes(data[8], data[9]);
  gyroRaw[1] = combineBytes(data[10], data[11]);
  gyroRaw[2] = combineBytes(data[12], data[13]);

  for (int i = 0; i < 3; i++) {
    accelG[i]  = accelRaw[i] / ACCEL_SCALE;
    gyroDps[i] = gyroRaw[i] / GYRO_SCALE;
  }

  return true;
}

// ---------------- Utility ----------------
void waitForUser() {
  while (Serial.available() > 0) {
    Serial.read();
  }

  while (Serial.available() == 0) {
    delay(10);
  }

  while (Serial.available() > 0) {
    Serial.read();
  }
}

float mapToPercentage(float x, float in_min, float in_max) {
  if (abs(in_max - in_min) < 1.0) {
    return 0.0;
  }

  return (x - in_min) * 100.0 / (in_max - in_min);
}

// Reads a touch pin and returns 1 if fingers are touching, 0 if apart.
// INPUT_PULLUP means: open = HIGH, touching (shorted to GND) = LOW.
int readTouch(uint8_t pin) {
  return (digitalRead(pin) == LOW) ? 1 : 0;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // ---------------- MPU6050 setup ----------------
  Wire.begin(sdaPin, sclPin);
  Wire.setClock(400000);

  uint8_t whoami = readRegister(WHO_AM_I);

  Serial.print("WHO_AM_I = 0x");
  Serial.println(whoami, HEX);

  if (whoami == 0x68 || whoami == 0x70) {
    Serial.println("MPU6050 detected.");
  } else {
    Serial.println("MPU6050 not detected.");
  }

  writeRegister(PWR_MGMT_1, 0x00);
  delay(100);

  // ---------------- Touch sensor setup ----------------
  for (int i = 0; i < 4; i++) {
    pinMode(touchPins[i], INPUT_PULLUP);
  }

  // ---------------- Flex sensor setup ----------------
  analogSetAttenuation(ADC_6db);

  // Calibration - open hand
  Serial.println("Calibration, open/straighten hand, press ENTER");
  waitForUser();

  long sumStraight[5] = {0, 0, 0, 0, 0};

  for (int i = 0; i < 30; i++) {
    for (int finger = 0; finger < 5; finger++) {
      sumStraight[finger] += analogRead(flexPins[finger]);
    }
    delay(15);
  }

  for (int finger = 0; finger < 5; finger++) {
    acdStraight[finger] = (float)sumStraight[finger] / 30.0;
  }

  Serial.println("Saved Straight References:");
  for (int finger = 0; finger < 5; finger++) {
    Serial.println(acdStraight[finger], 1);
  }

  // Calibration - closed fist
  Serial.println("Calibration, close fist/bend fingers, press ENTER");
  waitForUser();

  long sumBent[5] = {0, 0, 0, 0, 0};

  for (int i = 0; i < 30; i++) {
    for (int finger = 0; finger < 5; finger++) {
      sumBent[finger] += analogRead(flexPins[finger]);
    }
    delay(15);
  }

  for (int finger = 0; finger < 5; finger++) {
    acdBend[finger] = (float)sumBent[finger] / 30.0;
  }

  Serial.println("Saved Bent References:");
  for (int finger = 0; finger < 5; finger++) {
    Serial.println(acdBend[finger], 1);
  }

  // Seed filters
  for (int finger = 0; finger < 5; finger++) {
    filteredAdc[finger] = analogRead(flexPins[finger]);
  }

  Serial.println("Accel calibration, hold hand flat in front of body, press ENTER");
  waitForUser();

  float sumAccel[3] = {0.0, 0.0, 0.0};

  for (int i = 0; i < 30; i++) {
    readMPU6050Burst();
    sumAccel[0] += accelG[0];
    sumAccel[1] += accelG[1];
    sumAccel[2] += accelG[2];
    delay(15);
  }

  for (int i = 0; i < 3; i++) {
    accelOffset[i] = sumAccel[i] / 30.0;
  }

  Serial.println("Accel offset saved:");
  Serial.print(accelOffset[0], 3); Serial.print(",");
  Serial.print(accelOffset[1], 3); Serial.print(",");
  Serial.println(accelOffset[2], 3);

  Serial.println("thumb,index,middle,ring,pinky,ti,im,mr,rp,accelX,accelY,accelZ,gyroX,gyroY,gyroZ");
  }

void loop() {
  bool success = readMPU6050Burst();

  if (!success) {
    Serial.println("MPU6050 read failed");
    delay(100);
    return;
  }

  // ---------------- Flex sensors ----------------

  for (int finger = 0; finger < 5; finger++) {
    int rawValue = analogRead(flexPins[finger]);

    if (rawValue > 0) {
    filteredAdc[finger] = (alpha * (float)rawValue) + ((1.0 - alpha) * filteredAdc[finger]);
    bendPercentage[finger] = mapToPercentage(filteredAdc[finger], acdStraight[finger], acdBend[finger]);
    bendPercentage[finger] = constrain(bendPercentage[finger], 0.0, 100.0);
    }

  }

  // ---------------- Touch sensors ----------------
  int touchState[4];
  for (int i = 0; i < 4; i++) {
    touchState[i] = readTouch(touchPins[i]);
  }

  // ---------------- Serial output ----------------
  // thumb,index,middle,ring,pinky,ti,im,mr,rp,accelX,accelY,accelZ,gyroX,gyroY,gyroZ
  Serial.print(bendPercentage[0], 1);
  Serial.print(",");
  Serial.print(bendPercentage[1], 1);
  Serial.print(",");
  Serial.print(bendPercentage[2], 1);
  Serial.print(",");
  Serial.print(bendPercentage[3], 1);
  Serial.print(",");
  Serial.print(bendPercentage[4], 1);
  Serial.print(",");

  Serial.print(touchState[0]);
  Serial.print(",");
  Serial.print(touchState[1]);
  Serial.print(",");
  Serial.print(touchState[2]);
  Serial.print(",");
  Serial.print(touchState[3]);
  Serial.print(",");

  Serial.print(accelG[0] - accelOffset[0], 3); 
  Serial.print(",");
  Serial.print(accelG[1] - accelOffset[1], 3); 
  Serial.print(",");
  Serial.print(accelG[2] - accelOffset[2], 3); 
  Serial.print(",");
  Serial.print(gyroDps[0], 2);
  Serial.print(",");
  Serial.print(gyroDps[1], 2);
  Serial.print(",");
  Serial.println(gyroDps[2], 2);

  delay(30);
}