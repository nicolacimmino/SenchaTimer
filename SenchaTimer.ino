#include <Arduino.h>
#include <FastLED.h>
#include <avr/sleep.h>

#define LED_GND 15
#define LED_D 16
#define LEDS_COUNT 1
#define SW_PIN 7
#define MAX_INFUSIONS 5
#define TIME_LED_IX 0

CRGB led[LEDS_COUNT];

uint16_t infusionTimes[] = {
    90,
    30,
    45,
    90,
    180,
};

uint8_t infusionsCount = 0;
bool running = false;
unsigned long infusionStartTime = 0;
unsigned long pressStartTime = 0;

void setup()
{
  pinMode(LED_GND, OUTPUT);
  digitalWrite(LED_GND, LOW);
  delay(100);

  FastLED.addLeds<WS2812B, LED_D, GRB>(led, LEDS_COUNT);
  FastLED.setBrightness(70);

  for (int ix = 0; ix < LEDS_COUNT; ix++)
  {
    led[ix] = CRGB::Green;
  }

  FastLED.show();

  pinMode(SW_PIN, INPUT_PULLUP);
}

void showCurrentInfusion()
{
  led[TIME_LED_IX] = CRGB::Green;

  uint16_t timeSlot = (millis() % 3000) / 200;
  
  if (timeSlot > (2 * (infusionsCount + 1)) || timeSlot % 2 == 0)
  {    
    led[TIME_LED_IX].fadeToBlackBy(220);   
  }

  FastLED.show();
}

void showTimerRunning()
{
  // Keep breathing! See Sean Voisen great post from which I grabbed the formula.
  // https://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
  float val = (exp(sin(millis() / 2000.0 * PI)) - 0.36787944) * 108.0;
  led[TIME_LED_IX] = CRGB::Green;
  led[TIME_LED_IX].fadeToBlackBy(val);
  FastLED.show();
}

void lightShow(CRGB color1, CRGB color2, uint8_t repetitions, uint16_t delaymS)
{
  for (uint8_t ix = 0; ix < repetitions; ix++)
  {
    led[TIME_LED_IX] = (ix % 2) ? color1 : color2;
    FastLED.show();
    delay(delaymS);
  }
}

void showEnd()
{
  lightShow(CRGB::Orange, CRGB::Yellow, 10, 200);
}

void showReset()
{
  lightShow(CRGB::Green, CRGB::Yellow, 10, 200);
}

void checkBrutton()
{
  if (digitalRead(SW_PIN) == HIGH)
  {
    pressStartTime = 0;
    return;
  }

  if (pressStartTime == 0)
  {
    pressStartTime = millis();
  }

  if (digitalRead(SW_PIN) == LOW && millis() - pressStartTime > 1000)
  {
    running = false;
    showReset();

    return;
  }

  if (!running)
  {
    running = true;
    infusionStartTime = millis();
  }
}

void checkInfusionEnd()
{
  if (millis() - infusionStartTime > (1000.0 * infusionTimes[infusionsCount]))
  {
    running = false;
    showEnd();
    infusionsCount = (infusionsCount + 1) % MAX_INFUSIONS;
  }
}

void loop()
{
  checkBrutton();

  if (!running)
  {
    showCurrentInfusion();
    return;
  }

  showTimerRunning();

  checkInfusionEnd();
}