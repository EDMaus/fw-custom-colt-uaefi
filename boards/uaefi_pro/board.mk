# boards/uaefi_pro/board.mk

# Board-local source files
BOARDCPPSRC += $(BOARD_DIR)/board_configuration.cpp
BOARDCPPSRC += $(BOARD_DIR)/colt_can.cpp
BOARDCPPSRC += \
    $(BOARD_DIR)/../../ext/rusefi/firmware/config/boards/hellen/hellen_common.cpp

# Board include path
BOARDINC += $(BOARD_DIR)