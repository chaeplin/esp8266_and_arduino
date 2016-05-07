#### Photocells / cds tested ####

- cds R/Ω : 500 ~ 500K
- Vcc - cds - (A1) - 22K - gnd
- check vcc using internal ref.
- Vo(A0) = Vcc (R / (R + cds))
- use map ?
- aref ? http://electronics.stackexchange.com/questions/60237/attiny-default-aref

- analogRead(A1) : 0 ~ 1023
280 ~ 950

- pin
- PB2/P7/D2/A1 --> CDS
- PB1/P6/D1    --> LED
- PB0/P5/D0    --> INT
- PB4/P3/D4    --> Serial TX
- PB3/P2/D3    --> Serial RX


