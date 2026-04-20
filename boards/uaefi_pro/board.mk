# boards/uaefi_pro/board.mk

# Start from the official uaEFI/uaEFI Pro MM100 board definition.
# This pulls in Hellen MM100 defaults, USB descriptor, board id, ADC/CAN/EGT/SENT,
# communication port, and the correct common sources.
include $(BOARDS_DIR)/hellen/uaefi/board.mk

# Board-local custom source files
BOARDCPPSRC += $(BOARD_DIR)/colt_can.cpp

# Board-local headers
BOARDINC += $(BOARD_DIR)
