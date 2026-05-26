#include "radio_cc1101.h"

// CC1101 strobes
static constexpr uint8_t CC1101_SRES  = 0x30;
static constexpr uint8_t CC1101_SIDLE = 0x36;

// registers/status
static constexpr uint8_t CC1101_PARTNUM = 0x30;
static constexpr uint8_t CC1101_VERSION = 0x31;
static constexpr uint8_t CC1101_RSSI    = 0x34;

// Address flags
static constexpr uint8_t READ_SINGLE = 0x80;
static constexpr uint8_t READ_BURST  = 0xC0;

// Crystal on common CC1101 modules is 26MHz.
// FREQ = (freq * 2^16) / fxosc
static constexpr float FXOSC = 26e6f;

bool RadioCC1101::begin(int sck, int miso, int mosi, int csn) {
  _csn = csn;
  pinMode(_csn, OUTPUT);
  csHigh();

  SPI.begin(sck, miso, mosi);
  delay(5);

  reset();

  // Minimal sane setup for RSSI reading:
  // Put in IDLE, set some defaults if needed (optional).
  idle();

  return probe();
}

bool RadioCC1101::probe() {
  // Just check that we can read something non-0x00/0xFF often indicates wiring issue
  uint8_t part = readStatus(CC1101_PARTNUM);
  uint8_t ver  = readStatus(CC1101_VERSION);
  if (part == 0x00 || part == 0xFF) return false;
  if (ver  == 0x00 || ver  == 0xFF) return false;
  return true;
}

void RadioCC1101::reset() {
  csHigh();
  delayMicroseconds(5);
  csLow();
  delayMicroseconds(10);
  csHigh();
  delayMicroseconds(41);
  csLow();
  delayMicroseconds(10);

  strobe(CC1101_SRES);
  csHigh();
  delay(1);
}

void RadioCC1101::idle() {
  strobe(CC1101_SIDLE);
}

void RadioCC1101::setFrequencyHz(uint32_t hz) {
  // CC1101 freq regs: FREQ2, FREQ1, FREQ0 at 0x0D..0x0F
  uint32_t f = (uint32_t)((hz * 65536.0f) / FXOSC);
  uint8_t freq2 = (f >> 16) & 0xFF;
  uint8_t freq1 = (f >> 8) & 0xFF;
  uint8_t freq0 = (f) & 0xFF;

  writeReg(0x0D, freq2);
  writeReg(0x0E, freq1);
  writeReg(0x0F, freq0);
}

float RadioCC1101::readRSSIdBm() {
  uint8_t raw = readStatus(CC1101_RSSI);
  int16_t rssi_dec = (raw >= 128) ? (raw - 256) : raw;
  // From datasheet: RSSI_dBm = (RSSI_dec / 2) - RSSI_offset
  // RSSI_offset ~ 74 for 868/915 MHz typical; depends on band.
  const float offset = 74.0f;
  return (rssi_dec / 2.0f) - offset;
}

void RadioCC1101::sweep(uint32_t startHz, uint32_t stopHz, uint32_t stepHz,
                        std::function<void(uint32_t, float)> cb) {
  if (stepHz == 0) return;
  for (uint32_t f = startHz; f <= stopHz; f += stepHz) {
    setFrequencyHz(f);
    delay(2);
    float rssi = readRSSIdBm();
    cb(f, rssi);
    delay(2);
    // break guard overflow
    if (UINT32_MAX - f < stepHz) break;
  }
}

void RadioCC1101::csLow()  { digitalWrite(_csn, LOW); }
void RadioCC1101::csHigh() { digitalWrite(_csn, HIGH); }

uint8_t RadioCC1101::strobe(uint8_t cmd) {
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  csLow();
  uint8_t status = SPI.transfer(cmd);
  csHigh();
  SPI.endTransaction();
  return status;
}

void RadioCC1101::writeReg(uint8_t addr, uint8_t val) {
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  csLow();
  SPI.transfer(addr);
  SPI.transfer(val);
  csHigh();
  SPI.endTransaction();
}

uint8_t RadioCC1101::readReg(uint8_t addr) {
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  csLow();
  SPI.transfer(addr | READ_SINGLE);
  uint8_t v = SPI.transfer(0x00);
  csHigh();
  SPI.endTransaction();
  return v;
}

uint8_t RadioCC1101::readStatus(uint8_t addr) {
  // status regs use burst read bit too in some libs; READ_BURST is common approach
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  csLow();
  SPI.transfer(addr | READ_BURST);
  uint8_t v = SPI.transfer(0x00);
  csHigh();
  SPI.endTransaction();
  return v;
}