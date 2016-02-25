# Makefile for the FocalTech ft5x36 Touchscreen driver

obj-$(CONFIG_TOUCHSCREEN_FT5X36) += focaltech_ctl.o
obj-$(CONFIG_TOUCHSCREEN_FT5X36) += ft5x36_ex_fun.o
obj-$(CONFIG_TOUCHSCREEN_FT5X36) += ft5x36_ts.o
obj-$(CONFIG_TOUCHSCREEN_FT5X36) += focal_mcap_test_lib.a
obj-$(CONFIG_TOUCHSCREEN_FT5X36) += lct_ctp_selftest.o

# Make sure the prebuilt lib is included
ftlib := $(PWD)/drivers/input/touchscreen/ft5x36/focal_mcap_test_lib.a
ldflags-$(CONFIG_TOUCHSCREEN_FT5X36) += --whole-archive $(ftlib) --no-whole-archive
