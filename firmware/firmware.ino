// Flex sensor pins
const uint8_t thumbPin = 1;
const uint8_t indexPin = 2;
const uint8_t middlePin = 4;
const uint8_t ringPin = 5;
const uint8_t pinkyPin = 6;

// Calibration variables
float acdStraight = 0; // Fixed typo from acdStright
float acdBend = 0;

// Filter variables
float filteredAdc = 0.0;
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
  
  // Calibration
  Serial.println("Calibration, straighten finger, press ENTER");
  waitForUser();
  
  long sumStraight = 0;
  for (int i = 0; i < 30; i++) {
    sumStraight += analogRead(indexPin);
    delay(15);
  }
  acdStraight = (float)sumStraight / 30.0;
  Serial.print("Saved Straight Reference (Around ~4000): ");
  Serial.println(acdStraight, 1);
  
  Serial.println("Calibration, bend finger, press ENTER");
  waitForUser();
  
  long sumBent = 0;
  for (int i = 0; i < 30; i++) {
    sumBent += analogRead(indexPin);
    delay(15);
  }
  acdBend = (float)sumBent / 30.0;
  Serial.print("Saved Bent Reference (Around ~3500): ");
  Serial.println(acdBend, 1);
  
  filteredAdc = analogRead(indexPin); // Seed the filter
}

void loop() {
  int rawValue = analogRead(indexPin);
  
  // Reject if 0
  if (rawValue <= 0) {
    delay(10);
    return;
  }
  
  filteredAdc = (alpha * (float)rawValue) + ((1.0 - alpha) * filteredAdc);
  
  float bendPercentage = mapToPercentage((float)filteredAdc, acdStraight, acdBend);
  bendPercentage = constrain(bendPercentage, 0.0, 100.0);
  
  Serial.println(bendPercentage, 1);
  delay(30);
}

// Change ADC reading to percentages
float mapToPercentage(float x, float in_min, float in_max) {
  return (x - in_min) * 100.0 / (in_max - in_min);
}
