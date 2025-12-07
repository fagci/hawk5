#include "components.h"
#include "../apps/apps.h"
#include "../driver/st7565.h"
#include "../helper/channels.h"
#include "../helper/measurements.h"
#include <stdint.h>

void UI_Battery(uint8_t Level) {
  DrawRect(LCD_WIDTH - 13, 0, 12, 5, C_FILL);
  FillRect(LCD_WIDTH - 12, 1, Level, 3, C_FILL);
  DrawVLine(LCD_WIDTH - 1, 1, 3, C_FILL);

  if (Level > 10) {
    DrawHLine(LCD_WIDTH - 4, 1, 3, C_INVERT);
    DrawHLine(LCD_WIDTH - 8, 1, 5, C_INVERT);
    DrawHLine(LCD_WIDTH - 4, 3, 3, C_INVERT);
  }
}

void UI_TxBar(uint8_t y) {
  FillRect(0, y, LCD_WIDTH, 8, C_CLEAR);
  PrintMediumEx(LCD_WIDTH - 1, y + 7, POS_R, C_FILL, "%u%c",
                ctx->tx_state.power_level,
                ctx->tx_state.pa_enabled ? '+' : '-');
}

void UI_RSSIBar(uint8_t y) {
  uint16_t rssi = vfo->msm.rssi;
  uint8_t snr = vfo->msm.snr;
  if (!rssi)
    return;

  const uint8_t BAR_WIDTH = LCD_WIDTH - 24;
  const uint8_t BAR_BASE = y + 7;
  const bool isUhf = ctx->frequency >= 30 * MHZ;

  const uint8_t *r2dBm = rssi2s[isUhf];
  const uint8_t numTicks = 12;
  const int16_t DBM_MIN = -r2dBm[0];
  const int16_t DBM_MAX = -r2dBm[numTicks - 1];

  uint8_t att = ctx->radio_type == RADIO_BK4819 ? BK4819_GetAttenuation() : 0;
  int16_t dBm = Rssi2DBm(rssi) + att;
  uint8_t rssiW = ConvertDomain(dBm, DBM_MIN, DBM_MAX, 0, BAR_WIDTH);
  uint8_t barH = ConvertDomain(snr, 0, 30, 1, 6);

  FillRect(0, y, LCD_WIDTH, 8, C_CLEAR);

  PrintMediumEx(LCD_WIDTH - 1, BAR_BASE, POS_R, C_FILL, "%+3d", dBm);

  FillRect(0, BAR_BASE - barH, rssiW, barH, C_FILL);
  DrawHLine(0, BAR_BASE, BAR_WIDTH, C_FILL);

  for (uint8_t i = 1; i <= numTicks; i++) {
    dBm = -r2dBm[i - 1];
    uint8_t x = ConvertDomain(dBm, DBM_MIN, DBM_MAX, 0, BAR_WIDTH);
    uint8_t height = (i == 9) ? 4 : 2;
    FillRect(x, BAR_BASE - height, 1, height, C_INVERT);
  }
}

void UI_Scanlists(uint8_t baseX, uint8_t baseY, uint16_t sl) {
  for (uint8_t i = 0; i < 16; ++i) {
    bool isActive = (sl >> i) & 1;
    uint8_t xi = i & 7, yi = i >> 3;
    FillRect(baseX + xi * 3 + (xi > 3) * 2, baseY + yi * 3 + (yi && !isActive),
             2, 1 + isActive, C_INVERT);
  }
}

void UI_DrawLoot(const Loot *loot, uint8_t x, uint8_t y, TextPos pos) {
  char c = loot->blacklist ? '-' : loot->whitelist ? '+' : ' ';
  PrintMediumEx(x, y, pos, C_INVERT, "%c%u.%05u %c", c, loot->f / MHZ,
                loot->f % MHZ, loot->open ? '!' : ' ');
}

void UI_BigFrequency(uint8_t y, uint32_t f) {
  PrintBiggestDigitsEx(LCD_WIDTH - 22, y, POS_R, C_FILL, "%4u.%03u", f / MHZ,
                       f / 100 % 1000);
  PrintMediumEx(LCD_WIDTH - 1, y, POS_R, C_FILL, "%02u", f % 100);
}

void UI_DisplayScanlists(uint32_t y) {
  char buf[17];
  ScanlistStr(gSettings.currentScanlist, buf);
  PrintMediumEx(LCD_XCENTER, y, POS_C, C_FILL, "%.4s %.4s %.4s %.4s", buf,
                buf + 4, buf + 8, buf + 12);
}

void UI_RenderScanScreen() {
  if (gScanlistSize) {
    uint32_t f = RADIO_GetParam(ctx, PARAM_FREQUENCY);
    PrintMediumEx(LCD_XCENTER, 26, POS_C, C_FILL, "%u.%05u", f / MHZ, f % MHZ);
    if (vfo->is_open)
      UI_RSSIBar(28);
  } else {
    PrintMediumBoldEx(LCD_XCENTER, 18, POS_C, C_FILL, "Scanlist empty");
  }

  if (gLastActiveLoot) {
    UI_DrawLoot(gLastActiveLoot, LCD_XCENTER, 50, POS_C);
  }
  UI_DisplayScanlists(60);
}
