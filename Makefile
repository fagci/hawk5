# =============================================================================
# Directory Structure
# =============================================================================
SRC_DIR       := src
OBJ_DIR       := obj
BIN_DIR       := bin

# =============================================================================
# Project Configuration
# =============================================================================
PROJECT_NAME  := firmware
TARGET        := $(BIN_DIR)/$(PROJECT_NAME)
GIT_HASH      := $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")
BUILD_TIME    := $(shell date -u +'%Y-%m-%d_%H:%M_UTC')
BUILD_TAG     := $(shell date -u +'%Y%m%d_%H%M')

# =============================================================================
# Source Files
# =============================================================================
SRC := $(wildcard $(SRC_DIR)/*.c) \
       $(wildcard $(SRC_DIR)/driver/*.c) \
       $(wildcard $(SRC_DIR)/helper/*.c) \
       $(wildcard $(SRC_DIR)/ui/*.c) \
       $(wildcard $(SRC_DIR)/apps/*.c)

OBJS := $(OBJ_DIR)/start.o \
        $(OBJ_DIR)/init.o \
        $(OBJ_DIR)/external/printf/printf.o \
        $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# =============================================================================
# BSP Configuration
# =============================================================================
BSP_DEFINITIONS := $(wildcard hardware/*/*.def)
BSP_HEADERS     := $(patsubst hardware/%,inc/%,$(BSP_DEFINITIONS:.def=.h))

# =============================================================================
# Toolchain
# =============================================================================
TOOLCHAIN_PREFIX := arm-none-eabi-
AS       := $(TOOLCHAIN_PREFIX)gcc
CC       := $(TOOLCHAIN_PREFIX)gcc
LD       := $(TOOLCHAIN_PREFIX)gcc
OBJCOPY  := $(TOOLCHAIN_PREFIX)objcopy
SIZE     := $(TOOLCHAIN_PREFIX)size

# =============================================================================
# Compiler Flags
# =============================================================================
# Common flags for AS and CC
COMMON_FLAGS := -mcpu=cortex-m0 -mthumb -mabi=aapcs
OPTIMIZATION := -Os -flto=auto -ffunction-sections -fdata-sections

# Assembler flags
ASFLAGS  := $(COMMON_FLAGS) -c

# Compiler flags
CFLAGS   := $(COMMON_FLAGS) $(OPTIMIZATION) \
            -std=c2x \
            -Wall -Wextra -Wpedantic \
            -Wno-missing-field-initializers \
            -Wno-incompatible-pointer-types \
            -Wno-unused-function -Wno-unused-variable \
            -fno-builtin -fshort-enums \
            -fno-delete-null-pointer-checks \
            -fsingle-precision-constant \
            -finline-functions-called-once \
            -MMD -MP

# Debug/Release specific flags
DEBUG_FLAGS   := -g3 -DDEBUG -Og
RELEASE_FLAGS := -g0 -DNDEBUG

# Defines
DEFINES  := -DPRINTF_INCLUDE_CONFIG_H \
            -DGIT_HASH=\"$(GIT_HASH)\" \
            -DTIME_STAMP=\"$(BUILD_TIME)\" \
            -DCMSIS_device_header=\"ARMCM0.h\" \
            -DARMCM0

# Include paths
INC_DIRS := -I./src/config \
            -I./src/external/CMSIS_5/CMSIS/Core/Include \
            -I./src/external/CMSIS_5/Device/ARM/ARMCM0/Include

# =============================================================================
# Linker Flags
# =============================================================================
LDFLAGS  := $(COMMON_FLAGS) $(OPTIMIZATION) \
            -nostartfiles \
            -Tfirmware.ld \
            --specs=nano.specs \
            -lc -lnosys -lm \
            -Wl,--gc-sections \
            -Wl,--build-id=none \
            -Wl,--print-memory-usage \
            -Wl,-Map=$(OBJ_DIR)/output.map

# =============================================================================
# Build Configuration
# =============================================================================
# По умолчанию release сборка
BUILD_TYPE ?= release

ifeq ($(BUILD_TYPE),debug)
    CFLAGS += $(DEBUG_FLAGS)
    OPTIMIZATION := -Og
else
    CFLAGS += $(RELEASE_FLAGS)
endif

# =============================================================================
# Build Rules
# =============================================================================
.PHONY: all debug release clean help info flash

# Основная цель
all: $(TARGET).bin
	@echo "Build completed: $(TARGET).bin"

# Debug сборка
debug:
	@$(MAKE) BUILD_TYPE=debug all

# Release сборка с копированием
release: clean
	@$(MAKE) BUILD_TYPE=release all
	@cp $(TARGET).packed.bin $(BIN_DIR)/Hawk5-alfa-by-fagci-$(BUILD_TAG).bin
	@echo "Release firmware: $(BIN_DIR)/Hawk5-alfa-by-fagci-$(BUILD_TAG).bin"

# Генерация бинарного файла
$(TARGET).bin: $(TARGET)
	@echo "Creating binary file..."
	$(OBJCOPY) -O binary $< $@
	@if [ -f fw-pack.py ]; then \
		python3 fw-pack.py $@ $(GIT_HASH) $(TARGET).packed.bin; \
	else \
		echo "Warning: fw-pack.py not found, skipping packing"; \
		cp $@ $(TARGET).packed.bin; \
	fi

# Линковка
$(TARGET): $(OBJS) | $(BIN_DIR)
	@echo "Linking..."
	$(LD) $(LDFLAGS) $^ -o $@
	@echo ""
	$(SIZE) $@
	arm-none-eabi-nm --size-sort -r $(BIN_DIR)/$(PROJECT_NAME) | head -20
	@echo ""

# Компиляция C файлов
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(BSP_HEADERS) $(OBJ_DIR)
	@mkdir -p $(@D)
	@echo "CC $<"
	@$(CC) $(CFLAGS) $(DEFINES) $(INC_DIRS) -c $< -o $@

# Компиляция ассемблерных файлов
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.S | $(OBJ_DIR)
	@mkdir -p $(@D)
	@echo "AS $<"
	@$(AS) $(ASFLAGS) $< -o $@

# Генерация BSP заголовков
inc/%/%.h: hardware/%/%.def
	@mkdir -p $(@D)
	@echo "Generating BSP header: $@"
	# TODO: Add your header generation command here
	@touch $@

# Создание директорий
$(BIN_DIR) $(OBJ_DIR):
	@mkdir -p $@

# =============================================================================
# Utility Targets
# =============================================================================

# Показать информацию о сборке
info:
	@echo "Project Information:"
	@echo "  Project:     $(PROJECT_NAME)"
	@echo "  Git Hash:    $(GIT_HASH)"
	@echo "  Build Time:  $(BUILD_TIME)"
	@echo "  Build Type:  $(BUILD_TYPE)"
	@echo ""
	@echo "Toolchain:"
	@echo "  CC:          $(CC)"
	@echo "  LD:          $(LD)"
	@echo ""
	@echo "Source Files: $(words $(SRC)) files"
	@echo "Object Files: $(words $(OBJS)) files"

# Очистка
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(TARGET) $(TARGET).* $(OBJ_DIR) $(BIN_DIR)/*.bin inc/
	@echo "Clean completed"

# Очистка всего включая зависимости
distclean: clean
	@echo "Deep cleaning..."
	@rm -rf $(BIN_DIR)
	@echo "Distclean completed"

# Помощь
help:
	@echo "Available targets:"
	@echo "  all      - Build firmware (default: release mode)"
	@echo "  debug    - Build firmware with debug symbols"
	@echo "  release  - Build release firmware with timestamp"
	@echo "  clean    - Remove build artifacts"
	@echo "  distclean- Remove all generated files"
	@echo "  info     - Show build configuration"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make              # Build release version"
	@echo "  make debug        # Build debug version"
	@echo "  make release      # Build and package release"
	@echo "  make BUILD_TYPE=debug  # Alternative debug build"

# =============================================================================
# Dependencies
# =============================================================================
DEPS := $(OBJS:.o=.d)
-include $(DEPS)
