// Flex sensor pins
const uint8_t thumbPin = 1;
const uint8_t indexPin = 2;
const uint8_t middlePin = 4;
const uint8_t ringPin = 5;
const uint8_t pinkyPin = 6;

// Put all pins in one array
const uint8_t flexPins[5] = {
  thumbPin,
  indexPin,
  middlePin,
  ringPin,
  pinkyPin
};

// Calibration variables for all 5 fingers
float acdStraight[5] = {0, 0, 0, 0, 0};
float acdBend[5] = {0, 0, 0, 0, 0};

// Filter variables for all 5 fingers
float filteredAdc[5] = {0, 0, 0, 0, 0};
const float alpha = 0.15;

void waitForUser() {
  while (Serial.available() > 0) {
    Serial.read();
  }
  while (Serial.available() == 0) {
    delay(10);
  }
}

void setup() {
  Serial.begin(115200);

  analogSetAttenuation(ADC_2_5db); // The flex sensor only reaches about 1.49V. ADC_2_5db makes it focus 
                                   // the measurement window from 0V to 1.5V instead of 3.3V. This leads 
                                   // the tracking data to a ~ 3500-4000 range

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

  Serial.println("thumb,index,middle,ring,pinky");
}

void loop() {
  float bendPercentage[5];

  for (int finger = 0; finger < 5; finger++) {
    int rawValue = analogRead(flexPins[finger]);

    // Reject if 0
    if (rawValue <= 0) {
      bendPercentage[finger] = 0;
      continue;
    }

    filteredAdc[finger] = (alpha * (float)rawValue) + ((1.0 - alpha) * filteredAdc[finger]);

    bendPercentage[finger] = mapToPercentage((float)filteredAdc[finger], acdStraight[finger], acdBend[finger]);
    bendPercentage[finger] = constrain(bendPercentage[finger], 0.0, 100.0);
  }

  // Print all 5 fingers
  Serial.print(bendPercentage[0], 1);
  Serial.print(",");
  Serial.print(bendPercentage[1], 1);
  Serial.print(",");
  Serial.print(bendPercentage[2], 1);
  Serial.print(",");
  Serial.print(bendPercentage[3], 1);
  Serial.print(",");
  Serial.println(bendPercentage[4], 1);

  delay(30);
}

// Change ADC reading to percentages
float mapToPercentage(float x, float in_min, float in_max) {
  return (x - in_min) * 100.0 / (in_max - in_min);
}