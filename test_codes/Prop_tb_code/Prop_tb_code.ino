/*
  Propulsion Testbench final code
  - Matches schematic (HX711 DT->A4, SCK->A5; SD CS->D8; E-MATCH MOSFET -> D9; Cal switch -> D2)
  - Calibration uses a known weight (in kg) and computes the scale factor
  - SD file opened once for the duration of the burn and closed at the end
  - Thrust computed in Newtons (kg * 9.80665)
*/

#include <Arduino.h>
#include <HX711.h>
#include <SD.h>

// Pin definitions based on schematic
const int LoadCell_Dt = A4;        // HX711 Data pin (DT)
const int LoadCell_sck = A5;       // HX711 Clock pin (SCK)
const int SD_CS = 8;               // SD Card CS pin (D8)
const int E_MATCH_PIN = 9;         // MOSFET control pin for E-match (D9)
const int CALIBRATION_SWITCH = 2;  // Calibration switch (D2)

// Global objects
HX711 loadcell;
File sdfile;
String filename;

// Test variables
float known_weight = 0.0f;    // in kg (user input during calibration)
unsigned long burnStartTime = 0;
unsigned long burnEndTime = 0; 
float thrust = 0.0f;          // Newtons
float impulse = 0.0f;        // N-s
float peakthrust = 0.0f;     // N
float avgthrust = 0.0f;      // N
unsigned long lastsampletime = 0;
float burntime = 0.0f;

// Control variables
unsigned long previoustime = 0;
int countdownTime = 15;
bool countdownstate = false;
bool hx_ini = false;
bool testInProgress = false;
unsigned long samplecount = 0;

// SD logging control
bool sdOpenForWrite = false;
unsigned long lastSdFlush = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(1); } // wait for Serial (USB native boards)

  Serial.println(F("=== Rocket Propulsion Test Bench ==="));
  Serial.println(F("E-match: One end connected directly to +12V, other end MOSFET-switched (D9)."));
  Serial.println();

  pinMode(CALIBRATION_SWITCH, INPUT_PULLUP);
  pinMode(E_MATCH_PIN, OUTPUT);
  digitalWrite(E_MATCH_PIN, LOW); // MOSFET OFF - E-match circuit open (SAFE)

  initializeHX711();
  initializeSD();

  Serial.println(F("System Ready. E-match circuit SAFE (MOSFET OFF)"));
  Serial.println(F("Press calibration switch to start test sequence."));
}

void loop() {
  // Check calibration switch to start test sequence
  if (digitalRead(CALIBRATION_SWITCH) == LOW && !countdownstate && !testInProgress) {
    delay(50); // simple debounce
    if (digitalRead(CALIBRATION_SWITCH) == LOW) {
      Serial.println(F("\n=== TEST SEQUENCE INITIATED ==="));
      hxCalibrate();
      startCountdown();
    }
  }

  // Handle countdown
  countDown();

  // Acquire data during test
  if (testInProgress) {
    acquireData();
    checkBurnComplete();
  }

  delay(10);
}

void initializeHX711() {
  // Use common-Bogde HX711 API
  loadcell.begin(LoadCell_Dt, LoadCell_sck);
  unsigned long startTime = millis();

  // small pause then wait for ready
  delay(500);
  while (!loadcell.is_ready()) {
    Serial.println(F("HX711 connecting..."));
    delay(500);
    if (millis() - startTime > 5000) {
      Serial.println(F("HX711 failed to initialize after multiple attempts."));
      while (1) {
        // stuck here because load cell is required
        delay(1000);
      }
    }
  }
  Serial.println(F("HX711 initialized"));
  hx_ini = true;
}

void initializeSD() {
  if (!SD.begin(SD_CS)) {
    Serial.println(F("SD card initialization FAILED"));
    while (1) { delay(1000); }
  } else {
    Serial.println(F("SD card initialized"));
    createNewSDfile();
  }
}

void createNewSDfile() {
  int i = 0;
  do {
    filename = String(i) + "_PLT.csv";
    i++;
  } while (SD.exists(filename));

  // create file and header, then close (we open later during test)
  File f = SD.open(filename, FILE_WRITE);
  if (f) {
    f.println("Elapsed Time_ms,Thrust_N,Impulse_Ns,AverageThrust_N,PeakThrust_N,BurnTime_s");
    f.close();
    Serial.println("Data file created: " + filename);
  } else {
    Serial.println(F("Error: File couldn't be created"));
  }
}

void hxCalibrate() {
  if (!hx_ini) {
    Serial.println(F("HX711 not initialized. Aborting calibration."));
    return;
  }

  Serial.println(F("\n--- HX711 Calibration ---"));
  Serial.println(F("Remove all weight from the loadcell and press ENTER when ready..."));
  while (!Serial.available()) { delay(10); }
  Serial.readStringUntil('\n'); // clear buffer

  // Tare (zero)
  Serial.println(F("Taring (zeroing) load cell..."));
  loadcell.tare(20); // common API
  delay(200);
  Serial.println(F("Tare completed."));

  Serial.println(F("Place known weight on the load cell and type its mass in kg (e.g., 0.5):"));
  while (!Serial.available()) { delay(10); }
  String s = Serial.readStringUntil('\n');
  s.trim();
  known_weight = s.toFloat();

  if (known_weight <= 0.0f) {
    Serial.println(F("Invalid weight entered. Calibration aborted."));
    return;
  }

  // Read a raw averaged value (get_units currently works as raw if scale==1)
  Serial.println(F("Measuring raw value... (be still)"));
  delay(500);
  // get_units before setting scale should give raw units (scale defaults to 1)
  float rawReading = loadcell.get_units(20); // typical Bogde API; returns raw if scale=1
  // Compute calibration factor to map rawReading -> known_weight (kg)
  float calFactor = rawReading / known_weight;
  loadcell.set_scale(calFactor);

  // Verify
  delay(200);
  float measured = loadcell.get_units(20); // now should be close to known_weight
  Serial.print(F("Calibration done. Known weight (kg): "));
  Serial.println(known_weight, 6);
  Serial.print(F("Measured after set_scale (kg): "));
  Serial.println(measured, 6);
  Serial.print(F("Calibration factor set: "));
  Serial.println(calFactor, 6);
  Serial.println(F("---------------------------"));
}

void startCountdown() {
  previoustime = millis();
  countdownstate = true;
  countdownTime = 15; // default
  Serial.println(F("Starting countdown sequence..."));
}

void countDown() {
  if (countdownstate) {
    if (millis() - previoustime >= 1000) {
      previoustime = millis();
      countdownTime--;
      Serial.println("T-" + String(countdownTime) + " s");

      if (countdownTime <= 0) {
        countdownstate = false;
        igniteMotor();
      }
    }
  }
}

void igniteMotor() {
  Serial.println(F("\nIGNITION!"));

  // Turn ON MOSFET to complete E-match circuit
  digitalWrite(E_MATCH_PIN, HIGH);
  delay(2000); // hold for 2 seconds for reliable ignition pulse
  digitalWrite(E_MATCH_PIN, LOW); // turn OFF MOSFET - return to safe state

  // Start test: prepare logging & variables
  testInProgress = true;
  burnStartTime = millis();
  lastsampletime = burnStartTime;
  samplecount = 0;
  impulse = 0.0f;
  peakthrust = 0.0f;
  avgthrust = 0.0f;

  // Open SD file for append and keep it open during the burn
  sdfile = SD.open(filename, FILE_WRITE);
  if (!sdfile) {
    Serial.println(F("Warning: Unable to open SD file for writing. Logging disabled."));
    sdOpenForWrite = false;
  } else {
    sdOpenForWrite = true;
    // position at end and flush header is already added earlier
  }

  Serial.println(F("Data acquisition started..."));
}

void acquireData() {
  unsigned long now = millis();
  // dt in seconds
  float dt = (now - lastsampletime) / 1000.0f;
  if (dt <= 0) return; // avoid divide by zero / negative

  // get_units() should return mass in kg after calibration. If you used grams for known_weight,
  // change known_weight input accordingly. Here we assume known_weight was given in kg.
  float mass_kg = loadcell.get_units(1); // 1-sample to be fast; increase if noisy
  if (mass_kg < 0) mass_kg = 0.0f;        // ignore negative readings
  thrust = mass_kg * 9.80665f;            // N (kg * m/s^2)

  // integrate impulse (N*s)
  impulse += thrust * dt;

  // update peak and rolling average
  if (thrust > peakthrust) peakthrust = thrust;

  samplecount++;
  // incremental average
  avgthrust += (thrust - avgthrust) / (float)samplecount;

  lastsampletime = now;
  burntime = (now - burnStartTime) / 1000.0f;

  // Write to SD file if open
  if (sdOpenForWrite) {
    sdfile.print(now - burnStartTime);          // elapsed ms
    sdfile.print(',');
    sdfile.print(thrust, 3);
    sdfile.print(',');
    sdfile.print(impulse, 6);
    sdfile.print(',');
    sdfile.print(avgthrust, 4);
    sdfile.print(',');
    sdfile.print(peakthrust, 3);
    sdfile.print(',');
    sdfile.println(burntime, 3);

    // flush every 500 ms to minimize data loss and SD wear
    if (millis() - lastSdFlush > 500) {
      sdfile.flush();
      lastSdFlush = millis();
    }
  }

  // Print status every 200ms
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 200) {
    Serial.print(F("Thrust: "));
    Serial.print(thrust, 2);
    Serial.print(F(" N | Impulse: "));
    Serial.print(impulse, 3);
    Serial.print(F(" N-s | Time: "));
    Serial.print(burntime, 2);
    Serial.print(F(" s | Samples: "));
    Serial.println(samplecount);
    lastPrint = millis();
  }
}

void checkBurnComplete() {
  static unsigned long lowThrustStart = 0;
  const float THRUST_THRESHOLD = 0.5; // Newtons - tune this based on your motor
  const unsigned long LOW_THRUST_DURATION = 3000; // ms to consider burn ended

  if (thrust < THRUST_THRESHOLD) {
    if (lowThrustStart == 0) lowThrustStart = millis();
    else if (millis() - lowThrustStart > LOW_THRUST_DURATION) {
      endBurn();
    }
  } else {
    lowThrustStart = 0;
  }

  // Safety timeout after 60 seconds
  if (burntime > 60.0f) {
    Serial.println(F("Safety timeout reached!"));
    endBurn();
  }
}

void endBurn() {
  if (!testInProgress) return;

  testInProgress = false;
  burnEndTime = millis();
  burntime = (burnEndTime - burnStartTime) / 1000.0f;

  // Ensure MOSFET is OFF for safety
  digitalWrite(E_MATCH_PIN, LOW);

  // Close SD file if open
  if (sdOpenForWrite) {
    sdfile.flush();
    sdfile.close();
    sdOpenForWrite = false;
  }

  // Final summary
  Serial.println(F("\n=== BURN COMPLETE ==="));
  Serial.print(F("Burn Time: "));
  Serial.print(burntime, 3);
  Serial.println(F(" seconds"));

  Serial.print(F("Total Impulse: "));
  Serial.print(impulse, 3);
  Serial.println(F(" N-s"));

  Serial.print(F("Peak Thrust: "));
  Serial.print(peakthrust, 3);
  Serial.println(F(" N"));

  Serial.print(F("Average Thrust: "));
  Serial.print(avgthrust, 3);
  Serial.println(F(" N"));

  Serial.print(F("Total Samples: "));
  Serial.println(samplecount);

  Serial.print(F("Data saved to: "));
  Serial.println(filename);

  Serial.println(F("\nReset Arduino to run another test."));
}
