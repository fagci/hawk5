#ifndef RADIO_H
#define RADIO_H

#include "driver/bk4819.h"
#include "helper/channels.h"
#include "helper/lootlist.h"
#include <stdbool.h>
#include <stdint.h>

#define SQL_DELAY 50
#define GARBAGE_FREQUENCY_MOD (13 * MHZ)

#define MAX_VFOS 16

extern bool gShowAllRSSI;
extern bool gMonitorMode;
extern const char *PARAM_NAMES[];
extern const char *TX_STATE_NAMES[7];
extern const char *FLT_BOUND_NAMES[2];
extern const char *BW_NAMES_BK4819[10];
extern const char *BW_NAMES_SI47XX[7];
extern const char *BW_NAMES_SI47XX_SSB[6];
extern const char *SQ_TYPE_NAMES[4];
extern const char *MOD_NAMES_BK4819[8];
extern const char *RADIO_NAMES[3];

extern const uint16_t StepFrequencyTable[15];

// Параметры
typedef const enum {
  PARAM_RADIO,
  PARAM_FREQUENCY,
  PARAM_PRECISE_F_CHANGE,
  PARAM_STEP,
  PARAM_POWER,
  PARAM_MODULATION,
  PARAM_SQUELCH_TYPE,
  PARAM_SQUELCH_VALUE,
  PARAM_GAIN,
  PARAM_VOLUME,
  PARAM_BANDWIDTH,

  PARAM_TX_OFFSET,
  PARAM_TX_OFFSET_DIR,
  PARAM_TX_STATE,
  PARAM_TX_FREQUENCY,
  PARAM_TX_FREQUENCY_FACT,
  PARAM_TX_POWER,
  PARAM_TX_POWER_AMPLIFIER,

  PARAM_RX_CODE,
  PARAM_TX_CODE,

  PARAM_AFC,
  PARAM_AFC_SPD,
  PARAM_DEV,
  PARAM_MIC,
  PARAM_XTAL,
  PARAM_FILTER,

  PARAM_RSSI,
  PARAM_NOISE,
  PARAM_GLITCH,
  PARAM_SNR,

  PARAM_COUNT
} ParamType;

typedef enum {
  TX_UNKNOWN,
  TX_ON,
  TX_VOL_HIGH,
  TX_BAT_LOW,
  TX_DISABLED,
  TX_DISABLED_UPCONVERTER,
  TX_POW_OVERDRIVE,
} TXStatus;

typedef enum {
  RADIO_SCAN_STATE_IDLE,      // Сканирование не активно
  RADIO_SCAN_STATE_SWITCHING, // Сканирование не активно
  RADIO_SCAN_STATE_WARMUP, // Ожидание стабилизации после переключения
  RADIO_SCAN_STATE_MEASURING, // Выполнение замера
  RADIO_SCAN_STATE_DECISION // Принятие решения о переключении
} RadioScanState;

// Диапазон частот и его параметры
typedef struct {
  uint32_t min_freq;
  uint32_t max_freq;
  uint16_t available_bandwidths[10]; // Доступные полосы (кГц)
  uint8_t available_mods[5];         // Доступные модуляции
} FreqBand;

// Контекст VFO
typedef struct {
  char name[10];
  uint32_t last_save_time; // Время последнего сохранения
  uint32_t frequency;      // Текущая частота

  struct {
    uint32_t frequency; // Частота передачи (может отличаться от RX)
    uint8_t power_level; // Уровень мощности
    TXStatus last_error;
    ModulationType modulation; // Модуляция TX
    Code code;
    OffsetDirection offsetDirection;
    bool dirty;     // Флаг изменения параметров TX
    bool is_active; // true, если идёт передача
    bool pa_enabled;
  } tx_state;

  uint16_t bandwidth; // Полоса пропускания
  uint16_t dev;
  Radio radio_type;
  ModulationType modulation;    // Текущая модуляция
  uint8_t volume;               // Громкость
  const FreqBand *current_band; // Активный диапазон
  bool dirty[PARAM_COUNT];      // Флаги изменений
  Code code;
  Step step;
  Squelch squelch;
  TXOutputPower power;
  uint8_t gain;

  uint8_t afc;
  uint8_t afc_speed;
  uint8_t mic;
  XtalMode xtal;

  Filter filter;

  bool preciseFChange;
  bool fixed_bounds;

  bool save_to_eeprom; // Флаг необходимости сохранения в EEPROM
} VFOContext;

// Channel/VFO mode
typedef enum { MODE_VFO, MODE_CHANNEL } VFOMode;

// Extended VFO context
typedef struct {
  Measurement msm;             // TODO: implement
  VFOContext context;          // Existing VFO context
  uint32_t last_activity_time; // for multiwatch
  uint16_t channel_index;      // Channel index if in channel mode
  uint16_t vfo_ch_index;       // MR index of VFO
  VFOMode mode;                // VFO or channel mode
  bool is_active;              // Whether this is the active VFO
  bool is_open;
} ExtendedVFOContext;

// Global radio state
typedef struct {
  ExtendedVFOContext vfos[MAX_VFOS]; // Array of VFOs
  uint32_t last_scan_time;           // Last scan time
  uint8_t num_vfos;                  // Number of configured VFOs
  uint8_t active_vfo_index;          // Currently active VFO
  uint8_t primary_vfo_index;         //
  uint8_t last_active_vfo; // Последний активный VFO с активностью
  bool audio_routing_enabled; // Флаг управления аудио маршрутизацией
  bool multiwatch_enabled;   // Whether multiwatch is enabled
  RadioScanState scan_state; // Состояние сканирования
} RadioState;

// New functions for multi-VFO and multiwatch support
void RADIO_InitState(RadioState *state, uint8_t num_vfos);
bool RADIO_SwitchVFO(RadioState *state, uint8_t vfo_index);
void RADIO_LoadVFOFromStorage(RadioState *state, uint8_t vfo_index,
                              const VFO *storage);
void RADIO_SaveVFOToStorage(const RadioState *state, uint8_t vfo_index,
                            VFO *storage);
void RADIO_LoadChannelToVFO(RadioState *state, uint8_t vfo_index,
                            uint16_t channel_index);
bool RADIO_SaveCurrentVFO(RadioState *state);
void RADIO_ToggleMultiwatch(RadioState *state, bool enable);
void RADIO_UpdateMultiwatch(RadioState *state);
bool RADIO_ToggleVFOMode(RadioState *state, uint8_t vfo_index);

// Инициализация
void RADIO_Init(VFOContext *ctx, Radio radio_type);

// Установка параметра
void RADIO_SetParam(VFOContext *ctx, ParamType param, uint32_t value,
                    bool save_to_eeprom);

void RADIO_CheckAndSaveVFO(RadioState *state);

// Применение настроек
void RADIO_ApplySettings(VFOContext *ctx);

// Проверка поддержки параметра в текущем диапазоне
bool RADIO_IsParamValid(VFOContext *ctx, ParamType param, uint32_t value);

uint32_t RADIO_GetParam(const VFOContext *ctx, ParamType param);
bool RADIO_AdjustParam(VFOContext *ctx, ParamType param, uint32_t inc,
                       bool save_to_eeprom);
bool RADIO_IncDecParam(VFOContext *ctx, ParamType param, bool inc,
                       bool save_to_eeprom);
void RADIO_LoadVFOs(RadioState *state);

void RADIO_EnableAudioRouting(RadioState *state, bool enable);
void RADIO_UpdateAudioRouting(RadioState *state);

void RADIO_ToggleTX(VFOContext *ctx, bool on);
bool RADIO_IsSSB(const VFOContext *ctx);
const char *RADIO_GetParamValueString(const VFOContext *ctx, ParamType param);

uint8_t RADIO_GetCurrentVFONumber(const RadioState *state);
ExtendedVFOContext *RADIO_GetCurrentVFO(RadioState *state);
const ExtendedVFOContext *RADIO_GetCurrentVFOConst(const RadioState *state);

void RADIO_SwitchAudioToVFO(RadioState *state, uint8_t vfo_index);
void RADIO_UpdateSquelch(RadioState *state);

uint16_t RADIO_GetRSSI(const VFOContext *ctx);
uint8_t RADIO_GetSNR(const VFOContext *ctx);
uint8_t RADIO_GetNoise(const VFOContext *ctx);
uint8_t RADIO_GetGlitch(const VFOContext *ctx);

extern RadioState gRadioState;
extern ExtendedVFOContext *vfo;
extern VFOContext *ctx;

#endif // RADIO_H
