/////////////////////////////////////////////////////////////////////////////////////////////
//  Sencha Timer.
//  Copyright (C) 2020 Nicola Cimmino
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see http://www.gnu.org/licenses/.
//
//
/////////////////////////////////////////////////////////////////////////////////////////////

#include <EEPROM.h>
#include <Arduino.h>
#include <FastLED.h>
#include <Sleep_n0m1.h>

/////////////////////////////////////////////////////////////////////////////////////////////
//  Hardware.
//

#define LED_GND_PIN 15
#define LED_DATA_PIN 16
#define LEDS_COUNT 1
#define TIME_LED_IX 0
#define BUTTON_PIN 7
#define EEPROM_INFUSIONS_COUNT 0x100
#define EEPROM_TEA_TYPE 0x101

//
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////
//  Consts.
//

#define MAX_INFUSIONS 5
#define MAX_TEA_TYPES 6
#define MODE_INFUSION 0
#define MODE_TEA_SELECTION 1
#define MODE_TEA_PROGRAM_TEMPERATURE 2
#define MODE_TEA_PROGRAM_DURATION 3

//
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////
//  Globals.
//

CRGB led[LEDS_COUNT];
uint8_t mode = MODE_INFUSION;
uint8_t teaType = 0;
uint8_t infusionsCount = 0;
unsigned long infusionStartTime = 0;
unsigned long lastInfusionEndTime = 0;
Sleep sleep;

//
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////
//  Infusion control.
//
//  EEPROM Map.
//
//  0         Tea 1 infusion 1 time (seconds/5)
//  1         Tea 1 infustion 1 temperature (C)
//  2         Tea 1 infusion 2 time (seconds/5)
//  3         Tea 1 infustion 2 temperature (C)
//  ...
//  10        Tea 2 infusion 1 time (seconds/5)
//  ...

#define EEPROM_INFUSION_CFG_BASE 0
#define EEPROM_INFUSION_CFG_ENTRY_SIZE 2
#define EEPROM_INFUSION_CFG_TEA_ENTRY_SIZE (EEPROM_INFUSION_CFG_ENTRY_SIZE * MAX_INFUSIONS)

void setupDefaultTeas()
{
  // Sencha
  EEPROM.write(0, 90 / 5);
  EEPROM.write(1, 70);
  EEPROM.write(2, 30 / 5);
  EEPROM.write(3, 75);
  EEPROM.write(4, 45 / 5);
  EEPROM.write(5, 80);
  EEPROM.write(6, 90 / 5);
  EEPROM.write(7, 85);
  EEPROM.write(8, 180 / 5);
  EEPROM.write(9, 85);

  // Gyokuro
  EEPROM.write(10, 420 / 5);
  EEPROM.write(11, 0);
  EEPROM.write(12, 15 / 5);
  EEPROM.write(13, 50);
  EEPROM.write(14, 39 / 5);
  EEPROM.write(15, 60);
  EEPROM.write(16, 60 / 5);
  EEPROM.write(17, 70);
  EEPROM.write(18, 90 / 5);
  EEPROM.write(19, 70);

  // Test only
  EEPROM.write(20, 2);
  EEPROM.write(21, 80);
  EEPROM.write(22, 0);
  EEPROM.write(23, 0);

  EEPROM.write(30, 2);
  EEPROM.write(31, 80);
  EEPROM.write(32, 0);
  EEPROM.write(33, 0);

  EEPROM.write(40, 2);
  EEPROM.write(41, 80);
  EEPROM.write(42, 0);
  EEPROM.write(43, 0);

  EEPROM.write(50, 2);
  EEPROM.write(51, 80);
  EEPROM.write(52, 0);
  EEPROM.write(53, 0);

  // End marker
  EEPROM.write(60, 0);
}

CRGB teaTypeColors[MAX_TEA_TYPES] = {
    CRGB::Red,
    CRGB::Blue,
    CRGB::Yellow,
    CRGB::Green,
    CRGB::White,
    CRGB::Purple,
};

uint16_t getInfusionTime()
{
  return 5 * EEPROM.read(EEPROM_INFUSION_CFG_BASE + (teaType * EEPROM_INFUSION_CFG_TEA_ENTRY_SIZE) + (EEPROM_INFUSION_CFG_ENTRY_SIZE * infusionsCount));
}

void writeInfusionTime()
{
  uint8_t infusionTime = ((millis() - infusionStartTime) / 1000.0) / 5;
  EEPROM.write(EEPROM_INFUSION_CFG_BASE + (teaType * EEPROM_INFUSION_CFG_TEA_ENTRY_SIZE) + (EEPROM_INFUSION_CFG_ENTRY_SIZE * infusionsCount), infusionTime);
}

uint8_t getInfusionTemperature()
{
  return EEPROM.read(EEPROM_INFUSION_CFG_BASE + 1 + (teaType * EEPROM_INFUSION_CFG_TEA_ENTRY_SIZE) + (EEPROM_INFUSION_CFG_ENTRY_SIZE * infusionsCount));
}

void increaseInfusionTemperature()
{
  uint8_t temperature = getInfusionTemperature() + 5;

  if (temperature >= 95)
  {
    temperature = 40;
  }

  EEPROM.write(EEPROM_INFUSION_CFG_BASE + 1 + (teaType * EEPROM_INFUSION_CFG_TEA_ENTRY_SIZE) + (EEPROM_INFUSION_CFG_ENTRY_SIZE * infusionsCount), temperature);
}

//
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////
// Setup the LED and initialise the input pin.

void setup()
{
  pinMode(LED_GND_PIN, OUTPUT);
  digitalWrite(LED_GND_PIN, LOW);
  delay(300);

  FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(led, LEDS_COUNT);
  FastLED.setBrightness(70);
  led[TIME_LED_IX] = CRGB::Black;
  FastLED.show();

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  infusionsCount = min(EEPROM.read(EEPROM_INFUSIONS_COUNT), MAX_INFUSIONS - 1);
  teaType = min(EEPROM.read(EEPROM_TEA_TYPE), MAX_TEA_TYPES - 1);

  showCurrentTeaType();

  // Powerup with button pressed to enter tea selection mode.
  if (digitalRead(BUTTON_PIN) == LOW)
  {
    mode = MODE_TEA_SELECTION;
    while (digitalRead(BUTTON_PIN) == LOW)
    {
      delay(100);
    }
  }
  else
  {
    delay(2000);
  }

  // Uncomment to program EEPROM first time.
  //setupDefaultTeas();
}

//
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////
// Show the current infustion as a sequence of flahses.
// The LED is kept dimly lit between flashes to aknoweledge the fact the unit is powered up.
// This is a non-blocking call, the notification runs on a 3000 mS schedule divided into
// 200 mS slots which are deterined by the current time:
// 0    First Flash
// 1    Dim
// 2    Second Flash | Dim
// 3    Dim
// 4    Third Flash | Dim
// 5    Dim
// 6    Forth Flash | Dim
// 7    Dim
// 8    Fifth Flash | Dim
// > 9  Dim
//

void showCurrentInfusion()
{
  // Temperature range 70 - 90C => 20
  // Color range: Green (96) - Red (255) => 159
  // Hue = 159 * ((temp - 70) / 20) + 96
  if ((getInfusionTemperature() != 0))
  {
    led[TIME_LED_IX] = CHSV(96 + 159 * ((getInfusionTemperature() - 70) / 20.0), 255, 255);
  }
  else
  {
    // Zero indicates room temperature infusion, use black so we don't need to stretch too much the range above.
    led[TIME_LED_IX] = CRGB::White;
  }

  uint16_t timeSlot = (millis() % 3000) / 200;

  if (timeSlot > (2 * (infusionsCount + 1)) || timeSlot % 2 == 0)
  {
    led[TIME_LED_IX].fadeToBlackBy(240);
  }

  FastLED.show();
}

//
/////////////////////////////////////////////////////////////////////////////////////////////

void showCurrentInfusionTimeProgramming()
{
  led[TIME_LED_IX] = CRGB::Red;

  uint16_t timeSlot = (millis() % 3000) / 200;

  if (timeSlot > (2 * (infusionsCount + 1)) || timeSlot % 2 == 0)
  {
    led[TIME_LED_IX].fadeToBlackBy(240);
  }

  FastLED.show();
}

void showTimerRunningProgramming()
{
  led[TIME_LED_IX] = CRGB::Red;

  // Keep breathing! See Sean Voisen great post from which I grabbed the formula.
  // https://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
  float val = (exp(sin(millis() / 2000.0 * PI)) - 0.36787944) * 108.0;

  led[TIME_LED_IX].fadeToBlackBy(val);
  FastLED.show();
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Show that the timer is running.
// Breaths the LED with a color that shifts from yellow to green as the infustion progresses.
// This is a non-blocking call, the colour and brightness of the LED depend on the current
// time and infusion time.
//

void showTimerRunning()
{

  float infusionProgress = (millis() - infusionStartTime) / (1000.0 * getInfusionTime());

  led[TIME_LED_IX] = CHSV(64 + (infusionProgress * 32), 255, 255);

  // Keep breathing! See Sean Voisen great post from which I grabbed the formula.
  // https://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
  float val = (exp(sin(millis() / 2000.0 * PI)) - 0.36787944) * 108.0;

  led[TIME_LED_IX].fadeToBlackBy(val);
  FastLED.show();
}

//
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////
// Blink the LED.
// Used to attract attention blinks the LED between two colors the specificed amount of times.
// This is a blocking call that will return only once the show completes.

void lightShow(CRGB color1, CRGB color2, uint8_t repetitions, uint16_t delaymS)
{
  for (uint8_t ix = 0; ix < repetitions; ix++)
  {
    led[TIME_LED_IX] = (ix % 2) ? color1 : color2;
    FastLED.show();
    delay(delaymS);
  }
}

//
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////
// Reset the current infusion.
// This stops the current infusion but doesn't reset the count. The operation is aknoweledged
// by a green/yellow flashing pattern.

void resetCurrentInfusion()
{
  infusionStartTime = 0;

  lightShow(CRGB::Green, CRGB::Yellow, 10, 200);
}

//
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////
// End the current infusion.
// This ends the infusion and increments the current infusion to the next. The operation is
// aknoweledged by a orange/yellow flashing pattern.

void endInfusion()
{
  infusionStartTime = 0;

  lightShow(CRGB::Orange, CRGB::Yellow, 10, 200);

  infusionsCount = (infusionsCount + 1) % MAX_INFUSIONS;

  // End marker, following infusions are not set
  if (getInfusionTime() == 0)
  {
    infusionsCount = 0;
  }

  EEPROM.write(EEPROM_INFUSIONS_COUNT, infusionsCount);

  if (infusionsCount == 0)
  {
    enterLowPowerIdling();
  }
}

//
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////
// Asks a yes/no question.
// It will first show red/green in quick succession until the button is released.
// Then alternatively green and red every 2s. Returns true or false according to the
//  current color displayed when the button is pressed.

bool askYesNoQuestion()
{
  unsigned long questionStartTime;
  bool answer = false;

  while (digitalRead(BUTTON_PIN) == LOW)
  {
    lightShow(CRGB::Green, CRGB::Red, 2, 100);
  }

  while (true)
  {
    questionStartTime = millis();

    led[TIME_LED_IX] = answer ? CRGB::Green : CRGB::Red;
    FastLED.show();

    while (millis() < questionStartTime + 2000)
    {
      if (digitalRead(BUTTON_PIN) == LOW)
      {
        return answer;
      }
    }

    answer = !answer;
  }
}

//
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////
// Resets the timer to the beginnig of the first infusion.

void resetInfusions()
{
  if (askYesNoQuestion())
  {
    infusionsCount = 0;

    EEPROM.write(EEPROM_INFUSIONS_COUNT, infusionsCount);

    lightShow(CRGB::Green, CRGB::Black, 10, 200);
  }
}

//
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////
// Enter a low power mode and just flash red every 8s.
// During sleep the LED power is removed and the controller is in power save mode.
// To start a new infusion user will need to power-cycle the device.
//

void enterLowPowerIdling()
{
  while (true)
  {
    led[TIME_LED_IX] = CRGB::Red;
    FastLED.show();
    delay(100);
    led[TIME_LED_IX] = CRGB::Black;
    FastLED.show();

    pinMode(LED_GND_PIN, INPUT);

    sleep.pwrDownMode();
    sleep.sleepDelay(8000);

    pinMode(LED_GND_PIN, OUTPUT);
    digitalWrite(LED_GND_PIN, LOW);
    delay(300);
  }
}

//
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////
// Test the button and take action.
// Regular Mode:
//  When no infusion running:
//    Short press: will start the infusion.
//    Long press: will reset the timer to the start of the first infusion.
//  When infusion running:
//    Long press: will stop and reset the infustion and maintain the current infusions count.

void click()
{
  switch (mode)
  {
  case MODE_INFUSION:
    if (infusionStartTime == 0)
    {
      infusionStartTime = millis();
    }

    break;
  case MODE_TEA_SELECTION:
    teaType = (teaType + 1) % MAX_TEA_TYPES;
    
    EEPROM.write(EEPROM_TEA_TYPE, teaType);

    infusionsCount = 0;

    EEPROM.write(EEPROM_INFUSIONS_COUNT, infusionsCount);

    break;
  case MODE_TEA_PROGRAM_TEMPERATURE:
    increaseInfusionTemperature();

    break;
  case MODE_TEA_PROGRAM_DURATION:
    if (infusionStartTime == 0)
    {
      infusionStartTime = millis();
    }
    else
    {
      writeInfusionTime();
      infusionStartTime = 0;
      infusionsCount += 1;
      mode = MODE_TEA_PROGRAM_TEMPERATURE;

      break;
    }
  }
}

void longPress()
{
  switch (mode)
  {
  case MODE_INFUSION:
    if (infusionStartTime != 0)
    {
      resetCurrentInfusion();
      return;
    }

    resetInfusions();
    break;
  case MODE_TEA_SELECTION:
    mode = MODE_TEA_PROGRAM_TEMPERATURE;
    infusionsCount = 0;
    break;
  case MODE_TEA_PROGRAM_TEMPERATURE:
    mode = MODE_TEA_PROGRAM_DURATION;
    break;
  case MODE_TEA_PROGRAM_DURATION:
    infusionStartTime = 0;
    break;
  }
}
void checkButton()
{
  unsigned long pressStartTime = 0;

  if (digitalRead(BUTTON_PIN) == HIGH)
  {
    return;
  }

  pressStartTime = millis();

  while (digitalRead(BUTTON_PIN) == LOW)
  {
    if (millis() - pressStartTime > 1000)
    {
      longPress();

      while (digitalRead(BUTTON_PIN) == LOW)
      {
      }

      return;
    }
  }

  click();
}

//
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////
// Check if the infusion has come to the end time.

void checkInfusionEnd()
{
  if (millis() - infusionStartTime > (1000.0 * getInfusionTime()))
  {
    endInfusion();
  }
}

//
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////
// Show the currently selected tea type

void showCurrentTeaType()
{
  led[TIME_LED_IX] = teaTypeColors[teaType];
  FastLED.show();
}

//
/////////////////////////////////////////////////////////////////////////////////////////////

void showCurrentTeaProgramTemperature()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Main loop.
// Keep checking the input button and run the timer if needed.

void infusionLoop()
{
  checkButton();

  if (infusionStartTime == 0)
  {
    showCurrentInfusion();

    return;
  }

  showTimerRunning();

  checkInfusionEnd();
}

void teaSelectionLoop()
{
  checkButton();

  showCurrentTeaType();

  delay(50);
}

void teaProgramTemperatureLoop()
{
  checkButton();

  showCurrentInfusion();

  delay(50);
}

void teaProgramDurationLoop()
{
  checkButton();

  if (infusionStartTime == 0)
  {
    showCurrentInfusionTimeProgramming();

    return;
  }

  showTimerRunningProgramming();
}

void loop()
{
  switch (mode)
  {
  case MODE_INFUSION:
    infusionLoop();
    break;
  case MODE_TEA_SELECTION:
    teaSelectionLoop();
    break;
  case MODE_TEA_PROGRAM_TEMPERATURE:
    teaProgramTemperatureLoop();
    break;
  case MODE_TEA_PROGRAM_DURATION:
    teaProgramDurationLoop();
    break;
  }
}

//
/////////////////////////////////////////////////////////////////////////////////////////////
