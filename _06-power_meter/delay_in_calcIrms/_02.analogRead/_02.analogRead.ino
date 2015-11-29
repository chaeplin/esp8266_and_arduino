int sampleI;
int Number_of_Samples;
int sumI;

void setup()
{  
  Serial.begin(115200);
  sampleI = 0;
  sumI    = 0;
  Number_of_Samples = 1000;
}

void loop()
{
  long start_adc_now = micros();
  
  for (unsigned int n = 0; n < Number_of_Samples; n++)
  {
    sampleI = analogRead(A0);
    sumI += sampleI;
  }
  
  long stop_adc_now = micros();
  
  Serial.print("total delay in adc : ");
  Serial.print((stop_adc_now - start_adc_now) / 1000);
  Serial.print(" ms ---> each analogRead : ");
  Serial.print((stop_adc_now - start_adc_now) / Number_of_Samples );
  Serial.print(" micros ---> analogRead : ");
  Serial.println(sumI / Number_of_Samples);

  sumI = 0;
  
  //delayMicroseconds(10);
  delay (1000);

}
