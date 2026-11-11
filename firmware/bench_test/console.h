// Console that prints on both USB-C ports of the dev board:
// the UART bridge port (UART0) and the ESP32-S3's native USB serial port.
#pragma once
#include <Arduino.h>

#if ARDUINO_USB_CDC_ON_BOOT
#define CON_UART Serial0
#define CON_USB  Serial
#else
#define CON_UART Serial
#endif

class DualConsole : public Print {
public:
  void begin(unsigned long baud) {
    CON_UART.begin(baud);
#ifdef CON_USB
    CON_USB.begin(baud);
    CON_USB.setTxTimeoutMs(0);   // never stall when no PC is listening on the native port
#endif
  }
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t *b, size_t n) override {
    CON_UART.write(b, n);
#ifdef CON_USB
    CON_USB.write(b, n);
#endif
    return n;
  }
  int read() {
    if (CON_UART.available()) return CON_UART.read();
#ifdef CON_USB
    if (CON_USB.available()) return CON_USB.read();
#endif
    return -1;
  }
};

static DualConsole con;
