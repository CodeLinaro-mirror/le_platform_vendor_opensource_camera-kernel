/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _CAM_VFE634_110_H_
#define _CAM_VFE634_110_H_
#include "cam_vfe_top_ver4.h"
#include "cam_vfe_core.h"
#include "cam_vfe_bus_ver3.h"
#include "cam_irq_controller.h"
#include "cam_vfe736.h"

static struct cam_vfe_ver4_reg_offset_diag_regs
	vfe634_110_rdi_diag_regs[CAM_VFE_RDI_VER2_MAX] = {
	{
		.cfg0             = 0x0000018C,
		.cfg1             = 0x00000190,
		.cfg2             = 0x00000194,
	},
	{
		.cfg0             = 0x00000180,
		.cfg1             = 0x00000184,
		.cfg2             = 0x00000188,
	},
	{
		.cfg0             = 0x00000174,
		.cfg1             = 0x00000178,
		.cfg2             = 0x0000017C,
	},
};

static struct cam_vfe_ver4_reg_offset_diag_regs vfe634_110_ipp_diag_regs = {
	.cfg0                     = 0x00000198,
	.cfg1                     = 0x0000019C,
	.cfg2                     = 0x000001A0,
};


static struct cam_vfe_ver4_reg_offset_diag_regs vfe634_110_pdaf_diag_regs = {
	.cfg0                     = 0x00000168,
	.cfg1                     = 0x0000016C,
	.cfg2                     = 0x00000170,
};

static struct cam_vfe_top_ver4_reg_offset_common vfe634_110_top_common_reg = {
	.hw_version               = 0x00000000,
	.hw_capability            = 0x00000004,
	.lens_feature             = 0x00000008,
	.stats_feature            = 0x0000000C,
	.color_feature            = 0x00000010,
	.zoom_feature             = 0x00000014,
	.core_cgc_ovd_0           = 0x00000018,
	.core_cgc_ovd_1           = 0x0000001C,
	.ahb_cgc_ovd              = 0x00000020,
	.core_cfg_0               = 0x00000024,
	.core_cfg_1               = 0x00000028,
	.core_cfg_2               = 0x0000002C,
	.global_reset_cmd         = 0x00000030,
	.irq_set                  = 0x0000004C,
	.diag_config              = 0x00000050,
	.ipp_violation_status     = 0x00000064,
	.pdaf_violation_status    = 0x00000404,
	.core_cfg_3               = 0x00000068,
	.stats_throttle_cfg_0     = 0x00000070,
	.stats_throttle_cfg_1     = 0x00000074,
	.stats_throttle_cfg_2     = 0x00000078,
	.core_cfg_4               = 0x00000080,
	.core_cfg_5               = 0x00000084,
	.core_cfg_6               = 0x00000088,
	.period_cfg               = 0x0000008C,
	.irq_sub_pattern_cfg      = 0x00000090,
	.epoch0_pattern_cfg       = 0x00000094,
	.epoch1_pattern_cfg       = 0x00000098,
	.epoch_height_cfg         = 0x0000009C,
	.bus_violation_status     = 0x00000C64,
	.bus_overflow_status      = 0x00000C68,
	.top_debug_cfg            = 0x000000FC,
	.num_top_debug_reg        = CAM_VFE_736_NUM_DBG_REG,
	.top_debug = {
		0x000000A0,
		0x000000A4,
		0x000000A8,
		0x000000AC,
		0x000000B0,
		0x000000B4,
		0x000000B8,
		0x000000BC,
		0x000000C0,
		0x000000C4,
		0x000000C8,
		0x000000CC,
		0x000000D0,
		0x000000D4,
		0x000000D8,
		0x000000DC,
		0x000000E0,
		0x000000E4,
	},
	.perf_regs_0             = &vfe736_perf_count_reg_0,
	.perf_regs_1             = &vfe736_perf_count_reg_1,
	.perf_regs_2             = &vfe736_perf_count_reg_2,
	.core_cfg_7              = 0x00000130,
	.noise_processing        = 0x00000140,
	.diag_regs_pdaf          = &vfe634_110_pdaf_diag_regs,
	.diag_regs_rdi           = &vfe634_110_rdi_diag_regs,
	.diag_regs_ipp           = &vfe634_110_ipp_diag_regs,
};

struct cam_vfe_ver4_path_hw_info
	vfe634_110_rdi_hw_info_arr[CAM_VFE_RDI_VER2_MAX] = {
	{
		.common_reg     = &vfe634_110_top_common_reg,
		.reg_data       = &vfe736_vfe_full_rdi_reg_data[0],
	},
	{
		.common_reg     = &vfe634_110_top_common_reg,
		.reg_data       = &vfe736_vfe_full_rdi_reg_data[1],
	},
	{
		.common_reg     = &vfe634_110_top_common_reg,
		.reg_data       = &vfe736_vfe_full_rdi_reg_data[2],
	},
};

static struct cam_vfe_top_ver4_hw_info vfe634_110_top_hw_info = {
	.common_reg     = &vfe634_110_top_common_reg,
	.rdi_hw_info[0] = &vfe634_110_rdi_hw_info_arr[0],
	.rdi_hw_info[1] = &vfe634_110_rdi_hw_info_arr[1],
	.rdi_hw_info[2] = &vfe634_110_rdi_hw_info_arr[2],
	.vfe_full_hw_info = {
		.common_reg     = &vfe634_110_top_common_reg,
		.reg_data       = &vfe736_pp_common_reg_data,
	},
	.pdlib_hw_info = {
		.common_reg     = &vfe634_110_top_common_reg,
		.reg_data       = &vfe736_pdlib_reg_data,
	},
	.ipp_module_desc        = vfe736_ipp_mod_desc,
	.wr_client_desc         = vfe736_wr_client_desc,
	.num_mux = 5,
	.mux_type = {
		CAM_VFE_CAMIF_VER_4_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_PDLIB_VER_1_0,
	},
	.num_path_port_map = 2,
	.path_port_map = {
		{CAM_ISP_HW_VFE_IN_PDLIB, CAM_ISP_IFE_OUT_RES_2PD},
		{CAM_ISP_HW_VFE_IN_PDLIB, CAM_ISP_IFE_OUT_RES_PREPROCESS_2PD}
	},
	.num_top_errors                  = ARRAY_SIZE(vfe736_top_irq_err_desc),
	.top_err_desc                    = vfe736_top_irq_err_desc,
	.num_pdaf_violation_errors       = ARRAY_SIZE(vfe736_pdaf_violation_desc),
	.pdaf_violation_desc             = vfe736_pdaf_violation_desc,
	.debug_reg_info                  = &vfe736_dbg_reg_info,
};

static struct cam_vfe_hw_info cam_vfe634_110_hw_info = {
	.irq_hw_info                   = &vfe736_irq_hw_info,

	.bus_version                   = CAM_VFE_BUS_VER_3_0,
	.bus_hw_info                   = &vfe736_bus_hw_info,

	.top_version                   = CAM_VFE_TOP_VER_4_0,
	.top_hw_info                   = &vfe634_110_top_hw_info,
};

#endif /* _CAM_VFE634_110_H_ */
