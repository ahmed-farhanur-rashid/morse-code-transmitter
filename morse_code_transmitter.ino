/*
 *
 *  Please Refer to README to get a general Overview.
 *
*/


#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <RF24.h>
#include <nRF24L01.h>

// Pin Definitions
const int dotButtonPin = 2;
const int dashButtonPin = 3;
const int actionButtonPin = 4;
const int transmitButtonPin = 5;

// Extra Setup
const int DEBOUNCE_DELAY = 100;
const int LONG_PRESS_DURATION = 500;

// LCD With I2C Module Setup
const byte LCD_ADDRESS = 0x27;  // Address of I2C backpack
const int LCD_COLS = 16;
const int LCD_ROWS = 2;
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);

// NRF24L01+ PA/LNA Setup
const int CE_PIN = 9;
const int CSN_PIN = 10;
const int PAYLOAD_SIZE = sizeof(char) * LCD_COLS;  // Size of sent or received data
const byte address[6] = "00001";                   // Writing & Receiving pipe address.
RF24 radio(CE_PIN, CSN_PIN);                       // CE, CSN

// Input and Transmission Variables/Objects
String morseInput = "";
String translatedText = "";

// Morse Code Dictonary
String morseToEnglish(String morse) {
  if (morse == ".-") return "A";
  if (morse == "-...") return "B";
  if (morse == "-.-.") return "C";
  if (morse == "-..") return "D";
  if (morse == ".") return "E";
  if (morse == "..-.") return "F";
  if (morse == "--.") return "G";
  if (morse == "....") return "H";
  if (morse == "..") return "I";
  if (morse == ".---") return "J";
  if (morse == "-.-") return "K";
  if (morse == ".-..") return "L";
  if (morse == "--") return "M";
  if (morse == "-.") return "N";
  if (morse == "---") return "O";
  if (morse == ".--.") return "P";
  if (morse == "--.-") return "Q";
  if (morse == ".-.") return "R";
  if (morse == "...") return "S";
  if (morse == "-") return "T";
  if (morse == "..-") return "U";
  if (morse == "...-") return "V";
  if (morse == ".--") return "W";
  if (morse == "-..-") return "X";
  if (morse == "-.--") return "Y";
  if (morse == "--..") return "Z";
  return "?";
}

void setup() {

  pinMode(dotButtonPin, INPUT_PULLUP);
  pinMode(dashButtonPin, INPUT_PULLUP);
  pinMode(actionButtonPin, INPUT_PULLUP);
  pinMode(transmitButtonPin, INPUT_PULLUP);

  // Setup lcd, not LCD
  lcd.begin(LCD_COLS, LCD_ROWS);  // 16 columns and 2 rows
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Input: ");
  lcd.setCursor(0, 1);
  lcd.print("Ready");

  Serial.begin(9600);  // Set signal rate

  // Setup radio, not NRF24L01
  radio.begin();
  radio.setAutoAck(false);
  radio.setDataRate(RF24_250KBPS);  // Set datarate
  radio.openWritingPipe(address);
  radio.openReadingPipe(0, address);   // Use pipe 1 for receiving
  radio.setPALevel(RF24_PA_LOW);       // Set power level
  radio.setPayloadSize(PAYLOAD_SIZE);  // Set packet size, max = 32Bytes
  radio.startListening();
}

void loop() {
  // LOW means buttons pressed, as pressing button cuts of continuous electricity to input pins.
  // Reset Functionality
  if (digitalRead(dotButtonPin) == LOW && digitalRead(dashButtonPin) == LOW) {
    delay(10 * DEBOUNCE_DELAY);
    morseInput = "";
    translatedText = "";
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("Input: ");
    lcd.setCursor(0, 1);
    lcd.print("Ready");
  }

  // Dots
  if (digitalRead(dotButtonPin) == LOW) {
    delay(3 * DEBOUNCE_DELAY);
    morseInput += ".";
    lcd.setCursor(7, 0);
    lcd.print(morseInput);
  }

  // Dash
  if (digitalRead(dashButtonPin) == LOW) {
    delay(3 * DEBOUNCE_DELAY);
    morseInput += "-";
    lcd.setCursor(7, 0);
    lcd.print(morseInput);
  }

  // Word-Word Translate & Space Addition
  if (digitalRead(actionButtonPin) == LOW) {

    unsigned long pressStartTime = millis();  // Record the time when the button was pressed

    // Wait until the button is released or determined to be a long press
    while (digitalRead(actionButtonPin) == LOW) {
      if (millis() - pressStartTime >= LONG_PRESS_DURATION) {
        // Long press detected
        translatedText = removeLastChar(translatedText);  // Remove the last character
        clearRow(1);                                      // Clear the row on the LCD
        lcd.print(translatedText);                        // Display the updated input
        delay(5 * DEBOUNCE_DELAY);                        // Debounce to prevent accidental repeats
        return;                                           // Exit to avoid processing as a short press
      }
    }

    delay(5 * DEBOUNCE_DELAY);
    lcd.clear();
    if (morseInput == "") {
      translatedText += " ";
    } else {
      translatedText += morseToEnglish(morseInput);
    }
    morseInput = "";
    lcd.setCursor(0, 0);
    lcd.print("Input: ");
    lcd.setCursor(0, 1);
    lcd.print(translatedText);
  }

  // Transmission
  if (digitalRead(transmitButtonPin) == LOW) {

    delay(5 * DEBOUNCE_DELAY);  // Debounce delay to prevent accidental presses

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Transmitting...");
    lcd.setCursor(0, 1);
    lcd.print(translatedText);

    delay(20 * DEBOUNCE_DELAY);  // Debounce delay to look cool

    radio.stopListening();  // Stop listening to send data

    translatedText = translatedText.substring(0, PAYLOAD_SIZE);  // Ensure proper size

    if (radio.write(translatedText.c_str(), PAYLOAD_SIZE)) {
      Serial.println("Message transmitted successfully.");
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("Sent!");
    } else {
      Serial.println("Transmission failed.");
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("Failed to send!");
    }

    lcd.setCursor(0, 0);
    lcd.print("Input: ");
    delay(100);              // Wait for stability before restarting
    radio.startListening();  // Resume listening
    morseInput = "";
    translatedText = "";
  }

  if (radio.available()) {

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Receiving Data...");

    char receivedText[LCD_COLS + 1] = "";  // 16 Bytes Array + null terminator
    radio.read(&receivedText, PAYLOAD_SIZE);

    lcd.setCursor(0, 1);
    lcd.print(receivedText);
  }
}

// Helper Functions

void clearRow(int row) {
  lcd.setCursor(0, row);
  for (int i = 0; i < LCD_COLS; i++) lcd.print(" "); // lcd.print("                ");
  lcd.setCursor(0, row);
}

String removeLastChar(String string) {
  if (string.length() > 0) {
    return string.substring(0, string.length() - 1);
  } else {
    return string;  // Return the empty string if the input is already empty
  }
}
