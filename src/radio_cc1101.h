#pragma once
#include <Arduino.h>
#include <SPI.h>

class RadioCC1101 {
public:
  bool begin(int sck, int miso, int mosi, int csn);
  bool probe();

  void reset();
  void idle();

  // Ustaw częstotliwość (Hz) – uproszczone
  void setFrequencyHz(uint32_t hz);

  // Odczyt RSSI (dBm przybliżone)
  float readRSSIdBm();

  // Prosty "sweep" – zwraca RSSI dla punktów częstotliwości
  // callback(freqHz, rssiDbm)
  void sweep(uint32_t startHz, uint32_t stopHz, uint32_t stepHz,
             std::function<void(uint32_t, float)> cb);

private:
  int _csn=-1;
  SPIClass* _spi=nullptr;

  uint8_t strobe(uint8_t cmd);
  void writeReg(uint8_t addr, uint8_t val);
  uint8_t readReg(uint8_t addr);
  uint8_t readStatus(uint8_t addr);

  void csLow();
  void csHigh();
};A