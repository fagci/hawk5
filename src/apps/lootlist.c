#include "lootlist.h"
#include "../dcs.h"
#include "../driver/st7565.h"
#include "../driver/system.h"
#include "../driver/uart.h"
#include "../helper/bands.h"
#include "../helper/channels.h"
#include "../helper/lootlist.h"
#include "../helper/measurements.h"
#include "../helper/menu.h"
#include "../radio.h"
#include "../scheduler.h"
#include "../ui/components.h"
#include "../ui/graphics.h"
#include "../ui/statusline.h"
#include "apps.h"
#include "chlist.h"
#include <stdbool.h>
#include <stdint.h>

static uint8_t menuIndex = 0;
static const uint8_t MENU_ITEM_H_LARGER = 15;
static void initMenu();

typedef enum {
  SORT_LOT,
  SORT_DUR,
  SORT_BL,
  SORT_F,
} Sort;

bool (*sortings[])(const Loot *a, const Loot *b) = {
    LOOT_SortByLastOpenTime,
    LOOT_SortByDuration,
    LOOT_SortByBlacklist,
    LOOT_SortByF,
};

static char *sortNames[] = {
    "last open",
    "duration",
    "blacklist",
    "freq",
};

static Sort sortType = SORT_LOT;

static bool shortList = true;
static bool sortRev = false;

static void tuneToLoot(const Loot *loot, bool save) {
  BANDS_SetRadioParamsFromCurrentBand();
  RADIO_SetParam(ctx, PARAM_FREQUENCY, loot->f, save);
  RADIO_ApplySettings(ctx);
}

static void displayFreqBlWl(uint8_t y, const Loot *loot) {
  UI_DrawLoot(loot, 1, y + 7, POS_L);
}

static void getLootItem(uint16_t i, uint16_t index, bool isCurrent) {
  const Loot *item = LOOT_Item(index);
  const uint8_t y = MENU_Y + i * MENU_ITEM_H_LARGER;

  displayFreqBlWl(y, item);

  PrintSmallEx(LCD_WIDTH - 6, y + 7, POS_R, C_INVERT, "%us",
               item->duration / 1000);

  // PrintSmallEx(8, y + 7 + 6, POS_L, C_INVERT, "%03ddB",
  // Rssi2DBm(item->rssi));
  if (item->ct != 0xFF) {
    PrintSmallEx(8 + 55, y + 7 + 6, POS_L, C_INVERT, "CT:%u.%uHz",
                 CTCSS_Options[item->ct] / 10, CTCSS_Options[item->ct] % 10);
  } else if (item->cd != 0xFF) {
    PrintSmallEx(8 + 55, y + 7 + 6, POS_L, C_INVERT, "DCS:D%03oN",
                 DCS_Options[item->cd]);
  }
}

static void getLootItemShort(uint16_t i, uint16_t index, bool isCurrent) {
  const Loot *loot = LOOT_Item(index);
  const uint8_t x = LCD_WIDTH - 6;
  const uint8_t y = MENU_Y + i * MENU_ITEM_H;
  const uint32_t ago = (Now() - loot->lastTimeOpen) / 1000;

  displayFreqBlWl(y, loot);

  switch (sortType) {
  case SORT_LOT:
    PrintSmallEx(x, y + 7, POS_R, C_INVERT, "%u:%02u", ago / 60, ago % 60);
    break;
  case SORT_DUR:
  case SORT_BL:
  case SORT_F:
    PrintSmallEx(x, y + 7, POS_R, C_INVERT, "%us", loot->duration / 1000);
    break;
  }
}

static void renderItem(uint16_t index, uint8_t i, bool isCurrent) {
  (shortList ? getLootItemShort : getLootItem)(i, index, isCurrent);
}

static void sort(Sort type) {
  if (sortType == type) {
    sortRev = !sortRev;
  } else {
    sortRev = type == SORT_DUR;
  }
  LOOT_Sort(sortings[type], sortRev);
  sortType = type;
  STATUSLINE_SetText("By %s %s", sortNames[sortType], sortRev ? "v" : "^");
}

static void saveLootToCh(const Loot *loot, int16_t chnum, uint16_t scanlist) {
  CH ch = LOOT_ToCh(loot);
  ch.scanlists = scanlist;
  CHANNELS_Save(chnum, &ch);
}

static void saveToFreeChannels(bool saveWhitelist, uint16_t scanlist) {
  uint32_t saved = 0;
  for (uint16_t i = 0; i < LOOT_Size(); ++i) {
    uint16_t chnum = CHANNELS_GetCountMax();
    const Loot *loot = LOOT_Item(i);
    if (saveWhitelist && !loot->whitelist) {
      continue;
    }
    if (!saveWhitelist && !loot->blacklist) {
      continue;
    }

    while (chnum) {
      chnum--;
      if (CHANNELS_GetMeta(chnum).type == TYPE_EMPTY) {
        // save new
        saveLootToCh(loot, chnum, scanlist);
        saved++;
        break;
      } else {
        CH ch;
        CHANNELS_Load(chnum, &ch);
        if (ch.rxF == loot->f && ch.meta.type == TYPE_CH) {
          break;
        }
      }
    }
  }

  FillRect(0, LCD_YCENTER - 4, LCD_WIDTH, 9, C_FILL);
  PrintMediumBoldEx(LCD_XCENTER, LCD_YCENTER + 3, POS_C, C_INVERT, "Saved: %u",
                    saved);
  ST7565_Blit();
  SYS_DelayMs(2000);
}

static bool action(const uint16_t index, KEY_Code_t key, Key_State_t state) {

  Loot *loot;
  loot = LOOT_Item(index);
  const uint8_t MENU_SIZE = LOOT_Size();

  VFOContext *ctx = &RADIO_GetCurrentVFO(&gRadioState)->context;

  if (state == KEY_LONG_PRESSED) {
    switch (key) {
    case KEY_0:
      LOOT_Clear();
      RADIO_SetParam(ctx, PARAM_FREQUENCY, 0, false);
      RADIO_ApplySettings(ctx);
      return true;
    case KEY_SIDE1:
      gMonitorMode = !gMonitorMode;
      return true;
    case KEY_8:
      saveToFreeChannels(false, 7);
      return true;
    case KEY_5:
      saveToFreeChannels(true, gSettings.currentScanlist);
      return true;
    case KEY_STAR:
      // TODO: select any of SL
      CHANNELS_LoadBlacklistToLoot();
      return true;
    default:
      break;
    }
  }

  if (state == KEY_RELEASED || state == KEY_LONG_PRESSED_CONT) {
    switch (key) {
    case KEY_UP:
    case KEY_DOWN:
      tuneToLoot(loot, false);
      return true;
    default:
      break;
    }
  }

  if (state == KEY_RELEASED) {
    switch (key) {
    case KEY_EXIT:
      APPS_exit();
      return true;
    case KEY_PTT:
      tuneToLoot(loot, true);
      APPS_run(APP_VFO1);
      return true;
    case KEY_1:
      sort(SORT_LOT);
      return true;
    case KEY_2:
      sort(SORT_DUR);
      return true;
    case KEY_3:
      sort(SORT_BL);
      return true;
    case KEY_4:
      sort(SORT_F);
      return true;
    case KEY_SIDE1:
      loot->whitelist = false;
      loot->blacklist = !loot->blacklist;
      return true;
    case KEY_SIDE2:
      loot->blacklist = false;
      loot->whitelist = !loot->whitelist;
      return true;
    case KEY_7:
      shortList = !shortList;
      initMenu();
      return true;
    case KEY_9:
      return true;
    case KEY_5:
      tuneToLoot(loot, false);
      gChListFilter = TYPE_FILTER_CH_SAVE;
      APPS_run(APP_CH_LIST);
      return true;
    case KEY_0:
      LOOT_Remove(menuIndex);
      if (menuIndex > LOOT_Size() - 1) {
        menuIndex = LOOT_Size() - 1;
      }
      loot = LOOT_Item(menuIndex);
      if (loot) {
        tuneToLoot(loot, false);
      } else {
        RADIO_SetParam(ctx, PARAM_FREQUENCY, 0, false);
        RADIO_ApplySettings(ctx);
      }
      return true;
    case KEY_MENU:
      tuneToLoot(loot, true);
      APPS_exit();
      return true;
    default:
      break;
    }
  }
}

static uint32_t lastSqCheck;
void LOOTLIST_update() {
  RADIO_UpdateMultiwatch(&gRadioState);
  RADIO_CheckAndSaveVFO(&gRadioState);

  if (Now() - lastSqCheck >= 55) {
    RADIO_UpdateSquelch(&gRadioState);
    lastSqCheck = Now();
  }
  gRedrawScreen = true;
  SYS_DelayMs(SQL_DELAY);
}

static Menu lootMenu = {"Loot", .render_item = renderItem, .action = action};

static void initMenu() {
  lootMenu.num_items = LOOT_Size();
  lootMenu.itemHeight = shortList ? MENU_ITEM_H : MENU_ITEM_H_LARGER;
  MENU_Init(&lootMenu);
}

void LOOTLIST_render(void) { MENU_Render(); }

void LOOTLIST_init(void) {
  initMenu();
  sortType = SORT_F;
  sort(SORT_LOT);
  if (LOOT_Size()) {
    tuneToLoot(LOOT_Item(menuIndex), false);
  }
}

bool LOOTLIST_key(KEY_Code_t key, Key_State_t state) {
  if (MENU_HandleInput(key, state)) {
    return true;
  }

  return false;
}
