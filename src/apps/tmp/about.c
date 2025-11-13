#include "about.h"
#include "../ui/graphics.h"

void ABOUT_Render() {
  PrintMediumEx(LCD_XCENTER, LCD_YCENTER - 8, POS_C, C_FILL, "hawk5");
  PrintSmallEx(LCD_XCENTER, LCD_YCENTER, POS_C, C_FILL, "by FAGCI");
  PrintSmallEx(LCD_XCENTER, LCD_YCENTER + 8, POS_C, C_FILL, TIME_STAMP);
  PrintSmallEx(LCD_XCENTER, LCD_YCENTER + 24, POS_C, C_FILL,
               "t.me/uvk5_spectrum_talk");
}
