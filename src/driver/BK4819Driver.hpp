#pragma once

#include "../board.h"
#include "../helper/battery.h"
#include "IRadioDriver.hpp"
#include "RadioCommon.hpp"
#include "bk4819-regs.h"
#include "bk4819.h"
#include "systick.h"
#include "uart.h"

// ============================================================================
// BK4819 DRIVER
// ============================================================================

class BK4819Driver : public IRadioDriver {
public:
  BK4819Driver()
      : IRadioDriver(), applyingFrequency_(false), txState_(TxState::TX_OFF) {

    // ====================================================================
    // FREQUENCY - контекстно-зависимые границы
    // ====================================================================
    params_[(uint8_t)ParamId::Frequency] = Param<uint32_t>(
        145500000, this, [](void *ctx) -> uint32_t { return 5000; }, // getMin
        [](void *ctx) -> uint32_t { return 130000000; },             // getMax
        [](void *ctx, uint32_t v) {                                  // apply
          auto *driver = static_cast<BK4819Driver *>(ctx);
          if (driver->powerState_ == 0 || driver->applyingFrequency_)
            return;

          driver->applyingFrequency_ = true; // защита от рекурсии

          BK4819_TuneTo(v, false);
          BK4819_SelectFilterEx((v < SETTINGS_GetFilterBound()) ? FILTER_VHF
                                                                : FILTER_UHF);

          // АВТО-ПЕРЕКЛЮЧЕНИЕ МОДУЛЯЦИИ
          auto &modParam = driver->getParam(ParamId::Modulation);

          if (v >= 8800000 && v <= 10800000) {
            // FM broadcast диапазон: 88-108 МГц
            modParam.set((uint32_t)ModType::WFM);
          } else if (v >= 13000000) {
            // VHF/UHF - FM по умолчанию
            if (modParam.get() == (uint32_t)ModType::WFM) {
              modParam.set((uint32_t)ModType::FM);
            }
          } else {
            // КВ - AM по умолчанию
            if (modParam.get() == (uint32_t)ModType::WFM ||
                modParam.get() == (uint32_t)ModType::FM) {
              modParam.set((uint32_t)ModType::AM);
            }
          }

          driver->applyingFrequency_ = false;
        },
        PARAM_PERSIST);

    params_[(uint8_t)ParamId::TxState] =
        Param<uint32_t>((uint32_t)TxState::TX_OFF, 0, 6, this,
                        [](void *ctx, uint32_t v) {
                          auto *driver = static_cast<BK4819Driver *>(ctx);

                          if (v == 0) {
                            // Выключить передачу
                            driver->stopTx();
                          } else if (v == 1) {
                            // Попытка включить передачу
                            driver->startTx();
                          }

                          // После попытки - обновить реальное состояние
                          driver->getParam(ParamId::TxState)
                              .set((uint32_t)driver->txState_, true);
                        },
                        PARAM_ACTION // Не сохраняем в EEPROM
        );

    // ====================================================================
    // MODULATION
    // ====================================================================
    params_[(uint8_t)ParamId::Modulation] =
        Param<uint32_t>((uint32_t)ModType::FM, 0, 6, this,
                        [](void *ctx, uint32_t v) {
                          auto *driver = static_cast<BK4819Driver *>(ctx);
                          if (driver->powerState_ == 0)
                            return;
                          BK4819_SetModulation((ModulationType)v);
                        },
                        PARAM_PERSIST);

    // ====================================================================
    // BANDWIDTH
    // ====================================================================
    params_[(uint8_t)ParamId::Bandwidth] = Param<uint32_t>(
        BK4819_FILTER_BW_23k, BK4819_FILTER_BW_12k, BK4819_FILTER_BW_6k, this,
        [](void *ctx, uint32_t v) {
          auto *driver = static_cast<BK4819Driver *>(ctx);
          if (driver->powerState_ == 0)
            return;
          BK4819_SetFilterBandwidth((BK4819_FilterBandwidth_t)v);
        },
        PARAM_PERSIST);

    // ====================================================================
    // GAIN (LNA+PGA) - зависит от частоты
    // ====================================================================
    params_[(uint8_t)ParamId::Gain] = Param<uint32_t>(
        34, this, [](void *ctx) -> uint32_t { return 0; },
        [](void *ctx) -> uint32_t {
          auto *driver = static_cast<BK4819Driver *>(ctx);
          uint32_t freq = driver->getParam(ParamId::Frequency).get();
          return (freq < SETTINGS_GetFilterBound()) ? 72 : 78;
        },
        [](void *ctx, uint32_t v) {
          auto *driver = static_cast<BK4819Driver *>(ctx);
          if (driver->powerState_ == 0)
            return;

          uint32_t freq = driver->getParam(ParamId::Frequency).get();
          uint8_t lna = (v / 24);
          uint8_t pga =
              (freq < SETTINGS_GetFilterBound()) ? (v % 24) / 3 : (v % 24) / 6;
          BK4819_WriteRegister((BK4819_REGISTER_t)0x13,
                               (lna << 8) | (pga << 0));
        },
        PARAM_PERSIST);

    // ====================================================================
    // SQUELCH
    // ====================================================================
    params_[(uint8_t)ParamId::Squelch] = Param<uint32_t>(
        3, 0, 9, this,
        [](void *ctx, uint32_t v) {
          auto *driver = static_cast<BK4819Driver *>(ctx);
          if (driver->powerState_ == 0)
            return;
          BK4819_Squelch(v, 0, 0);
        },
        PARAM_PERSIST);

    // ====================================================================
    // STEP
    // ====================================================================
    params_[(uint8_t)ParamId::Step] =
        Param<uint32_t>(1250, 125, 10000000, nullptr, nullptr, PARAM_PERSIST);

    // ====================================================================
    // VOLUME
    // ====================================================================
    params_[(uint8_t)ParamId::Volume] = Param<uint32_t>(
        8, 0, 15, this,
        [](void *ctx, uint32_t v) {
          auto *driver = static_cast<BK4819Driver *>(ctx);
          if (driver->powerState_ == 0)
            return;
          BK4819_WriteRegister((BK4819_REGISTER_t)0x48, (v & 0x0F) | 0xB040);
        },
        PARAM_PERSIST);

    // ====================================================================
    // READ-ONLY PARAMETERS (измерения)
    // ====================================================================
    params_[(uint8_t)ParamId::RSSI] =
        Param<uint32_t>(0, 0, 512, PARAM_READONLY);
    params_[(uint8_t)ParamId::Noise] =
        Param<uint32_t>(0, 0, 255, PARAM_READONLY);
    params_[(uint8_t)ParamId::Glitch] =
        Param<uint32_t>(0, 0, 255, PARAM_READONLY);
    params_[(uint8_t)ParamId::SNR] = Param<uint32_t>(0, 0, 255, PARAM_READONLY);
    params_[(uint8_t)ParamId::SquelchOpen] =
        Param<uint32_t>(0, 0, 1, PARAM_READONLY);

    // ====================================================================
    // ACTIONS (без сохранения)
    // ====================================================================
    params_[(uint8_t)ParamId::PowerOn] = Param<uint32_t>(
        0, 0, 1, this,
        [](void *ctx, uint32_t v) {
          auto *driver = static_cast<BK4819Driver *>(ctx);
          if (v && driver->powerState_ == 0) {
            driver->powerOn();
          }
        },
        PARAM_ACTION);

    params_[(uint8_t)ParamId::PowerOff] = Param<uint32_t>(
        0, 0, 1, this,
        [](void *ctx, uint32_t v) {
          auto *driver = static_cast<BK4819Driver *>(ctx);
          if (v && driver->powerState_ > 0) {
            driver->powerOff();
          }
        },
        PARAM_ACTION);

    params_[(uint8_t)ParamId::RxMode] = Param<uint32_t>(
        0, 0, 1, this,
        [](void *ctx, uint32_t v) { (v ? BK4819_RX_TurnOn : BK4819_Idle)(); },
        PARAM_ACTION);
  }

  RadioType getRadioType() const override { return RadioType::BK4819; }
  const char *getRadioName() const override { return "BK4819"; }

  void powerOn() {
    if (powerState_ > 0)
      return;
    powerState_ = 1;
    BK4819_Init();

    // Применяем все параметры после включения
    getParam(ParamId::Frequency).set(getParam(ParamId::Frequency).get(), true);
    getParam(ParamId::Modulation)
        .set(getParam(ParamId::Modulation).get(), true);
    getParam(ParamId::Bandwidth).set(getParam(ParamId::Bandwidth).get(), true);
    getParam(ParamId::Gain).set(getParam(ParamId::Gain).get(), true);
    getParam(ParamId::Squelch).set(getParam(ParamId::Squelch).get(), true);
    getParam(ParamId::Volume).set(getParam(ParamId::Volume).get(), true);
  }

  void powerOff() {
    if (powerState_ == 0)
      return;
    BK4819_Sleep();
    powerState_ = 0;
  }

  // Обновление read-only параметров
  void updateMeasurements() {
    if (powerState_ == 0)
      return;
    getParam(ParamId::RSSI).set(BK4819_GetRSSI(), true);
    getParam(ParamId::Noise).set(BK4819_GetNoise(), true);
    getParam(ParamId::Glitch).set(BK4819_GetGlitch(), true);
    getParam(ParamId::SNR).set(BK4819_GetSNR(), true);
    getParam(ParamId::SquelchOpen).set(BK4819_IsSquelchOpen(), true);
  }

private:
  bool applyingFrequency_; // защита от циклических зависимостей
  TxState txState_;

  // Проверка возможности передачи
  TxState checkTxPossible() {
    // 1. Проверка upconverter
    if (gSettings.upconverter) {
      return TxState::TX_UPCONV;
    }

    // 2. Проверка батареи
    if (gBatteryPercent == 0) {
      return TxState::TX_BAT_LOW;
    }

    // 3. Проверка зарядки
    if (gChargingWithTypeC || gBatteryVoltage > 880) {
      return TxState::TX_CHARGING;
    }

    // 4. Проверка что радио BK4819 (другие не поддерживают TX)
    if (getRadioType() != RadioType::BK4819) {
      return TxState::TX_DISABLED;
    }

    // 5. Проверка состояния питания
    if (powerState_ == 0) {
      return TxState::TX_DISABLED;
    }

    // Всё ОК!
    return TxState::TX_ON;
  }

  // Включить передачу
  void startTx() {
    // Проверка ограничений
    TxState checkResult = checkTxPossible();

    if (checkResult != TxState::TX_ON) {
      // Ошибка! Установить состояние ошибки
      txState_ = checkResult;

      // Обновить параметры для UI
      getParam(ParamId::TxState).set((uint32_t)txState_, true);

      Log("TX FAILED: %u", (uint32_t)checkResult);
      return;
    }

    // Уже в TX?
    if (txState_ == TxState::TX_ON) {
      return;
    }

    // Включаем передачу на железе
    uint32_t freq = getParam(ParamId::Frequency).get();
    uint8_t power = 2; // Можно добавить параметр TxPower

    Log("TX START at %u Hz", freq);

    // Отключить RX
    BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, false);

    // Настроить частоту TX
    BK4819_SelectFilter(freq);
    BK4819_TuneTo(freq, true);

    // Подготовить TX
    BK4819_PrepareTransmit();
    TIMER_DelayMs(10);

    // Включить PA
    bool paEnabled = true; // Можно добавить параметр
    BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, paEnabled);
    TIMER_DelayMs(5);

    // Установить мощность
    BK4819_SetupPowerAmplifier(power, freq);
    TIMER_DelayMs(10);

    // Состояние успешно!
    txState_ = TxState::TX_ON;

    // Обновить параметры
    getParam(ParamId::TxState).set((uint32_t)txState_, true);

    // Индикатор (красный LED)
    BOARD_ToggleRed(true);
  }

  // Выключить передачу
  void stopTx() {
    if (txState_ != TxState::TX_ON) {
      return;
    }

    Log("TX STOP");

    // EOT (end of transmission) tone
    BK4819_ExitSubAu();
    if (gSettings.roger) {
      BK4819_PlayRogerTiny();
    }

    // Выключить PA и TX
    BK4819_TurnsOffTones_TurnsOnRX();
    BK4819_SetupPowerAmplifier(0, 0);
    BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false);
    BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, true);

    // Вернуть частоту RX
    uint32_t freq = getParam(ParamId::Frequency).get();
    BK4819_SelectFilter(freq);
    BK4819_TuneTo(freq, true);

    // Состояние
    txState_ = TxState::TX_OFF;
    getParam(ParamId::TxState).set((uint32_t)txState_, true);

    // Индикатор
    BOARD_ToggleRed(false);
  }
};
