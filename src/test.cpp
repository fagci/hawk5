#pragma once

extern "C" {
#include "./ui/graphics.h"
}

class Lalala {
public:
  void drawSomething() {
    PrintBiggestDigitsEx(LCD_XCENTER, LCD_YCENTER, POS_C, C_FILL, "123");
  }
};

static Lalala *dr;

extern "C" {
void DRAW(void) {
  dr = new Lalala();
  dr->drawSomething();
}
}
