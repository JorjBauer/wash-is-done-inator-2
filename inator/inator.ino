#include <Arduino.h>
#include <SPI.h>

#define DAC_RESOLUTION    5
#define DAC_ARRAY_INDICES (pow(2,DAC_RESOLUTION))

SPISettings settingsA(16000000, MSBFIRST, SPI_MODE0);  // At 16 = SPI Clock = 8MHz.

const PROGMEM uint16_t SineLookup_5bits[32]
{
2048, 2447, 2831, 3185, 3495, 3750, 3939, 4056,
4095, 4056, 3939, 3750, 3495, 3185, 2831, 2447,
2048, 1648, 1264,  910,  600,  345,  156,   39,
   0,   39,  156,  345,  600,  910, 1264, 1648
};

// Pins seem to be by GPIO number, not D# - blink uses 2 (GPIO2) not 4 (D4)
int SSPin  = 15; // D8 == GPIO15
int SRCLKPin = 14; // D5 = GPIO14
int SERPin   = 13;    //  MOSI, GPIO13, D7
#define MARKER 2 // led pin = GPIO2, D4
//////////////////////////////////////////////////////////////////////////////
void setup() {
  pinMode(SSPin,  OUTPUT);   // Set SPI control PINs to output.
  pinMode(SRCLKPin, OUTPUT);
  pinMode(SERPin,   OUTPUT);
  pinMode(MARKER,   OUTPUT);

  SPI.begin();

  noInterrupts();
}

//////////////////////////////////////////////////////////////////////////////
// 0 - A, 1 - B
//
void writeMCP4922_AB(byte AB, uint16_t v) {

    v |=0xf000;             // B15(A/B)=1 B, B14(BUF)=1 on, B13(GAn) 1=x1  B12(SHDNn) 1=off
    if (!AB)  v &= ~0x8000; // When zero clear B15 for A.

    SPI.beginTransaction(settingsA);
    digitalWrite(SSPin, LOW);
    SPI.transfer( (0xff00 & v)>>8 );
    SPI.transfer(      0x00ff & v );
    digitalWrite(SSPin, HIGH);
    SPI.endTransaction();
}

void loop() {
   for (int i = 0; i < DAC_ARRAY_INDICES; i++) {
      digitalWrite(MARKER, LOW);
      digitalWrite(MARKER, HIGH);
      writeMCP4922_AB( 0, pgm_read_word(&(SineLookup_5bits[i])) );
   }
}
