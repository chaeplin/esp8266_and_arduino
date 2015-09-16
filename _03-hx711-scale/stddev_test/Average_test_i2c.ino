#include <Wire.h>
#include <Average.h>

// Reserve space for 10 entries in the average bucket.
// Change the type between < and > to change the entire way the library works.
Average<float> ave(10);
int x;
int r = LOW;
int o_r = LOW;

void setup() {
  Serial.begin(38400);
  Wire.begin(7);
  Wire.onReceive(receiveEvent);
}


void loop() {

  if ( r != o_r) {

    // Add a new random value to the bucket
    ave.push(x);

    Serial.print(x);
    Serial.print(" -> ");
    // Display the current data set
    for (int i = 0; i < 10; i++) {
      Serial.print(ave.get(i), 0);
      Serial.print(" ");
    }




    // And show some interesting results.
    Serial.print("\t\tMean:   "); Serial.print(ave.mean(), 0);
    Serial.print("  Max:    "); Serial.print(ave.maximum(), 0);
    Serial.print("  Min:    "); Serial.print(ave.minimum(), 0);
    Serial.print("  Max - Min:     "); Serial.print((ave.maximum() - ave.minimum()), 0);
    Serial.print("  StdDev: "); Serial.println(ave.stddev(), 0);


    
    o_r = r ;
  }

}

void receiveEvent(int howMany)
{
  byte a, b;

  a = Wire.read();
  b = Wire.read();

  x = a;
  x = x << 8 | b;

  r = !r ;
}

