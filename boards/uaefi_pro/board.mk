# boards/colt_uaefi/board.mk

# Board-local source files
BOARDCPPSRC += $(BOARD_DIR)/board_configuration.cpp
BOARDCPPSRC += $(BOARD_DIR)/colt_can.cpp

# Board include path
BOARDINC += $(BOARD_DIR)