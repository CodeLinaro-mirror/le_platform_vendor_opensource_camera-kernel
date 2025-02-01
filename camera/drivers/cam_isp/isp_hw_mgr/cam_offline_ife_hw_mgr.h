/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CAM_OFFLINE_IFE_HW_MGR_H_
#define _CAM_OFFLINE_IFE_HW_MGR_H_


#define CAM_IFE_HW_STATE_STOPPED  0
#define CAM_IFE_HW_STATE_STARTING 1
#define CAM_IFE_HW_STATE_STARTED  2
#define CAM_IFE_HW_STATE_BUSY     3
#define CAM_IFE_HW_STATE_STOPPING 4

int cam_offline_ife_mgr_free_in_proc_req(struct cam_ife_hw_mgr *ife_hw_mgr,
	uint32_t hw_id);
unsigned int cam_offline_ife_mgr_flush_in_queue(struct cam_ife_hw_mgr *ife_hw_mgr,
	uint32_t ctx_idx,
	bool just_incomplete,
	int64_t req_id);
int cam_offline_ife_mgr_enqueue_offline_update(void *hw_mgr_priv,
	void *prepare_hw_update_args);
int cam_offline_ife_mgr_enqueue_offline_config(void *hw_mgr_priv,
	void *config_hw_args);
int cam_offline_ife_mgr_required_hw(void *hw_mgr_priv, bool stop);
int cam_offline_ife_mgr_check_start_processing(void *hw_mgr_priv,
	struct cam_ife_hw_mgr_ctx *hw_mgr_ctx);
#endif /* _CAM_OFFLINE_IFE_HW_MGR_H_*/
