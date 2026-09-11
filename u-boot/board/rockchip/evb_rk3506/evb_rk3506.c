/*
 * SPDX-License-Identifier:     GPL-2.0+
 *
 * (C) Copyright 2024 Rockchip Electronics Co., Ltd
 */

#include <common.h>
#include <dwc3-uboot.h>
#include <usb.h>

#include <dm.h>

int rk_board_late_init(void)
{
	struct udevice *lt8912b_dev;
	u32 ret = 0;

	ret = uclass_get_device_by_name(UCLASS_I2C_GENERIC, "lt8912b", &lt8912b_dev);
	if(ret) {
		printf("%s: Cannot get lt8912b:%d\n", __func__, ret);
	}

	return 0;
}
