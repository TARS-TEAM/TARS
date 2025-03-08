#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <DS1302.h>

// ----- DS1302 RTC Setup -----
// DS1302 uses a 3-wire interface
// Connections: CE -> A4, I/O -> A5, SCLK -> D9
#define CE_PIN   A4
#define IO_PIN   A5
#define SCLK_PIN 9
DS1302 rtc(SCLK_PIN, IO_PIN, CE_PIN);

// ----- Fingerprint Sensors -----
// Sensor 1 (Entry Verification) on pins 2 (RX) and 3 (TX)
SoftwareSerial fingerSerial1(2, 3);
Adafruit_Fingerprint finger1 = Adafruit_Fingerprint(&fingerSerial1);

// Sensor 2 (Vote Confirmation) on pins 4 (RX) and 5 (TX)
SoftwareSerial fingerSerial2(4, 5);
Adafruit_Fingerprint finger2 = Adafruit_Fingerprint(&fingerSerial2);

// ----- GSM Module -----
// For simulation, we'll use Serial output. In a real implementation, use TinyGSM.
// GSM Module TX -> D12, RX -> D13
// (Here, we simulate messaging via Serial.)
#define GSM_TX_PIN 12
#define GSM_RX_PIN 13

// ----- Buzzer -----
// Buzzer connected to digital pin A1 (configured as digital output)
#define BUZZER_PIN A1

// ----- LED Definitions -----
// Entry Verification LEDs
#define LED_GREEN 6    // Verified entry
#define LED_YELLOW 7   // Employee mismatch/wrong shift
#define LED_RED 8      // Error/illegal vote (along with buzzer)
// Vote Confirmation LEDs
#define VOTE_GREEN 10  // Vote confirmed
#define VOTE_RED 11    // Vote confirmation failed

// ----- Party Vote Button Pins -----
// (Assume each button is connected to its own digital pin with internal pull-ups)
#define PARTY_BTN_1 14   // A0 can also be used as digital pin 14, or use digital pins if available.
#define PARTY_BTN_2 15   // (For example, if your board supports more digital I/O, otherwise, modify accordingly.)
#define PARTY_BTN_3 16
#define PARTY_BTN_4 17

// ----- Shift Timing (24-hour clock) -----
// Shift 1: 9:00 AM - 11:00 AM
// Shift 2: 11:00 AM - 1:00 PM
// Shift 3: 1:00 PM - 3:00 PM
// Shift 4: 3:00 PM - 4:00 PM (Final Slot)
#define SHIFT1_START 9
#define SHIFT1_END   11
#define SHIFT2_START 11
#define SHIFT2_END   13
#define SHIFT3_START 13
#define SHIFT3_END   15
#define SHIFT4_START 15
#define SHIFT4_END   16

// ----- Global Vote Counters -----
int party1Votes = 0;
int party2Votes = 0;
int party3Votes = 0;
int party4Votes = 0;
bool finalResultsPrinted = false;

// ----- Voter Vote Flags (to track if a voter has already voted) -----
// Example IDs: Voter 1: 201, Voter 2: 202, Voter 3: 203, Voter 4: 204, Voter 5: 205, Voter 6: 206, Voter 7: 207
bool voted201 = false;
bool voted202 = false;
bool voted203 = false;
bool voted204 = false;
bool voted205 = false;
bool voted206 = false;
bool voted207 = false;

// ----------------- Function Prototypes -----------------
uint8_t getFingerprintID(Adafruit_Fingerprint &finger);
int getCurrentShift();
void simulateDigitalSlip(int currentShift);
void sendSMS(String number, String message);
void buzzError();
void entryVerification();
void voteVerification(int selectedParty);
void simulateReminderMessages();
void markVoted(int voterID);

// ----------------- Setup -----------------
void setup() {
  Serial.begin(9600);
  while (!Serial);

  // Initialize buzzer pin
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize LED pins
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(VOTE_GREEN, OUTPUT);
  pinMode(VOTE_RED, OUTPUT);

  // Initialize party vote button pins (set as INPUT_PULLUP)
  pinMode(PARTY_BTN_1, INPUT_PULLUP);
  pinMode(PARTY_BTN_2, INPUT_PULLUP);
  pinMode(PARTY_BTN_3, INPUT_PULLUP);
  pinMode(PARTY_BTN_4, INPUT_PULLUP);

  // Initialize DS1302 RTC
  rtc.halt(false); // Start the RTC if it isn't running
  // You can set the RTC time here if needed, e.g.:
  // rtc.time(__TIME__, __DATE__);

  // Initialize Fingerprint Sensor 1 (Entry Verification)
  fingerSerial1.begin(57600);
  finger1.begin(57600);
  if (finger1.verifyPassword()) {
    Serial.println("Fingerprint sensor 1 (Entry) found!");
  } else {
    Serial.println("Fingerprint sensor 1 not found.");
    while (1);
  }

  // Initialize Fingerprint Sensor 2 (Vote Confirmation)
  fingerSerial2.begin(57600);
  finger2.begin(57600);
  if (finger2.verifyPassword()) {
    Serial.println("Fingerprint sensor 2 (Vote Confirmation) found!");
  } else {
    Serial.println("Fingerprint sensor 2 not found.");
    while (1);
  }

  Serial.println("System Initialized. Ready for voting process.");
}

// ----------------- Main Loop -----------------
void loop() {
  // Simulate reminder messages to non-voters periodically
  simulateReminderMessages();

  // Check if election period is over (after 4 PM)
  Time t = rtc.time();  // DS1302 returns a Time struct
  if (t.hour >= SHIFT4_END && !finalResultsPrinted) {
    Serial.println("Election period is over. Final Results:");
    Serial.print("Party 1 Votes: "); Serial.println(party1Votes);
    Serial.print("Party 2 Votes: "); Serial.println(party2Votes);
    Serial.print("Party 3 Votes: "); Serial.println(party3Votes);
    Serial.print("Party 4 Votes: "); Serial.println(party4Votes);
    int maxVotes = party1Votes;
    int winner = 1;
    if (party2Votes > maxVotes) { maxVotes = party2Votes; winner = 2; }
    if (party3Votes > maxVotes) { maxVotes = party3Votes; winner = 3; }
    if (party4Votes > maxVotes) { maxVotes = party4Votes; winner = 4; }
    Serial.print("Winner is Party "); Serial.print(winner);
    Serial.print(" with "); Serial.print(maxVotes);
    Serial.println(" votes.");
    Serial.println("Voters who cast their vote receive government incentives (tax waivers/subsidies).");
    finalResultsPrinted = true;
    while (1);  // Halt further voting
  }

  // Determine current shift using DS1302
  int currentShift = getCurrentShift();
  if (currentShift == 0) {
    Serial.println("Not within any voting slot period. Waiting...");
    delay(5000);
    return;
  }
  Serial.print("Current Shift: "); Serial.println(currentShift);

  // Simulate sending digital slip or reminder for the current shift
  simulateDigitalSlip(currentShift);

  // --- Entry Verification ---
  entryVerification();

  // --- Vote Selection via Push Buttons ---
  Serial.println("Waiting for party selection... (Press a party button)");
  int selectedParty = 0;
  while (selectedParty == 0) {
    if (digitalRead(PARTY_BTN_1) == LOW) { selectedParty = 1; }
    else if (digitalRead(PARTY_BTN_2) == LOW) { selectedParty = 2; }
    else if (digitalRead(PARTY_BTN_3) == LOW) { selectedParty = 3; }
    else if (digitalRead(PARTY_BTN_4) == LOW) { selectedParty = 4; }
  }
  delay(200);  // Debounce
  Serial.print("Party "); Serial.print(selectedParty); Serial.println(" selected.");

  // --- Vote Confirmation ---
  voteVerification(selectedParty);

  Serial.println("Vote recorded. Thank you for voting.");
  delay(5000);  // Wait before processing next voter
}

// ----------------- Fingerprint Matching Function -----------------
uint8_t getFingerprintID(Adafruit_Fingerprint &finger) {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return p;
  
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return p;
  
  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) {
    Serial.println("Fingerprint not found.");
    return p;
  }
  
  Serial.print("Found ID #");
  Serial.print(finger.fingerID);
  Serial.print(" with confidence: ");
  Serial.println(finger.confidence);
  return finger.fingerID;
}

// ----------------- DS1302-based Shift Allocation -----------------
// DS1302 library returns a Time struct with members: hour, minute, second, etc.
int getCurrentShift() {
  Time t = rtc.time();
  int hour = t.hour;
  if (hour >= SHIFT1_START && hour < SHIFT1_END) return 1;
  else if (hour >= SHIFT2_START && hour < SHIFT2_END) return 2;
  else if (hour >= SHIFT3_START && hour < SHIFT3_END) return 3;
  else if (hour >= SHIFT4_START && hour < SHIFT4_END) return 4;
  else return 0;
}

// ----------------- Digital Slip / Reminder Simulation -----------------
void simulateDigitalSlip(int currentShift) {
  Serial.println("=== Digital Slip / Reminder ===");
  Serial.print("Your assigned voting slot is: ");
  switch (currentShift) {
    case 1: Serial.println("9:00 AM - 11:00 AM"); break;
    case 2: Serial.println("11:00 AM - 1:00 PM"); break;
    case 3: Serial.println("1:00 PM - 3:00 PM"); break;
    case 4: Serial.println("3:00 PM - 4:00 PM"); break;
    default: Serial.println("Slot not assigned."); break;
  }
  // In a full implementation, use sendSMS() here.
  Serial.println("Please cast your vote in your current slot.");
  Serial.println("====================================");
  delay(3000);
}

// ----------------- GSM Messaging Simulation -----------------
void sendSMS(String number, String message) {
  // For simulation, output via Serial.
  Serial.print("SMS to "); Serial.print(number); Serial.print(": ");
  Serial.println(message);
}

// ----------------- Buzzer Activation -----------------
void buzzError() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(1000);  // Buzz for 1 second
  digitalWrite(BUZZER_PIN, LOW);
}

// ----------------- Reminder Messages to Non-Voters -----------------
void simulateReminderMessages() {
  Serial.println("=== Reminder Messages ===");
  if (!voted201) Serial.println("Reminder: Voter 1 (ID 201), please vote.");
  if (!voted202) Serial.println("Reminder: Voter 2 (ID 202), please vote.");
  if (!voted203) Serial.println("Reminder: Voter 3 (ID 203), please vote.");
  if (!voted204) Serial.println("Reminder: Voter 4 (ID 204), please vote.");
  if (!voted205) Serial.println("Reminder: Voter 5 (ID 205), please vote.");
  if (!voted206) Serial.println("Reminder: Voter 6 (ID 206), please vote.");
  if (!voted207) Serial.println("Reminder: Voter 7 (ID 207), please vote.");
  Serial.println("=========================");
  delay(2000);
}

// ----------------- Mark Voter as Having Voted -----------------
void markVoted(int voterID) {
  switch(voterID) {
    case 201: voted201 = true; break;
    case 202: voted202 = true; break;
    case 203: voted203 = true; break;
    case 204: voted204 = true; break;
    case 205: voted205 = true; break;
    case 206: voted206 = true; break;
    case 207: voted207 = true; break;
  }
}

// ----------------- Entry Verification -----------------
// Verifies the employee and voter using Fingerprint Sensor 1.
void entryVerification() {
  int currentShift = getCurrentShift();
  if (currentShift == 0) {
    Serial.println("Entry verification attempted outside shift hours.");
    return;
  }
  
  // Determine expected employee and allowed voter(s) based on shift.
  int expectedEmployee = 0;
  int expectedVoter = -1;
  // For simplicity, here are example assignments:
  if (currentShift == 1) { expectedEmployee = 101; expectedVoter = 201; }
  else if (currentShift == 2) { expectedEmployee = 102; expectedVoter = 204; } // In shift 2, cheating may occur if Voter 6 (ID 206) shows up.
  else if (currentShift == 3) { expectedEmployee = 103; expectedVoter = 204; } // Example: Voter 3 may impersonate Voter 4.
  else if (currentShift == 4) { expectedEmployee = 104; } // Shift 4: Allowed voters: 202 or 203 (for example)

  // --- Employee Verification ---
  Serial.println("Place EMPLOYEE fingerprint (Thumb) on Sensor 1...");
  uint8_t empID = getFingerprintID(finger1);
  delay(1000);
  if (empID != expectedEmployee) {
    Serial.println("Employee fingerprint mismatch. Unauthorized or wrong shift.");
    digitalWrite(LED_YELLOW, HIGH);
    buzzError();
    delay(2000);
    digitalWrite(LED_YELLOW, LOW);
    return;
  }
  Serial.println("Employee verified.");
  
  // --- Voter Verification ---
  Serial.println("Place VOTER fingerprint (Index) on Sensor 1...");
  uint8_t voterID = getFingerprintID(finger1);
  delay(1000);
  
  if (currentShift == 4) {
    // In Shift 4, allow Voter IDs 202 or 203
    if (!(voterID == 202 || voterID == 203)) {
      Serial.println("Voter fingerprint mismatch. Not allocated for final slot.");
      digitalWrite(LED_RED, HIGH);
      buzzError();
      delay(2000);
      digitalWrite(LED_RED, LOW);
      return;
    }
  } else {
    if (voterID != expectedVoter) {
      Serial.println("Voter fingerprint mismatch or illegal attempt.");
      digitalWrite(LED_RED, HIGH);
      buzzError();
      delay(2000);
      digitalWrite(LED_RED, LOW);
      return;
    }
  }
  
  Serial.println("Voter verified. Entry granted.");
  digitalWrite(LED_GREEN, HIGH);
  delay(2000);
  digitalWrite(LED_GREEN, LOW);
  markVoted(voterID);
}

// ----------------- Vote Verification -----------------
// Verifies the voter again using Fingerprint Sensor 2 (thumb scan) after party selection.
void voteVerification(int selectedParty) {
  Serial.println("Place VOTER fingerprint (Thumb) on Sensor 2 for vote confirmation...");
  uint8_t voterThumbID = getFingerprintID(finger2);
  delay(1000);
  
  int currentShift = getCurrentShift();
  
  // Re-check allowed voter for current shift
  if (currentShift == 4) {
    if (!(voterThumbID == 202 || voterThumbID == 203)) {
      Serial.println("Vote confirmation failed in Shift 4. Fingerprint mismatch.");
      digitalWrite(VOTE_RED, HIGH);
      buzzError();
      delay(2000);
      digitalWrite(VOTE_RED, LOW);
      return;
    }
  } else {
    // For shifts 1-3, check against expected voter (as in entry verification)
    int expectedVoter = (currentShift == 1) ? 201 : 204; // For example
    if (voterThumbID != expectedVoter) {
      Serial.println("Vote confirmation failed. Fingerprint mismatch.");
      digitalWrite(VOTE_RED, HIGH);
      buzzError();
      delay(2000);
      digitalWrite(VOTE_RED, LOW);
      return;
    }
  }
  
  Serial.println("Vote confirmed successfully.");
  digitalWrite(VOTE_GREEN, HIGH);
  delay(2000);
  digitalWrite(VOTE_GREEN, LOW);
  
  // Increment vote count for the selected party.
  switch(selectedParty) {
    case 1: party1Votes++; break;
    case 2: party2Votes++; break;
    case 3: party3Votes++; break;
    case 4: party4Votes++; break;
  }
}
