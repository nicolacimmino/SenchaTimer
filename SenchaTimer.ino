#include <Arduino.h>
#include <avr/sleep.h>
#include <FastLED.h>

#define LED_GND 15
#define LED_D 16
#define LEDS_COUNT 1
#define SW_GND 9
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

void setup()
{
  pinMode(LED_GND, OUTPUT);
  digitalWrite(LED_GND, LOW);
  delay(100);

  FastLED.addLeds<WS2812B, LED_D, GRB>(led, LEDS_COUNT);
  FastLED.setBrightness(90);

  for (int ix = 0; ix < LEDS_COUNT; ix++)
  {
    led[ix] = CRGB::Green;
  }

  FastLED.show();

  pinMode(SW_GND, OUTPUT);
  digitalWrite(SW_GND, LOW);

  pinMode(SW_PIN, INPUT_PULLUP);
}

void showCurrentInfusion()
{
  uint16_t timeOffset = (millis() % 3000) / 200;

  if (timeOffset > 10)
  {
    led[TIME_LED_IX] = CRGB::Black;
    FastLED.show();
    return;
  }

  if (timeOffset > (2 * (infusionsCount + 1)))
  {
    led[TIME_LED_IX] = CRGB::Black;
    FastLED.show();
    return;
  }

  if (timeOffset % 2 == 0)
  {
    led[TIME_LED_IX] = CRGB::Black;
    FastLED.show();
    return;
  }

  led[TIME_LED_IX] = CRGB::Green;
  FastLED.show();
}

void breathe()
{
  // Keep breathing! See Sean Voisen great post from which I grabbed the formula.
  // https://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
  float val = (exp(sin(millis() / 2000.0 * PI)) - 0.36787944) * 108.0;
  led[TIME_LED_IX] = CRGB::Green;
  led[TIME_LED_IX].fadeToBlackBy(val);
  FastLED.show();
}

void showEnd()
{
  for (uint8_t ix = 0; ix < 10; ix++)
  {
    led[TIME_LED_IX] = (ix % 2) ? CRGB::Orange : CRGB::Black;
    FastLED.show();
    delay(200);
  }
}

void showReset()
{
  for (uint8_t ix = 0; ix < 10; ix++)
  {
    led[TIME_LED_IX] = (ix % 2) ? CRGB::Purple : CRGB::Blue;
    FastLED.show();
    delay(200);
  }
}

unsigned long pressStartTime = 0;

void loop()
{
  if (digitalRead(SW_PIN) == LOW)
  {
    if (!running)
    {
      running = true;
      infusionStartTime = millis();
      return;
    }

    if (pressStartTime == 0)
    {
      pressStartTime = millis();
    }

    if (digitalRead(SW_PIN) == LOW && millis() - pressStartTime > 1000)
    {
      running = false;
      infusionsCount = 0;
      showReset();
      return;
    }
  }

  if (digitalRead(SW_PIN) == HIGH)
  {
    pressStartTime = 0;
  }

  if (!running)
  {
    showCurrentInfusion();
  }
  else
  {
    breathe();
    if (millis() - infusionStartTime > (1000.0 * infusionTimes[infusionsCount]))
    {
      running = false;
      showEnd();
      infusionsCount = (infusionsCount + 1) % MAX_INFUSIONS;
    }
  }
}