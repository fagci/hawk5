#pragma once
#include <cstdint>
#include <functional>
#include <variant>

// ============================================================================
// CONSTRAINT (может быть константа или функция)
// ============================================================================

class Constraint {
public:
  // Конструктор для константы
  constexpr Constraint(uint32_t value) : storage_(value) {}

  // Конструктор для функции
  Constraint(std::function<uint32_t()> func) : storage_(func) {}

  constexpr Constraint() : storage_(static_cast<uint32_t>(0)) {}

  // Получить значение
  uint32_t get() const {
    if (std::holds_alternative<uint32_t>(storage_)) {
      return std::get<uint32_t>(storage_); // 🔥 Zero overhead для константы!
    }
    return std::get<std::function<uint32_t()>>(storage_)();
  }

  // Проверка, является ли константой (для оптимизации)
  bool isConstant() const { return std::holds_alternative<uint32_t>(storage_); }

private:
  std::variant<uint32_t, std::function<uint32_t()>> storage_;
};

// ============================================================================
// PARAMETER DESCRIPTOR (с Constraint вместо отдельных min/max)
// ============================================================================

template <typename ContainerType> struct ParamDescriptor {
  const char *name;

  // 🔥 Min/Max как атрибуты с доступом к контейнеру!
  Constraint min;
  Constraint max;
  uint32_t step;

  // Getter/Setter
  std::function<uint32_t()> getter;
  std::function<void(uint32_t)> setter;
  std::function<void(uint32_t)> onChange;

  // Flags
  bool isReadOnly : 1;
  bool useCache : 1;
  bool delayedSave : 1;
  bool dirty : 1;

  uint32_t cachedValue;

  // Container pointer для доступа к this[OtherParam]
  ContainerType *container;

  // ========================================================================
  // VALIDATION
  // ========================================================================

  bool validate(uint32_t &value) const {
    uint32_t minVal = min.get();
    uint32_t maxVal = max.get();

    if (value < minVal) {
      value = minVal;
      return false;
    }
    if (value > maxVal) {
      value = maxVal;
      return false;
    }
    return true;
  }
};

// ============================================================================
// PARAMETER PROXY
// ============================================================================

template <typename ContainerType> class ParameterProxy {
public:
  using Descriptor = ParamDescriptor<ContainerType>;

  ParameterProxy(Descriptor *desc) : desc_(desc) {}

  // ========================================================================
  // READ/WRITE
  // ========================================================================

  operator uint32_t() const {
    if (desc_->useCache) {
      return desc_->cachedValue;
    }
    return desc_->getter ? desc_->getter() : 0;
  }

  ParameterProxy &operator=(uint32_t value) {
    if (desc_->isReadOnly)
      return *this;

    // Validate with dynamic min/max
    desc_->validate(value);

    // Check if changed
    uint32_t oldValue = uint32_t(*this);
    if (oldValue == value) {
      return *this; // 🔥 No write if unchanged!
    }

    // Update
    if (desc_->useCache) {
      desc_->cachedValue = value;
    }

    if (desc_->setter) {
      desc_->setter(value);
    }

    if (desc_->delayedSave) {
      desc_->dirty = true;
    }

    if (desc_->onChange) {
      desc_->onChange(value);
    }

    return *this;
  }

  // ========================================================================
  // COMPOUND OPERATORS
  // ========================================================================

  ParameterProxy &operator+=(uint32_t value) {
    *this = uint32_t(*this) + value;
    return *this;
  }

  ParameterProxy &operator-=(uint32_t value) {
    *this = uint32_t(*this) - value;
    return *this;
  }

  ParameterProxy &operator++() {
    *this = uint32_t(*this) + desc_->step;
    return *this;
  }

  ParameterProxy &operator--() {
    *this = uint32_t(*this) - desc_->step;
    return *this;
  }

  // ========================================================================
  // METADATA ACCESS (как атрибуты!)
  // ========================================================================

  struct MinAttribute {
    Descriptor *desc;
    operator uint32_t() const { return desc->min.get(); }
  } min{desc_};

  struct MaxAttribute {
    Descriptor *desc;
    operator uint32_t() const { return desc->max.get(); }
  } max{desc_};

  struct StepAttribute {
    Descriptor *desc;
    operator uint32_t() const { return desc->step; }
  } step{desc_};

  const char *name() const { return desc_->name; }
  bool isDirty() const { return desc_->dirty; }
  void clearDirty() { desc_->dirty = false; }

private:
  Descriptor *desc_;
};

// ============================================================================
// PARAMETER CONTAINER
// ============================================================================

template <size_t N> class ParameterContainer {
public:
  using Descriptor = ParamDescriptor<ParameterContainer<N>>;
  using Proxy = ParameterProxy<ParameterContainer<N>>;

  ParameterContainer() {
    // Set container pointer for all descriptors
    for (size_t i = 0; i < N; ++i) {
      descriptors_[i].container = this;
    }
  }

  Proxy operator[](size_t index) { return Proxy(&descriptors_[index]); }

  const Proxy operator[](size_t index) const {
    return Proxy(const_cast<Descriptor *>(&descriptors_[index]));
  }

  void checkSave() {
    for (size_t i = 0; i < N; ++i) {
      if (descriptors_[i].dirty && descriptors_[i].delayedSave) {
        if (onSave_) {
          onSave_(i, descriptors_[i].cachedValue);
        }
        descriptors_[i].dirty = false;
      }
    }
  }

  void setOnSave(std::function<void(size_t, uint32_t)> callback) {
    onSave_ = callback;
  }

protected:
  Descriptor descriptors_[N];
  std::function<void(size_t, uint32_t)> onSave_;
};
