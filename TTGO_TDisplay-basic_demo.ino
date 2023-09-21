// Basic demo of TTGO "T-Display" ESP32 module with 1.14" color screen features.
// Andrew Bedno - AndrewBedno.com
// Demonstrates screen use, WiFi scan, battery level read, button access, nonvolatile memory, and power control.
// Compile options: Board: ESP32 Dev Module.
// Recreates the module's "Factory Test" app but prettier, simpler, fully commented and minimum dependencies.

uint32_t CPU_MHz = 80;  // Configured clock speed.  240 max, divide to reduce power use.
uint32_t APB_Freq = 80000000;  // Timer clock speed.  Usually 80MHz but read from system in setup to confirm.

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
    // Highest frequency set in var defs.  Overkill and not actually used, but habit.
    setCpuFrequencyMhz(CPU_MHz);  CPU_MHz = getCpuFrequencyMhz();  APB_Freq = getApbFrequency();

    // Setup battery voltage input.
    pinMode(BATTERY_ADC_ENABLE_PIN, OUTPUT);
    digitalWrite(BATTERY_ADC_ENABLE_PIN, HIGH);

    // Setup buttons, connects to ground (logic low) when pressed.
    pinMode(BUTTON_1_PIN, INPUT_PULLUP);
    pinMode(BUTTON_2_PIN, INPUT_PULLUP);

    // Alternating non-volatile flag implements use of reset button for power off/on.
    Memory.begin("cyfi", false);
    Sleep_Flag = Memory.getBool("sleep", false);
    if (Sleep_Flag) {
      Memory.putBool("sleep", false);
      delay(200);  // Tiny delay assures flash write finishes.
      esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
      esp_sleep_enable_ext0_wakeup(BUTTON_1_PIN, 0);  // Enable wakeup on press of right button (or top reset button)
      delay(200);  // Tiny delay for RTOS settling.
      esp_deep_sleep_start();  // Halts.
    } else {
      Memory.putBool("sleep", true);  // Flag to sleep on next boot.
    }
    
    tft.init();  // Initialize screen.
    
    // Calibrate the battery level sense.
    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);    // Check type of calibration value used to characterize ADC
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) { Battery_VRef = adc_chars.vref; }

}

void loop()
{
    tft.fillScreen(TFT_BLACK);  // Clear screen.
    tft.setSwapBytes(true);
    tft.setRotation(1);  // 0=portrait, 1=landscape (2=180,3=270)

    tft.setTextSize(3);  // Big
    tft.setTextColor(TFT_GREEN);
    tft.setTextDatum(MC_DATUM);  // Centered.
    // Sample, calibrate, and display battery voltage (or USB voltage when charging).
    Battery_Read = analogRead(BATTERY_ADC_INPUT_PIN);
    Battery_Voltage = ((float)Battery_Read / 4095.0) * 2.0 * 3.3 * (Battery_VRef / 1000.0);
    sprintf(Battery_Msg, "%1.2fv ", Battery_Voltage);
    tft.drawString(Battery_Msg, tft.width() / 2, 0 );

    // Show MAC address while awaiting scan.
    tft.setTextColor(TFT_YELLOW);
    tft.setTextSize(2); // Smaller
    tft.setTextDatum(TL_DATUM);  // TopLeft justified.
    tft.drawString(WiFi.macAddress(), 1, 24);

    // Show nearest WiFi networks (max 7).
    WiFi_Networks = WiFi.scanNetworks();
    if (WiFi_Networks > 0) {
      tft.fillRect(0, 24, tft.width(), 24, TFT_BLACK);  // Clear mac address display
      tft.setCursor(0, 23);  // Location (for start of println) in pixels X,Y (0,0=L,T).
      tft.setTextColor(TFT_CYAN);
      for (WiFi_Network = 0; (WiFi_Network < WiFi_Networks) & (WiFi_Network < 8); WiFi_Network++) {
        sprintf(WiFi_Msg, "%s(%d)", WiFi.SSID(WiFi_Network).c_str(), WiFi.RSSI(WiFi_Network));
        WiFi_Msg[20] = 0;  // truncate
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
