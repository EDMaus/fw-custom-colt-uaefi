# boards/uaefi_pro/board.mk

# Start from the official uaEFI/uaEFI Pro MM100 board definition.
# This pulls in Hellen MM100 defaults, USB descriptor, board id, ADC/CAN/EGT/SENT,
# communication port, and the correct common sources.
include $(BOARDS_DIR)/hellen/uaefi/board.mk

# Board-local custom source files
ifneq ($(IS_RE_BOOTLOADER),yes)
BOARDCPPSRC += $(BOARD_DIR)/colt_can.cpp
endif

# Board-local headers
BOARDINC += $(BOARD_DIR)

# GCC 12 on Windows flags false positives in third-party ChibiOS/ST code.
EXTRA_PARAMS += -Wno-error=maybe-uninitialized -Wno-error=uninitialized
