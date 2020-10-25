/////////////////////////////////////////////////////////////////////////////////////////////
//  Sencha Timer.
//  Copyright (C) 2014 Nicola Cimmino
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

#include <Arduino.h>
#include <FastLED.h>
#include <avr/sleep.h>

/////////////////////////////////////////////////////////////////////////////////////////////
//  Hardware.
//

#define LED_GND_PIN 15
#define LED_DATA_PIN 16
#define LEDS_COUNT 1
#define TIME_LED_IX 0
#define BUTTON_PIN 7

//
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////
//  Infusion control.
//

#define MAX_INFUSIONS 5

uint16_t infusionTimes[MAX_INFUSIONS] = {
    90,
    30,
    45,
    90,
    180,
};

//
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////
//  Globals.
//

CRGB led[LEDS_COUNT];       
uint8_t infusionsCount = 0; 
unsigned long infusionStartTime = 0;
unsigned long pressStartTime = 0;

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

  for (int ix = 0; ix < LEDS_COUNT; ix++)
  {
    led[ix] = CRGB::Black;
  }

  FastLED.show();

  pinMode(BUTTON_PIN, INPUT_PULLUP);
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
  led[TIME_LED_IX] = CRGB::Green;

  uint16_t timeSlot = (millis() % 3000) / 200;

  if (timeSlot > (2 * (infusionsCount + 1)) || timeSlot % 2 == 0)
  {
    led[TIME_LED_IX].fadeToBlackBy(240);
  }

  FastLED.show();
}

//
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////
// Show that the timer is running.
// Breaths the LED with a color that shifts from yellow to green as the infustion progresses.
// This is a non-blocking call, the colour and brightness of the LED depend on the current
// time and infusion time.
//

void showTimerRunning()
{

  float infusionProgress = (millis() - infusionStartTime) / (1000.0 * infusionTimes[infusionsCount]);

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
  infusionsCount = (infusionsCount + 1) % MAX_INFUSIONS;

  lightShow(CRGB::Orange, CRGB::Yellow, 10, 200);
}

//
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////
// Test the button and take action.
// A short press while the infusion is not running will start it.
// A long press will reset stop the infustion and maintain the current infusions count.

void checkBrutton()
{
  if (digitalRead(BUTTON_PIN) == HIGH)
  {
    pressStartTime = 0;

    return;
  }

  if (pressStartTime == 0)
  {
    pressStartTime = millis();
  }

  if (digitalRead(BUTTON_PIN) == LOW && millis() - pressStartTime > 1000)
  {
    resetCurrentInfusion();

    return;
  }

  if (infusionStartTime == 0)
  {
    infusionStartTime = millis();
  }
}

//
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////
// Check if the infusion has come to the end time.

void checkInfusionEnd()
{
  if (millis() - infusionStartTime > (1000.0 * infusionTimes[infusionsCount]))
  {
    endInfusion();
  }
}

//
/////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////
// Main loop.
// Keep checking the input button and run the timer if needed.

void loop()
{
  checkBrutton();

  if (infusionStartTime != 0)
  {
    showCurrentInfusion();
    
    return;
  }

  showTimerRunning();

  checkInfusionEnd();
}

//
/////////////////////////////////////////////////////////////////////////////////////////////
