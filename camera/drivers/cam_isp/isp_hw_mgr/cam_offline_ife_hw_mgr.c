// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>


#include "cam_isp_hw_mgr_intf.h"
#include "cam_isp_hw.h"
#include "cam_vfe_hw_intf.h"
#include "cam_sfe_hw_intf.h"
#include "cam_isp_packet_parser.h"
#include "cam_ife_hw_mgr.h"
#include "cam_offline_ife_hw_mgr.h"
#include "cam_packet_util.h"


int cam_offline_ife_mgr_free_in_proc_req(struct cam_ife_hw_mgr *ife_hw_mgr,
					uint32_t hw_id)
{
	struct cam_ife_mgr_offline_in_queue *c_elem;
	struct cam_ife_mgr_offline_in_queue *c_elem_temp;

	list_for_each_entry_safe(c_elem, c_elem_temp,
			&ife_hw_mgr->in_proc_queue.list, list) {
		if (c_elem->hw_id == hw_id) {
			if (c_elem->request_id == 1 &&
					ife_hw_mgr->starting_offline_cnt) {
				ife_hw_mgr->starting_offline_cnt--;
			}
			list_del_init(&c_elem->list);
			kfree(c_elem);
		}
	}
	return 0;
}

unsigned int cam_offline_ife_mgr_flush_in_queue(
	struct cam_ife_hw_mgr *ife_hw_mgr,
	uint32_t ctx_idx,
	bool just_incomplete,
	int64_t req_id)
{
	struct cam_ife_mgr_offline_in_queue *c_elem;
	struct cam_ife_mgr_offline_in_queue *c_elem_temp;
	int left = 0;

	list_for_each_entry_safe(c_elem, c_elem_temp,
			&ife_hw_mgr->input_queue.list, list) {
		if (req_id >= 0 && req_id != c_elem->request_id)
			continue;
		if (c_elem->ctx_idx == ctx_idx) {
			if (!(c_elem->ready && just_incomplete)) {
				CAM_DBG(CAM_ISP, "flush req %d %llu",
					c_elem->ctx_idx, c_elem->request_id);
				c_elem->prepare.packet = NULL;
				list_del_init(&c_elem->list);
				kfree(c_elem);
			} else {
				left++;
				CAM_DBG(CAM_ISP, "leave req %d %llu",
					c_elem->ctx_idx, c_elem->request_id);
			}
		}
	}
	return left;
}

int cam_offline_ife_mgr_enqueue_offline_update(void *hw_mgr_priv,
	void *prepare_hw_update_args)
{
	struct cam_ife_hw_mgr *ife_hw_mgr        = hw_mgr_priv;
	struct cam_hw_prepare_update_args *prepare =
		(struct cam_hw_prepare_update_args *) prepare_hw_update_args;
	struct cam_ife_hw_mgr_ctx *hw_mgr_ctx =
		(struct cam_ife_hw_mgr_ctx *) prepare->ctxt_to_hw_map;
	struct cam_ife_mgr_offline_in_queue *c_elem;

	c_elem = kzalloc(sizeof(struct cam_ife_mgr_offline_in_queue),
			GFP_KERNEL);

	memcpy(&c_elem->prepare, prepare,
		sizeof(struct cam_hw_prepare_update_args));
	c_elem->request_id = prepare->packet->header.request_id;
	c_elem->ctx_idx = hw_mgr_ctx->ctx_idx;
	c_elem->ready = false;
	INIT_LIST_HEAD(&c_elem->list);

	mutex_lock(&ife_hw_mgr->ctx_mutex);
	list_add_tail(&c_elem->list, &ife_hw_mgr->input_queue.list);
	mutex_unlock(&ife_hw_mgr->ctx_mutex);
	return 0;
}

int cam_offline_ife_mgr_enqueue_offline_config(void *hw_mgr_priv,
	void *config_hw_args)
{
	int rc = -EINVAL;
	int found = 0;
	struct cam_ife_mgr_offline_in_queue *c_elem, *print_elem;
	struct cam_ife_hw_mgr         *ife_hw_mgr = hw_mgr_priv;
	struct cam_hw_config_args     *cfg =
			(struct cam_hw_config_args *)config_hw_args;
	struct cam_ife_hw_mgr_ctx     *hw_mgr_ctx =
			(struct cam_ife_hw_mgr_ctx *) cfg->ctxt_to_hw_map;

	mutex_lock(&ife_hw_mgr->ctx_mutex);
	list_for_each_entry(c_elem, &ife_hw_mgr->input_queue.list, list) {
		if (c_elem->prepare.packet->header.request_id != cfg->request_id ||
			c_elem->ctx_idx != hw_mgr_ctx->ctx_idx) {
			CAM_DBG(CAM_ISP, "#INVALID# input queue# c_elem %p %p %llu %d cfg %llu %d",
				c_elem, hw_mgr_ctx->concr_ctx,c_elem->prepare.packet->header.request_id,
				c_elem->ctx_idx,cfg->request_id,hw_mgr_ctx->ctx_idx);
			continue;
		}
		rc = 0;
		memcpy(&c_elem->cfg, cfg,
				sizeof(struct cam_hw_config_args));
		c_elem->ready = true;
		found =1;
		break;
	}

	//add log to scan input queue list
	if (found == 0) {
		list_for_each_entry(print_elem, &ife_hw_mgr->input_queue.list, list) {
			CAM_DBG(CAM_ISP, "#PRINT QUEUE# ife mgr ctx %p c_elem %p req id %llu ctx id %d",
				hw_mgr_ctx, print_elem, c_elem->prepare.packet->header.request_id,c_elem->ctx_idx);
		}
	}
	mutex_unlock(&ife_hw_mgr->ctx_mutex);
	return rc;
}

int cam_offline_ife_mgr_check_start_processing(void *hw_mgr_priv,
		struct cam_ife_hw_mgr_ctx *hw_mgr_ctx)
{
	struct cam_ife_mgr_offline_in_queue   *c_elem;
	struct cam_ife_mgr_offline_in_queue   *c_elem_temp;
	struct cam_ife_hw_mgr_ctx             *run_hw_mgr_ctx;
	struct cam_ife_hw_mgr                 *ife_hw_mgr        = hw_mgr_priv;
	struct cam_ife_hw_concrete_ctx        *c_ctx = NULL;
	int i, rc = 0;
	uint32_t state;
	bool found = false;
	bool is_init_pkt;


	mutex_lock(&ife_hw_mgr->ctx_mutex);

	if (list_empty(&ife_hw_mgr->used_ctx_list)) {
		CAM_INFO(CAM_ISP, "Currently no ctx in use");
		mutex_unlock(&ife_hw_mgr->ctx_mutex);
		return rc;
	}

	list_for_each_entry(c_ctx, &ife_hw_mgr->used_ctx_list, list) {
		state = atomic_read(&c_ctx->ctx_state);
		if (!c_ctx->flags.is_offline ||
			(state != CAM_IFE_HW_STATE_STARTED &&
			state != CAM_IFE_HW_STATE_STARTING)) {
			CAM_DBG(CAM_ISP, " #REJECT# FAILED to get ife, current concr_ctx : ctx id %p : %d offline %d acquired_hw_id %d state %d",
				c_ctx, c_ctx->ctx_index,
				c_ctx->flags.is_offline,
				c_ctx->acquired_hw_id, state);
			continue;
		}

		cam_offline_ife_mgr_free_in_proc_req(ife_hw_mgr,
						c_ctx->acquired_hw_id);

		found = false;
		list_for_each_entry_safe(c_elem, c_elem_temp,
				&ife_hw_mgr->input_queue.list, list) {
			is_init_pkt =
				((c_elem->prepare.packet->header.op_code + 1) &
					0xF) == CAM_ISP_PACKET_INIT_DEV;
			if (c_ctx->waiting_start &&
				c_elem->ctx_idx != c_ctx->start_ctx_idx) {
				CAM_DBG(CAM_ISP,"#REJECT#: WAITING start %d input queue elem ctx idx %d start cxt idx %d req id %llu",
					c_ctx->waiting_start,
					c_elem->ctx_idx,
					c_ctx->start_ctx_idx,
					c_elem->request_id);
				continue;
			}
			if (!c_elem->ready) {
				CAM_DBG(CAM_ISP,"#REJECT#: input queue elem ctx idx %d req id %llu NOT ready",
						c_elem->ctx_idx, c_elem->request_id);
				continue;
			}
			if (ife_hw_mgr->starting_offline_cnt == 0 &&
				c_elem->request_id == 0) {
				c_elem->hw_id = c_ctx->acquired_hw_id;
				list_del_init(&c_elem->list);
				list_add_tail(&c_elem->list,
					&ife_hw_mgr->in_proc_queue.list);
				CAM_DBG(CAM_ISP,
					"Skip init config for already started HW");
				continue;
			}
			if (c_elem->request_id !=
				c_elem->prepare.packet->header.request_id)
				CAM_ERR(CAM_ISP,
					"Request id mismatch. Packet recycled before use!");

			run_hw_mgr_ctx =
				&ife_hw_mgr->virt_ctx_pool[c_elem->ctx_idx];
			if (!run_hw_mgr_ctx->ctx_in_use)
				CAM_ERR(CAM_ISP, "UNUSED CONTEXT %d",
						c_elem->ctx_idx);
			run_hw_mgr_ctx->concr_ctx = c_ctx;
			c_elem->prepare.ctxt_to_hw_map = run_hw_mgr_ctx;
			c_elem->cfg.ctxt_to_hw_map = run_hw_mgr_ctx;
			found = true;
			list_del_init(&c_elem->list);
			c_elem->hw_id = c_ctx->acquired_hw_id;
			list_add_tail(&c_elem->list,
				&ife_hw_mgr->in_proc_queue.list);
			break;
		}
		CAM_DBG(CAM_ISP," found %d offline inqueue elem %p ctx %d",
					found, c_elem, c_elem->ctx_idx);
		if (found) {
			/* For the zero request we do not get event,
			 * so restore state
			 */
			if (c_elem->cfg.request_id != 0) {
				c_ctx->waiting_start = false;
				atomic_set(&c_ctx->ctx_state,
						CAM_IFE_HW_STATE_BUSY);
			} else {
				c_ctx->waiting_start = true;
				c_ctx->start_ctx_idx = c_elem->ctx_idx;
			}

			c_ctx->served_ctx_w = 1 - c_ctx->served_ctx_w;
			c_ctx->served_ctx_id[c_ctx->served_ctx_w] =
							run_hw_mgr_ctx->ctx_idx;
			rc = cam_ife_mgr_prepare_hw_update(hw_mgr_priv,
					&c_elem->prepare);
			c_elem->cfg.num_hw_update_entries =
				c_elem->prepare.num_hw_update_entries;
			rc = cam_ife_mgr_config_hw(hw_mgr_priv, &c_elem->cfg);
		}
	}

	mutex_unlock(&ife_hw_mgr->ctx_mutex);
	return rc;
}

int cam_offline_ife_mgr_required_hw(void *hw_mgr_priv, bool stop)
{
	struct cam_ife_hw_mgr          *ife_hw_mgr = hw_mgr_priv;
	int i, cnt, req_hw;

	cnt = 0;
	for (i = 0; i < CAM_CTX_MAX; i++) {
		if (ife_hw_mgr->virt_ctx_pool[i].ctx_in_use &&
				ife_hw_mgr->virt_ctx_pool[i].is_offline)
			cnt++;
	}

	/* If only one context presents - we need to stop all HW*/
	if ((stop) && (cnt == 1))
		return 0;

	req_hw = (cnt + 1) / 2;
	CAM_DBG(CAM_ISP, "Currently needed %d IFEs for offline processing",
			req_hw);
	return req_hw > CAM_MAX_OFFLINE_HW ?
		CAM_MAX_OFFLINE_HW : req_hw;
}
