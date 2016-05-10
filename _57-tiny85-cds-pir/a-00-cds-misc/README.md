#### Photocells / cds tested ####

- cds R/Ω : 500 ~ 500K
- vcc - cds - (A1) - 22K - gnd
- check vcc using internal ref.
- vo(A0) = vcc (R / (R + cds))
- use map
- aref ? http://electronics.stackexchange.com/questions/60237/attiny-default-aref

- pin
- PB2/P7/D2/A1 --> CDS
- PB1/P6/D1    --> LED
- PB0/P5/D0    --> INT / PIR
- PB4/P3/D4    --> Serial TX
- PB3/P2/D3    --> Serial RX


