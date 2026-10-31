/*
 * File      : sample_vlcview.c
 * This file is part of SOCs BSP for RT-Thread distribution.
 *
 * Copyright (c) 2017 Shanghai Fullhan Microelectronics Co., Ltd.
 * All rights reserved
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *  Visit http://www.fullhan.com to get contact with Fullhan.
 *
 * Change Logs:
 * Date           Author       Notes
 */

// Chip Include
#include "sample_common.h"
#include "vlcview.h"

static FH_SINT32 g_sample_running = 0;
static char *model_name = "VlcView";
#ifndef FH_FAST_BOOT
FH_SINT32 _vlcview_exit(FH_VOID)
{
	FH_SINT32 ret = FH_SDK_SUCCESS;
	SDK_PRT(model_name, "Vlcview Exit!\n");

	if (!g_sample_running)
	{
		SDK_PRT(model_name, "Vlcview Isn't Running!\n");
		return ret;
	}

#ifdef FH_APP_USING_COOLVIEW
	ret = sample_common_stop_coolview();
	SDK_FUNC_ERROR(model_name, ret);
	SDK_PRT(model_name, "sample_common_stop_coolview\n");
#endif

	ret = sample_common_stop_get_stream();
	SDK_FUNC_ERROR(model_name, ret);
	SDK_PRT(model_name, "sample_common_stop_get_stream\n");

	ret = sample_common_stop_demo();
	SDK_FUNC_ERROR(model_name, ret);
	SDK_PRT(model_name, "sample_common_stop_demo\n");

	ret = sample_common_isp_exit();
	SDK_FUNC_ERROR(model_name, ret);
	SDK_PRT(model_name, "sample_common_isp_exit\n");

	ret = sample_common_stop_send_stream();
	SDK_FUNC_ERROR(model_name, ret);
	SDK_PRT(model_name, "sample_common_stop_send_stream\n");

	ret = sample_common_dmc_deinit();
	SDK_FUNC_ERROR(model_name, ret);
	SDK_PRT(model_name, "sample_common_dmc_deinit\n");

	ret = sample_common_mjpeg_stop();
	SDK_FUNC_ERROR(model_name, ret);
	SDK_PRT(model_name, "sample_common_mjpeg_stop\n");

	ret = sample_common_dsp_exit();
	SDK_FUNC_ERROR(model_name, ret);
	SDK_PRT(model_name, "sample_common_dsp_exit\n");

	ret = sample_common_dsp_system_exit();
	SDK_FUNC_ERROR(model_name, ret);
	SDK_PRT(model_name, "sample_common_dsp_system_exit\n");
	g_sample_running = 0;

	return ret;
}

FH_SINT32 _vlcview(FH_CHAR *dst_ip, FH_UINT32 port)
{
	FH_SINT32 ret = FH_SDK_SUCCESS;
	SDK_PRT(model_name, "Vlcview Start!\n");

	if (g_sample_running)
	{
		SDK_PRT(model_name, "Vlcview Is Running!\n");
		return 0;
	}

	g_sample_running = 1;

	/******************************************
	  step  1: init media platform
	******************************************/
	ret = sample_common_dsp_system_init();
	SDK_FUNC_ERROR_GOTO(model_name, ret);
	/******************************************
	  step  2: init dsp vpu, enc, bgm and vpu bind enc
	******************************************/
	ret = sample_common_dsp_init();
	SDK_FUNC_ERROR_GOTO(model_name, ret);
	/******************************************
	 step  3: init mjpeg
	******************************************/
	ret = sample_common_mjpeg_start();
	SDK_FUNC_ERROR_GOTO(model_name, ret);
	/******************************************
	  step  4: init data-manage-center which is used to dispatch stream...
	******************************************/
	ret = sample_common_dmc_init(dst_ip, port);
	SDK_FUNC_ERROR_GOTO(model_name, ret);
	/******************************************
	  step  5: send yuv stream, then send to data manager..
	******************************************/
	ret = sample_common_start_send_stream();
	SDK_FUNC_ERROR_GOTO(model_name, ret);
	/******************************************
	  step  6: isp init and vicap bind isp, isp bind vpu
	******************************************/
	ret = sample_common_isp_init();
	SDK_FUNC_ERROR_GOTO(model_name, ret);
	/******************************************
	  step  7: isp send stream
	******************************************/
	ret = sample_common_isp_start();
	SDK_FUNC_ERROR_GOTO(model_name, ret);
	/******************************************
	  step  8: start sample_demo
	 ******************************************/
	ret = sample_common_start_demo();
	SDK_FUNC_ERROR_GOTO(model_name, ret);
	/******************************************
	  step  9: start get stream
	******************************************/
	ret = sample_common_start_get_stream();
	SDK_FUNC_ERROR_GOTO(model_name, ret);

	/******************************************
	  step  10: open coolview
	 ******************************************/
#ifdef FH_APP_USING_COOLVIEW
	ret = sample_common_start_coolview();
	SDK_FUNC_ERROR_GOTO(model_name, ret);
#endif

	return ret;

Exit:
	_vlcview_exit();
	return FH_SDK_FAILED;
}
#else
FH_SINT32 _vlcview_exit(FH_VOID)
{
	FH_SINT32 ret = FH_SDK_SUCCESS;
	SDK_PRT(model_name, "Vlcview Exit!\n");

	if (!g_sample_running)
	{
		SDK_PRT(model_name, "Vlcview Isn't Running!\n");
		return ret;
	}

#ifdef FH_APP_USING_COOLVIEW
	ret = sample_common_stop_coolview();
	SDK_FUNC_ERROR(model_name, ret);
	SDK_PRT(model_name, "sample_common_stop_coolview\n");
#endif

	ret = sample_common_stop_get_stream();
	SDK_FUNC_ERROR(model_name, ret);
	SDK_PRT(model_name, "sample_common_stop_get_stream\n");

	ret = sample_common_stop_send_stream();
	SDK_FUNC_ERROR(model_name, ret);
	SDK_PRT(model_name, "sample_common_stop_send_stream\n");

	ret = sample_common_dmc_deinit();
	SDK_FUNC_ERROR(model_name, ret);
	SDK_PRT(model_name, "sample_common_dmc_deinit\n");\
	g_sample_running = 0;
	return ret;
}

FH_SINT32 _vlcview(FH_CHAR *dst_ip, FH_UINT32 port)
{
	FH_SINT32 ret = FH_SDK_SUCCESS;
	SDK_PRT(model_name, "Vlcview Start!\n");

	if (g_sample_running)
	{
		SDK_PRT(model_name, "Vlcview Is Running!\n");
		return 0;
	}

	g_sample_running = 1;

	ret = sample_common_dmc_init(dst_ip, port);
	SDK_FUNC_ERROR_GOTO(model_name, ret);

	ret = sample_common_start_send_stream();
	SDK_FUNC_ERROR_GOTO(model_name, ret);

	ret = sample_common_start_get_stream();
	SDK_FUNC_ERROR_GOTO(model_name, ret);

#ifdef FH_APP_USING_COOLVIEW
	ret = sample_common_start_coolview();
	SDK_FUNC_ERROR_GOTO(model_name, ret);
#endif

	return ret;

Exit:
	_vlcview_exit();
	return FH_SDK_FAILED;
}
#endif
