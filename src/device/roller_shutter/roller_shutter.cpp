#include "roller_shutter.hpp"

const unsigned char upOutputPin = 4;
const unsigned char downOutputPin = 15;
const unsigned char upInputPin = 13;
const unsigned char downInputPin = 5;

// Init motor timer
unsigned long startedDrivingMillis = 0;

// Create power consumption sensor
Lumos::ADE7953Sensor ade7953Sensor(12, 14);
Lumos::NtcSensor ntcSensor(3350, 32000, 8000, (273.15 + 25.0));

// Webthings device types
const char *deviceTypesRollerShutter[] = { nullptr };

enum DrivingMode {
  STOP, UP, DOWN
} drivingMode;

ThingActionObject* stop_action(DynamicJsonDocument *input) {
  drivingMode = DrivingMode::STOP;
  return nullptr;
}

ThingActionObject* up_action(DynamicJsonDocument *input) {
  drivingMode = DrivingMode::UP;
  return nullptr;
}

ThingActionObject* down_action(DynamicJsonDocument *input) {
  drivingMode = DrivingMode::DOWN;
  return nullptr;
}

Lumos::RollerShutter::RollerShutter(const char *title)
  : Device(title, deviceTypesRollerShutter),
    stopAction("stop", "Stop", "Whether the shutter stops driving", "ToggleAction", nullptr, stop_action),
    upAction("up", "Up", "Whether the shutter drives up", "ToggleAction", nullptr, up_action),
    downAction("down", "Down", "Whether the shutter drives down", "ToggleAction", nullptr, down_action),
    powerProperty("power", "The current power consumption in watt", NUMBER, "InstantaneousPowerProperty"),
    temperatureProperty("temperature", "The current temperature of the device in °C", NUMBER, "TemperatureProperty") {

  // Add actions
  device.addAction(&stopAction);
  device.addAction(&upAction);
  device.addAction(&downAction);

  // Add properties
  device.addProperty(&powerProperty);
  ThingPropertyValue powerValue;
  powerValue.number = 0.0;
  powerProperty.setValue(powerValue);

  device.addProperty(&temperatureProperty);
  ThingPropertyValue temperatureValue;
  temperatureValue.number = 0.0;
  temperatureProperty.setValue(temperatureValue);

  // Init pins
  pinMode(upOutputPin, OUTPUT);
  digitalWrite(upOutputPin, LOW);

  pinMode(downOutputPin, OUTPUT);
  digitalWrite(downOutputPin, LOW);

  pinMode(upInputPin, INPUT_PULLUP);
  pinMode(downInputPin, INPUT_PULLUP);

  // Init ADE7953 power sensor
  ade7953Sensor.init();
};

void Lumos::RollerShutter::handle(void) {
  unsigned long currentMillis = millis();

  // Check for < 0 because of the overflow
  static unsigned long last500ms = 0;
  if ((currentMillis - last500ms) >= 500 || (currentMillis - last500ms) < 0) {
    last500ms = currentMillis;

    int upButton = digitalRead(upInputPin);
    int downButton = digitalRead(downInputPin);
    if (upButton == HIGH && downButton == HIGH) {
      drivingMode = DrivingMode::STOP;
    } else {
      if (upButton == HIGH && drivingMode == DrivingMode::STOP) {
        drivingMode = DrivingMode::UP;
      } else if (downButton == HIGH && drivingMode == DrivingMode::STOP) {
        drivingMode = DrivingMode::DOWN;
      } else {
        drivingMode = DrivingMode::STOP;
      }
    }

    switch (drivingMode) {
      case STOP:
        digitalWrite(upOutputPin, LOW);
        digitalWrite(downOutputPin, LOW);
        startedDrivingMillis = 0;
        break;

      case UP:
        digitalWrite(downOutputPin, LOW);
        digitalWrite(upOutputPin, HIGH);
        startedDrivingMillis = currentMillis;
        break;

      case DOWN:
        digitalWrite(upOutputPin, LOW);
        digitalWrite(downOutputPin, HIGH);
        startedDrivingMillis = currentMillis;
        break;
    }
  }

  // Stop motor after 2 minutes
  if ((currentMillis - startedDrivingMillis) >= 120000 && startedDrivingMillis != 0) {
    Serial.println("Driving for 2 minutes already => STOP");
    drivingMode = DrivingMode::STOP;
  }

  // Only update sensors all 2 seconds
  static unsigned long last1sMillis = millis();
  if ((currentMillis - last1sMillis) >= 2000 || (currentMillis - last1sMillis) < 0) {
    last1sMillis = currentMillis;

    // Power consumption
    static float old_power = 0.0;
    float power = ade7953Sensor.getPower();
    if (abs(power - old_power) >= 2) {
      old_power = power;

      ThingPropertyValue powerValue;
      powerValue.number = power;
      powerProperty.setValue(powerValue);

      Serial.print("Update power consumption: ");
      Serial.print(power);
      Serial.println("W");
    }

    // Auto relay stop by power consumption
    if (power < 10 && startedDrivingMillis != 0) {
      Serial.println("Reached top or bottom => STOP");
      drivingMode = DrivingMode::STOP;
    }

    // Temperature
    static double old_temperature = 0.0;
    float temperature = ntcSensor.getTemperature();
    if (abs(temperature - old_temperature) >= 20) {
      old_temperature = temperature;

      ThingPropertyValue temperatureValue;
      temperatureValue.number = temperature;
      temperatureProperty.setValue(temperatureValue);

      Serial.print("Update temperature: ");
      Serial.print(temperature);
      Serial.println("°C");
    }
  }
}
