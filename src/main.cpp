#include <Arduino.h>

void setup() 
{
  Serial.begin(115200);
  while (!Serial) 
    delay(10); // Wait for Serial on boards that need it

  Serial.println(F("Hello, world!"));
}

void loop() 
{
  delay(1000);
}
