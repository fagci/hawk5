#pragma once

#include "../helper/channels.h"
#include "BK1080Driver.hpp"
#include "BK4819Driver.hpp"
#include "IRadioDriver.hpp"
#include "SI4732Driver.hpp"
#include "RadioCommon.hpp"
#include "uart.h"

// ============================================================================
// VFO PROXY
// ============================================================================
class VFOProxy {
public:
    VFOProxy(IRadioDriver* driver) : driver_(driver) {}
    
    ParamProxyFunc operator[](ParamId id) {
        if (driver_) {
            return (*driver_)[id];  // Разыменовываем указатель
        }
        static Param<std::function<void(uint32_t)>> dummy;
        return dummy.proxy();
    }
    
    IRadioDriver* operator->() { return driver_; }
    operator bool() const { return driver_ != nullptr; }

private:
    IRadioDriver* driver_;
};

// ============================================================================
// VFO BANK
// ============================================================================
class VFOBank {
public:
    VFOBank() : activeVFO_(0), scanning_(false), scanIndex_(0) {
        vfoCount_ = 0;
        for (uint8_t i = 0; i < MAX_VFO_COUNT; ++i) {
            vfos_[i] = nullptr;
        }
    }
    
    void loadAll() {
        vfoCount_ = 0;
        uint8_t vfoIdx = 0;
        
        for (uint16_t i = 0; i < CHANNELS_GetCountMax() && vfoIdx < MAX_VFO_COUNT; ++i) {
            CHMeta meta = CHANNELS_GetMeta(i);
            bool isOurType = (TYPE_FILTER_VFO & (1 << meta.type)) != 0;
            if (!isOurType) continue;
            
            MR ch;
            CHANNELS_Load(i, &ch);
            IRadioDriver *driver = nullptr;
            
            if (ch.radio == RADIO_BK4819) driver = &bk4819_;
            if (ch.radio == RADIO_BK1080) driver = &bk1080_;
            if (ch.radio == RADIO_SI4732) driver = &si4732_;
            
            if (!driver) continue;
            
            Log("DRIVER: %u", driver);
            vfos_[vfoIdx] = driver;
            LogC(LOG_C_BG_BLUE, "[VFOBank] CH %u -> VFO %u", i, vfoIdx);
            driver->loadCh(i, &ch);
            vfoIdx++;
        }
        
        vfoCount_ = vfoIdx;
        
        // Включаем активный VFO - правильный синтаксис!
        if (vfos_[activeVFO_]) {
            (*vfos_[activeVFO_])[ParamId::PowerOn] = 1;
            (*vfos_[activeVFO_])[ParamId::RxMode] = 1;
        }
    }
    
    void saveAll() {
        for (uint8_t i = 0; i < vfoCount_; ++i) {
            if (vfos_[i] && vfos_[i]->isDirty()) {
                vfos_[i]->saveCh();
            }
        }
    }
    
    void scanTick() {
        if (!scanning_) return;
        
        IRadioDriver *driver = vfos_[scanIndex_];
        if (!driver) {
            scanNext();
            return;
        }
        
        // Правильный синтаксис для чтения параметра
        uint16_t rssi = (uint32_t)(*driver)[ParamId::RSSI];
        bool signalDetected = (rssi > SCAN_RSSI_THRESHOLD);
        
        if (signalDetected) {
            stopScan();
            switchVFO(scanIndex_);
        } else {
            scanNext();
        }
    }
    
    void startScan() {
        scanning_ = true;
        scanIndex_ = 0;
        scanNext();
    }
    
    void stopScan() { 
        scanning_ = false; 
    }
    
    bool isScanning() const { 
        return scanning_; 
    }
    
    void switchVFO(uint8_t index) {
        if (index >= vfoCount_ || !vfos_[index]) return;
        
        // Выключаем текущий VFO - правильный синтаксис!
        if (activeVFO_ < vfoCount_ && vfos_[activeVFO_]) {
            (*vfos_[activeVFO_])[ParamId::RxMode] = 0;
        }
        
        // Включаем новый VFO - правильный синтаксис!
        activeVFO_ = index;
        (*vfos_[activeVFO_])[ParamId::PowerOn] = 1;
        (*vfos_[activeVFO_])[ParamId::RxMode] = 1;
    }
    
    void setActive(uint8_t index) {
        switchVFO(index);
    }
    
    uint8_t getActiveIndex() const { 
        return activeVFO_; 
    }
    
    uint8_t getCount() const { 
        return vfoCount_; 
    }
    
    // ========================================================================
    // ACCESS - через VFOProxy
    // ========================================================================
    VFOProxy operator[](uint8_t index) {
        return VFOProxy((index < vfoCount_) ? vfos_[index] : nullptr);
    }
    
    VFOProxy active() {
        return VFOProxy((activeVFO_ < vfoCount_) ? vfos_[activeVFO_] : nullptr);
    }

private:
    static constexpr uint8_t MAX_VFO_COUNT = 8;
    static constexpr uint16_t SCAN_RSSI_THRESHOLD = 100;
    
    IRadioDriver *vfos_[MAX_VFO_COUNT];
    uint8_t activeVFO_;
    bool scanning_;
    uint8_t scanIndex_;
    uint8_t vfoCount_;
    
    BK1080Driver bk1080_;
    BK4819Driver bk4819_;
    SI4732Driver si4732_;
    
    void scanNext() {
        do {
            scanIndex_ = (scanIndex_ + 1) % vfoCount_;
        } while (!vfos_[scanIndex_] && scanIndex_ != activeVFO_);
        
        if (vfos_[scanIndex_]) {
            (*vfos_[scanIndex_])[ParamId::PowerOn] = 1;
            (*vfos_[scanIndex_])[ParamId::RxMode] = 1;
        }
    }
};

