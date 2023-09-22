// Basic demo of TTGO "T-Display" ESP32 module with 1.14" color screen features.
// Andrew Bedno - AndrewBedno.com
// Demonstrates screen use, WiFi scan, battery level read, button access, nonvolatile memory, and power control.
// Recreates and improves on the module's "Factory Test" app. But cleaner code, prettier, simpler, fully commented and minimum dependencies.
// Press top RESET button to power on/off. Shows battery voltage and Mac address, then a scan of nearest WiFi networks. Press either button to refresh.
// Compile options: Board: ESP32 Dev Module.  Speed: 80MHz

// Display library.
// REQUIRES custom install (in IDE library manager) from TTGO git zip (rather than Arduino stock) ...
//   to set SPI pins specific to board: TFT_MOSI=19, TFT_SCLK=18, TFT_CS=5, TFT_DC=16, TFT_RST=23
// https://github.com/Xinyuan-LilyGO/TTGO-T-Display
#include "TFT_eSPI.h"
TFT_eSPI tft = TFT_eSPI(135, 240); // Start custom screen library, resolution 135x240

#include <SPI.h>  // SPI communications library.
#include <WiFi.h>  // WiFi library.
#include <esp_adc_cal.h>  // ADC calibration for (battery level testing).

// Configure hardware options specific to this board model.
// Routes battery level input to ADC.
#define BATTERY_ADC_ENABLE_PIN GPIO_NUM_14
// Battery level analog input pin.  ADC1:32-39 ADC2 disabled when WiFi used.
#define BATTERY_ADC_INPUT_PIN GPIO_NUM_34

// Right/Upper button.
#define BUTTON_1_PIN GPIO_NUM_35
// Left/Lower button.
#define BUTTON_2_PIN GPIO_NUM_0

// Globals for non-volatile memory.
#include <Preferences.h>
Preferences Memory;
bool Sleep_Flag = false;

// Globals for battery level read,
char Battery_Msg[20];
int Battery_VRef = 1100;
uint16_t Battery_Read;
float Battery_Voltage;

// Globals for WiFi scan.
int16_t WiFi_Networks;
int WiFi_Network;
char WiFi_Msg[800];

void setup()
{

    // Setup battery voltage input.
    pinMode(BATTERY_ADC_ENABLE_PIN, OUTPUT);
    digitalWrite(BATTERY_ADC_ENABLE_PIN, HIGH);

    // Setup buttons, connects to ground (logic low) when pressed.
    pinMode(BUTTON_1_PIN, INPUT_PULLUP);
    pinMode(BUTTON_2_PIN, INPUT_PULLUP);

    // Alternating non-volatile flag implements use of reset button for power off/on.
    Memory.begin("ttgodemo", false);
    Sleep_Flag = Memory.getBool("sleep", false);
    if (Sleep_Flag) {
      Memory.putBool("sleep", false);
      delay(200);  // Tiny delay assures flash write finishes.
      // Turn off screen.
	    tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.writecommand(TFT_DISPOFF);
      tft.writecommand(TFT_SLPIN);
      digitalWrite(TFT_BL, 0);
      delay(200);  // Tiny delay to let screen finish.
      // Turn off rtos wakes.
	    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
      // esp_sleep_enable_ext0_wakeup(BUTTON_1_PIN, 0);  // Enable wakeup on press of right button
      delay(200);  // Tiny delay for RTOS settling.
      esp_deep_sleep_start();  // Halts.
    } else {
      Memory.putBool("sleep", true);  // Flag to sleep on next boot.
    }
    
    tft.init();  // Initialize screen.
    // Set backlight brightness (uses PWM of LED) to reduce power use.
    pinMode(TFT_BL, OUTPUT);
    ledcSetup(0, 5000, 8); // 0-15, 5000, 8
    ledcAttachPin(TFT_BL, 0); // TFT_BL, 0-15
    ledcWrite(0, 69);  // 0-15, 0-255 : 0=none/dark, 69=normal, 255=brightest/very high battery drain
    
    // Calibrate the battery level sense.
    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);    // Check type of calibration value used to characterize ADC
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) { Battery_VRef = adc_chars.vref; }

}

void loop()
{
    tft.fillScreen(TFT_BLACK);  // Clear screen.
    tft.setRotation(1);  // 0=portrait, 1=landscape (2=180,3=270)

    tft.setTextSize(3);  // Big
    tft.setTextColor(TFT_GREEN);
    tft.setTextDatum(MC_DATUM);  // Centered.
    // Sample, calibrate, and display battery voltage (or USB voltage when charging).
    Battery_Read = analogRead(BATTERY_ADC_INPUT_PIN);
    Battery_Voltage = ((float)Battery_Read / 4095.0) * 2.0 * 3.3 * (Battery_VRef / 1000.0);
    sprintf(Battery_Msg, "%1.2fv ", Battery_Voltage);
    tft.drawString(Battery_Msg, 120, 0 );

    // Show MAC address while awaiting scan.
    tft.setTextColor(TFT_YELLOW);
    tft.setTextSize(2); // Smaller
    tft.setTextDatum(TL_DATUM);  // TopLeft justified.
    tft.drawString(WiFi.macAddress(), 4, 30);

    // Show nearest WiFi networks.
    WiFi_Networks = WiFi.scanNetworks();
    if (WiFi_Networks > 0) {
	    tft.fillScreen(TFT_BLACK);  // Clear
      tft.setCursor(0, 0);  // Location (for start of println) in pixels X,Y (0,0=L,T).
      for (WiFi_Network = 0; (WiFi_Network < WiFi_Networks) & (WiFi_Network < 10); WiFi_Network++) {
        sprintf(WiFi_Msg, "%02d", abs( WiFi.RSSI(WiFi_Network)));  // inverse Signal strength.
        tft.setTextColor(TFT_LIGHTGREY);
        tft.setTextSize(1); // Tiny
        tft.print(WiFi_Msg);        
        sprintf(WiFi_Msg, "%s", WiFi.SSID(WiFi_Network).c_str());  // LAN name
        WiFi_Msg[18] = 0;  // truncate
        tft.setTextColor(TFT_CYAN);
        tft.setTextSize(2); // Small
        tft.println(WiFi_Msg);
      }
    }

    while (true) {
      delay(20);
      // Either button simply reboots to show updated display.  No debouncing done or needed.
      if ( (digitalRead(BUTTON_1_PIN) < 1) | (digitalRead(BUTTON_2_PIN) < 1) ) {
        Memory.putBool("sleep", false);  // Overrides power toggle after restart.
        tft.fillScreen(TFT_BLACK);  // Clear screen before reboot (or prior contents show after reboot).
        delay(20);
        ESP.restart();
      }
    }

}
