/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _CAM_VFE_LITE634_110_H_
#define _CAM_VFE_LITE634_110_H_
#include "cam_vfe_camif_ver3.h"
#include "cam_vfe_top_ver4.h"
#include "cam_vfe_core.h"
#include "cam_vfe_bus_ver3.h"
#include "cam_irq_controller.h"
#include "cam_vfe_lite73x.h"

static struct cam_vfe_ver4_reg_offset_diag_regs
	vfe_lite634_110_rdi_diag_regs[CAM_VFE_RDI_VER2_MAX] = {
	{
		.cfg0             = 0x000010D0,
		.cfg1             = 0x000010D4,
		.cfg2             = 0x000010D8,
	},
	{
		.cfg0             = 0x000010C4,
		.cfg1             = 0x000010C8,
		.cfg2             = 0x000010CC,
	},
	{
		.cfg0             = 0x000010B8,
		.cfg1             = 0x000010BC,
		.cfg2             = 0x000010C0,
	},
	{
		.cfg0             = 0x000010AC,
		.cfg1             = 0x000010B0,
		.cfg2             = 0x000010B4,
	},
};

static struct cam_vfe_ver4_reg_offset_diag_regs vfe_lite634_110_ipp_diag_regs = {
	.cfg0                     = 0x000010A0,
	.cfg1                     = 0x000010A4,
	.cfg2                     = 0x000010A8,
};

static struct cam_vfe_top_ver4_reg_offset_common vfe_lite634_110_top_common_reg = {
	.hw_version               = 0x00001000,
	.hw_capability            = 0x00001004,
	.lens_feature             = 0x00001008,
	.color_feature            = 0x0000100C,
	.stats_feature            = 0x00001010,
	.core_cgc_ovd_0           = 0x00001014,
	.ahb_cgc_ovd              = 0x00001018,
	.irq_set                  = 0x00001034,
	.irq_cmd                  = 0x00001038,
	.core_cfg_0               = 0x0000103C,
	.core_cfg_1               = 0x00001040,
	.core_cfg_2               = 0x00001044,
	.diag_config              = 0x00001048,
	.ipp_violation_status     = 0x0000105C,
	.stats_throttle_cfg_0     = 0x00001060,
	.bus_violation_status     = 0x00001264,
	.bus_overflow_status      = 0x00001268,
	.top_debug_cfg            = 0x00001080,
	.num_top_debug_reg        = CAM_VFE_73X_NUM_DBG_REG,
	.top_debug                = {
		0x00001064,
		0x00001068,
		0x0000106C,
		0x00001070,
		0x00001074,
		0x00001078,
		0x0000107C,
	},
	.perf_regs_0              = &vfe_lite736_perf_count_reg,
	.core_cfg_3               = 0x0000109C,
	.diag_regs_rdi            = &vfe_lite634_110_rdi_diag_regs,
	.diag_regs_ipp            = &vfe_lite634_110_ipp_diag_regs,
};

static struct cam_vfe_ver4_path_hw_info
	vfe_lite634_110_rdi_hw_info[CAM_VFE_RDI_VER2_MAX] = {
	{
		.common_reg     = &vfe_lite634_110_top_common_reg,
		.reg_data       = &vfe73x_rdi_reg_data[0],
	},
	{
		.common_reg     = &vfe_lite634_110_top_common_reg,
		.reg_data       = &vfe73x_rdi_reg_data[1],
	},
	{
		.common_reg     = &vfe_lite634_110_top_common_reg,
		.reg_data       = &vfe73x_rdi_reg_data[2],
	},
	{
		.common_reg     = &vfe_lite634_110_top_common_reg,
		.reg_data       = &vfe73x_rdi_reg_data[3],
	},
};

static struct cam_vfe_top_ver4_hw_info vfe_lite634_110_top_hw_info = {
	.common_reg = &vfe_lite634_110_top_common_reg,
	.rdi_hw_info[0] = &vfe_lite634_110_rdi_hw_info[0],
	.rdi_hw_info[1] = &vfe_lite634_110_rdi_hw_info[1],
	.rdi_hw_info[2] = &vfe_lite634_110_rdi_hw_info[2],
	.rdi_hw_info[3] = &vfe_lite634_110_rdi_hw_info[3],
	.vfe_full_hw_info = {
		.common_reg     = &vfe_lite634_110_top_common_reg,
		.reg_data       = &vfe73x_ipp_reg_data,
	},
	.ipp_module_desc        = vfe73x_ipp_mod_desc,
	.wr_client_desc         = vfe736x_wr_client_desc,
	.num_mux = 5,
	.mux_type = {
		CAM_VFE_CAMIF_VER_4_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
	},
	.debug_reg_info = &vfe73x_dbg_reg_info,
};

static struct cam_vfe_hw_info cam_vfe_lite634_110_hw_info = {
	.irq_hw_info                   = &vfe73x_irq_hw_info,

	.bus_version                   = CAM_VFE_BUS_VER_3_0,
	.bus_hw_info                   = &vfe736x_bus_hw_info,

	.top_version                   = CAM_VFE_TOP_VER_4_0,
	.top_hw_info                   = &vfe_lite634_110_top_hw_info,
};

#endif /* _CAM_VFE_LITE634_110_H_ */

