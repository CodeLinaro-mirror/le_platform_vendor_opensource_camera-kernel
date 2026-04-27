// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "cam_cci_dev.h"
#include "cam_req_mgr_dev.h"
#include "cam_cci_soc.h"
#include "cam_cci_core.h"
#include "camera_main.h"

#define CCI_MAX_DELAY 1000000

static struct v4l2_subdev *g_cci_subdev[MAX_CCI] = { 0 };
static struct dentry *debugfs_root;

struct v4l2_subdev *cam_cci_get_subdev(int cci_dev_index)
{
	struct v4l2_subdev *sub_device = NULL;

	if ((cci_dev_index < MAX_CCI) && (g_cci_subdev[cci_dev_index] != NULL))
		sub_device = g_cci_subdev[cci_dev_index];
	else
		CAM_WARN(CAM_CCI, "CCI subdev not available at Index: %u, MAX_CCI : %u",
			cci_dev_index, MAX_CCI);

	return sub_device;
}

static int32_t cam_cci_event_workq_cb(void *priv, void *data)
{
	struct cci_event_wq_payload *payload = (struct cci_event_wq_payload *)data;

	if (!payload) {
		CAM_ERR(CAM_CCI, "Invalid payload for kfree worker");
		return -EINVAL;
	}

	/* Free the copied debug string */
	if (payload->debug_string) {
		CAM_DBG(CAM_CCI, "Ctx:%u Timestamp:%llu string: %s size: %d report_id: %d",
			payload->context_id, payload->timestamp, payload->debug_string,
			payload->debug_string_size, payload->report_id);
		kfree(payload->debug_string);
	}

	/* Free the original entry structure*/
	if (payload->entry) {
		if (payload->entry->debug_string) {
			kfree(payload->entry->debug_string);
		} else {
			CAM_ERR(CAM_CCI, "debug string not exist");
		}
		kfree(payload->entry);
	}

	kfree(payload);
	return 0;
}


static void cam_cci_schedule_event_wq(struct cci_device *cci_dev, char *debug_string,
	uint32_t debug_string_size, uint32_t context_id, uint32_t report_id,
        uint64_t timestamp, struct cam_cci_debug_entry *entry)
{
	struct cci_event_wq_payload *payload;
	struct crm_worker_task *task;
	int rc;

	if (!debug_string || !debug_string_size || !cci_dev || !cci_dev->cci_event_worker)
		return;

	payload = kzalloc(sizeof(*payload), GFP_ATOMIC);
	if (!payload) {
		/* Fallback: log the issue but don't free in interrupt context */
		CAM_ERR(CAM_CCI, "CCI%d Failed to allocate kfree payload, memory leak in path",
			cci_dev->soc_info.index);
		kfree(debug_string);
		return;
	}

	payload->debug_string = debug_string;
	payload->debug_string_size = debug_string_size;
	payload->context_id = context_id;
	payload->report_id = report_id;
	payload->timestamp = timestamp;
	payload->entry = entry;
	task = cam_req_mgr_worker_get_task(cci_dev->cci_event_worker);
	if (!task) {
		CAM_ERR(CAM_CCI, "CCI%d Failed to get worker task, memory leak in path",
			cci_dev->soc_info.index);
		if (payload->debug_string)
			kfree(payload->debug_string);
		if (payload->entry) {
			if (payload->entry->debug_string) {
				kfree(payload->entry->debug_string);
			} else {
				CAM_ERR(CAM_CCI, "debug string not exist");
			}
			kfree(payload->entry);
		}
		kfree(payload);
		return;
	}
	task->process_cb = cam_cci_event_workq_cb;
	task->payload = payload;

	rc = cam_req_mgr_worker_enqueue_task(task, cci_dev, CRM_TASK_PRIORITY_0);
	if (rc) {
		CAM_ERR(CAM_CCI, "CCI%d Failed to enqueue kfree task, memory leak in path",
			cci_dev->soc_info.index);
		if (payload->debug_string)
			kfree(payload->debug_string);
		if (payload->entry) {
			if (payload->entry->debug_string) {
				kfree(payload->entry->debug_string);
			} else {
				CAM_ERR(CAM_CCI, "debug string not exist");
			}
			kfree(payload->entry);
		}
		kfree(payload);
	}
}

static long cam_cci_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int32_t rc = 0;

	if (arg == NULL) {
		CAM_ERR(CAM_CCI, "Args is Null");
		return -EINVAL;
	}

	switch (cmd) {
	case VIDIOC_MSM_CCI_CFG:
		rc = -EOPNOTSUPP;
		break;
	case VIDIOC_CAM_CONTROL:
		break;
	default:
		CAM_ERR(CAM_CCI, "Invalid ioctl cmd: %d", cmd);
		rc = -ENOIOCTLCMD;
	}

	return rc;
}

#ifdef CONFIG_COMPAT
static long cam_cci_subdev_compat_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	return cam_cci_subdev_ioctl(sd, cmd, NULL);
}
#endif

irqreturn_t cam_cci_irq(int irq_num, void *data)
{
	uint32_t irq_status0, irq_status1, reg_bmsk;
	uint32_t irq_update_rd_done = 0;
	struct cci_device *cci_dev = data;
	struct cam_hw_soc_info *soc_info =
		&cci_dev->soc_info;
	void __iomem *base = soc_info->reg_map[0].mem_base;
	unsigned long flags;
	bool rd_done_th_assert = false;

	irq_status0 = cam_io_r_mb(base + CCI_IRQ_STATUS_0_ADDR);
	irq_status1 = cam_io_r_mb(base + CCI_IRQ_STATUS_1_ADDR);
	CAM_DBG(CAM_CCI,
		"BASE: %p, irq0:%x irq1:%x",
		base, irq_status0, irq_status1);

	cam_io_w_mb(irq_status0, base + CCI_IRQ_CLEAR_0_ADDR);
	cam_io_w_mb(irq_status1, base + CCI_IRQ_CLEAR_1_ADDR);

	reg_bmsk = CCI_IRQ_MASK_1_RMSK;
	if ((irq_status1 & CCI_IRQ_STATUS_1_I2C_M1_RD_THRESHOLD) &&
	!(irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_RD_DONE_BMSK)) {
		reg_bmsk &= ~CCI_IRQ_STATUS_1_I2C_M1_RD_THRESHOLD;
		spin_lock_irqsave(&cci_dev->lock_status, flags);
		cci_dev->irqs_disabled |=
			CCI_IRQ_STATUS_1_I2C_M1_RD_THRESHOLD;
		spin_unlock_irqrestore(&cci_dev->lock_status, flags);
	}

	if ((irq_status1 & CCI_IRQ_STATUS_1_I2C_M0_RD_THRESHOLD) &&
	!(irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_RD_DONE_BMSK)) {
		reg_bmsk &= ~CCI_IRQ_STATUS_1_I2C_M0_RD_THRESHOLD;
		spin_lock_irqsave(&cci_dev->lock_status, flags);
		cci_dev->irqs_disabled |=
			CCI_IRQ_STATUS_1_I2C_M0_RD_THRESHOLD;
		spin_unlock_irqrestore(&cci_dev->lock_status, flags);
	}

	if (reg_bmsk != CCI_IRQ_MASK_1_RMSK) {
		cam_io_w_mb(reg_bmsk, base + CCI_IRQ_MASK_1_ADDR);
		CAM_DBG(CAM_CCI, "Updating the reg mask for irq1: 0x%x",
			reg_bmsk);
	} else if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_RD_DONE_BMSK ||
		irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_RD_DONE_BMSK) {
		if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_RD_DONE_BMSK) {
			spin_lock_irqsave(&cci_dev->lock_status, flags);
			if (cci_dev->irqs_disabled &
				CCI_IRQ_STATUS_1_I2C_M0_RD_THRESHOLD) {
				irq_update_rd_done |=
					CCI_IRQ_STATUS_1_I2C_M0_RD_THRESHOLD;
				cci_dev->irqs_disabled &=
					~CCI_IRQ_STATUS_1_I2C_M0_RD_THRESHOLD;
			}
			spin_unlock_irqrestore(&cci_dev->lock_status, flags);
		}
		if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_RD_DONE_BMSK) {
			spin_lock_irqsave(&cci_dev->lock_status, flags);
			if (cci_dev->irqs_disabled &
				CCI_IRQ_STATUS_1_I2C_M1_RD_THRESHOLD) {
				irq_update_rd_done |=
					CCI_IRQ_STATUS_1_I2C_M1_RD_THRESHOLD;
				cci_dev->irqs_disabled &=
					~CCI_IRQ_STATUS_1_I2C_M1_RD_THRESHOLD;
			}
			spin_unlock_irqrestore(&cci_dev->lock_status, flags);
		}
	}

	if (irq_update_rd_done != 0) {
		irq_update_rd_done |= cam_io_r_mb(base + CCI_IRQ_MASK_1_ADDR);
		cam_io_w_mb(irq_update_rd_done, base + CCI_IRQ_MASK_1_ADDR);
	}

	cam_io_w_mb(0x1, base + CCI_IRQ_GLOBAL_CLEAR_CMD_ADDR);

	if (irq_status0 & CCI_IRQ_STATUS_0_RST_DONE_ACK_BMSK) {
		struct cam_cci_master_info *cci_master_info;
		struct cam_cci_gpio_info *cci_gpio_info;
		if (cci_dev->cci_master_info[MASTER_0].reset_pending == true) {
			cci_master_info = &cci_dev->cci_master_info[MASTER_0];
			cci_dev->cci_master_info[MASTER_0].reset_pending =
				false;
			if (!cci_master_info->status)
				complete(&cci_master_info->reset_complete);

			complete_all(&cci_master_info->rd_done);
			complete_all(&cci_master_info->th_complete);
		}
		if (cci_dev->cci_master_info[MASTER_1].reset_pending == true) {
			cci_master_info = &cci_dev->cci_master_info[MASTER_1];
			cci_dev->cci_master_info[MASTER_1].reset_pending =
				false;
			if (!cci_master_info->status)
				complete(&cci_master_info->reset_complete);

			complete_all(&cci_master_info->rd_done);
			complete_all(&cci_master_info->th_complete);
		}
		if (cci_dev->cci_gpio_info.reset_pending[GPIOQUEUE_0] == true) {
			cci_gpio_info = &cci_dev->cci_gpio_info;
			cci_dev->cci_gpio_info.reset_pending[GPIOQUEUE_0] =
				false;
			if (!cci_gpio_info->status)
				complete(&cci_gpio_info->reset_complete[GPIOQUEUE_0]);
		}
		if (cci_dev->cci_gpio_info.reset_pending[GPIOQUEUE_1] == true) {
			cci_gpio_info = &cci_dev->cci_gpio_info;
			cci_dev->cci_gpio_info.reset_pending[GPIOQUEUE_1] =
				false;
			if (!cci_gpio_info->status)
				complete(&cci_gpio_info->reset_complete[GPIOQUEUE_1]);
		}
		if (cci_dev->cci_gpio_info.reset_pending[GPIOQUEUE_2] == true) {
			cci_gpio_info = &cci_dev->cci_gpio_info;
			cci_dev->cci_gpio_info.reset_pending[GPIOQUEUE_2] =
				false;
			if (!cci_gpio_info->status)
				complete(&cci_gpio_info->reset_complete[GPIOQUEUE_2]);
		}
	}

	if ((irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_RD_DONE_BMSK) &&
		(irq_status1 & CCI_IRQ_STATUS_1_I2C_M0_RD_THRESHOLD)) {
		cci_dev->cci_master_info[MASTER_0].status = 0;
		rd_done_th_assert = true;
		complete(&cci_dev->cci_master_info[MASTER_0].th_complete);
		complete(&cci_dev->cci_master_info[MASTER_0].rd_done);
	}
	if ((irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_RD_DONE_BMSK) &&
		(!rd_done_th_assert)) {
		cci_dev->cci_master_info[MASTER_0].status = 0;
		rd_done_th_assert = true;
		if (cci_dev->is_burst_read[MASTER_0])
			complete(
			&cci_dev->cci_master_info[MASTER_0].th_complete);
		complete(&cci_dev->cci_master_info[MASTER_0].rd_done);
	}
	if ((irq_status1 & CCI_IRQ_STATUS_1_I2C_M0_RD_THRESHOLD) &&
		(!rd_done_th_assert)) {
		cci_dev->cci_master_info[MASTER_0].status = 0;
		complete(&cci_dev->cci_master_info[MASTER_0].th_complete);
	}
	if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_Q0_REPORT_BMSK) {
		struct cam_cci_master_info *cci_master_info;
		if (cci_dev->en_cci_event_debug && cci_dev->cci_event_worker) {
			uint32_t report_id = 0xF & ((cam_io_r_mb(base + CCI_I2C_M0_Q0_REPORT_STATUS_ADDR)) >> 28);
			char *debug_string = NULL;
			uint32_t debug_string_size = 0;
			uint32_t context_id = cam_cci_find_context_for_i2c_queue(cci_dev, MASTER_0, QUEUE_0);

			if (context_id < CONTEXT_ID_MAX) {
				/* Pop debug string from I2C list queue with dynamic allocation */
				struct cam_cci_debug_entry *entry = NULL;
				int ret = cam_cci_debug_cmd_pop(&cci_dev->context_debug_info[context_id].i2c_debug_list[MASTER_0][QUEUE_0],
						&debug_string, &debug_string_size, &entry);
				uint64_t curr_timestamp = arch_timer_read_counter();

				if (ret == 0 && debug_string_size > 0 && debug_string) {
					cam_cci_schedule_event_wq(cci_dev, debug_string, debug_string_size, context_id,
									report_id, curr_timestamp, entry);
				} else if (ret == -ENOMEM) {
					CAM_ERR(CAM_CCI, "CCI_IRQ_STATUS_I2C_M0_Q0_REPORT_BMSK Ctx:%u Memory allocation failed", context_id);
				} else {
					CAM_ERR(CAM_CCI, "CCI_IRQ_STATUS_I2C_M0_Q0_REPORT_BMSK Ctx:%u report_id %u no string",
								context_id, report_id);
				}
			}
		}

		cci_master_info = &cci_dev->cci_master_info[MASTER_0];
		spin_lock_irqsave(
			&cci_dev->cci_master_info[MASTER_0].lock_q[QUEUE_0],
			flags);
		atomic_set(&cci_master_info->q_free[QUEUE_0], 0);
		cci_master_info->status = 0;
		if (atomic_read(&cci_master_info->done_pending[QUEUE_0]) == 1) {
			complete(&cci_master_info->report_q[QUEUE_0]);
			atomic_set(&cci_master_info->done_pending[QUEUE_0], 0);
		}
		spin_unlock_irqrestore(
			&cci_dev->cci_master_info[MASTER_0].lock_q[QUEUE_0],
			flags);
	}
	if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_Q1_REPORT_BMSK) {
		struct cam_cci_master_info *cci_master_info;
		if (cci_dev->en_cci_event_debug && cci_dev->cci_event_worker) {
			uint32_t reg_offset = MASTER_0 * 0x200 + QUEUE_1 * 0x100;
			uint32_t report_id = 0xF & ((cam_io_r_mb(base + CCI_I2C_M0_Q0_REPORT_STATUS_ADDR + reg_offset)) >> 28);
			char *debug_string = NULL;
			uint32_t debug_string_size = 0;
			uint32_t context_id = cam_cci_find_context_for_i2c_queue(cci_dev, MASTER_0, QUEUE_1);

			if (context_id < CONTEXT_ID_MAX) {
				/* Pop debug string from I2C list queue with dynamic allocation */
				struct cam_cci_debug_entry *entry = NULL;
				int ret = cam_cci_debug_cmd_pop(&cci_dev->context_debug_info[context_id].i2c_debug_list[MASTER_0][QUEUE_1],
						&debug_string, &debug_string_size, &entry);
				uint64_t curr_timestamp = arch_timer_read_counter();

				if (ret == 0 && debug_string_size > 0 && debug_string) {
					cam_cci_schedule_event_wq(cci_dev, debug_string, debug_string_size, context_id,
									report_id, curr_timestamp, entry);
				} else if (ret == -ENOMEM) {
					CAM_ERR(CAM_CCI, "CCI_IRQ_STATUS_I2C_M0_Q1_REPORT_BMSK Ctx:%u Memory allocation failed", context_id);
				} else {
					CAM_ERR(CAM_CCI, "CCI_IRQ_STATUS_I2C_M0_Q1_REPORT_BMSK Ctx:%u report_id %u no string",
								context_id, report_id);
				}
			}
		}

		cci_master_info = &cci_dev->cci_master_info[MASTER_0];
		spin_lock_irqsave(
			&cci_dev->cci_master_info[MASTER_0].lock_q[QUEUE_1],
			flags);
		atomic_set(&cci_master_info->q_free[QUEUE_1], 0);
		cci_master_info->status = 0;
		if (atomic_read(&cci_master_info->done_pending[QUEUE_1]) == 1) {
			complete(&cci_master_info->report_q[QUEUE_1]);
			atomic_set(&cci_master_info->done_pending[QUEUE_1], 0);
		}
		spin_unlock_irqrestore(
			&cci_dev->cci_master_info[MASTER_0].lock_q[QUEUE_1],
			flags);
	}
	if (irq_status1 & CCI_IRQ_STATUS_1_GPIO_Q0_REPORT_BMSK) {
		struct cam_cci_gpio_info *cci_gpio_info;
		if (cci_dev->en_cci_event_debug && cci_dev->cci_event_worker) {
			uint32_t report_id = 0xF & ((cam_io_r_mb(base + CCI_GPIO_Q0_REPORT_STATUS_ADDR)) >> 28);
			char *debug_string = NULL;
			uint32_t debug_string_size = 0;
			uint32_t context_id = cam_cci_find_context_for_gpio_queue(cci_dev, GPIOQUEUE_0);

			if (context_id < CONTEXT_ID_MAX) {
				/* Pop debug string from list queue with dynamic allocation */
				struct cam_cci_debug_entry *entry = NULL;
				int ret = cam_cci_debug_cmd_pop(&cci_dev->context_debug_info[context_id].gpio_debug_list[GPIOQUEUE_0],
						&debug_string, &debug_string_size, &entry);
				uint64_t curr_timestamp = arch_timer_read_counter();

				if (ret == 0 && debug_string_size > 0 && debug_string) {
					cam_cci_schedule_event_wq(cci_dev, debug_string, debug_string_size, context_id,
									report_id, curr_timestamp, entry);
				} else if (ret == -ENOMEM) {
					CAM_ERR(CAM_CCI, "CCI_IRQ_STATUS_GPIO_Q0_REPORT_BMSK Ctx:%u Memory allocation failed", context_id);
				} else {
					CAM_ERR(CAM_CCI, "CCI_IRQ_STATUS_GPIO_Q0_REPORT_BMSK Ctx:%u report_id %u no string",
								context_id, report_id);
				}
			}
		}

		cci_gpio_info = &cci_dev->cci_gpio_info;
		spin_lock_irqsave(
			&cci_dev->cci_gpio_info.lock_q[GPIOQUEUE_0],
			flags);
		cci_gpio_info->status = 0;
		complete(&cci_gpio_info->report_q[GPIOQUEUE_0]);
		spin_unlock_irqrestore(
			&cci_dev->cci_gpio_info.lock_q[GPIOQUEUE_0],
			flags);
	}
	if (irq_status1 & CCI_IRQ_STATUS_1_GPIO_Q1_REPORT_BMSK) {
		struct cam_cci_gpio_info *cci_gpio_info;
		if (cci_dev->en_cci_event_debug && cci_dev->cci_event_worker) {
			uint32_t reg_offset = GPIOQUEUE_1 * 0x100;
			uint32_t report_id = 0xF & ((cam_io_r_mb(base + CCI_GPIO_Q0_REPORT_STATUS_ADDR + reg_offset)) >> 28);
			char *debug_string = NULL;
			uint32_t debug_string_size = 0;
			uint32_t context_id = cam_cci_find_context_for_gpio_queue(cci_dev, GPIOQUEUE_1);

			if (context_id < CONTEXT_ID_MAX) {
				/* Pop debug string from list queue with dynamic allocation */
				struct cam_cci_debug_entry *entry = NULL;
				int ret = cam_cci_debug_cmd_pop(&cci_dev->context_debug_info[context_id].gpio_debug_list[GPIOQUEUE_1],
						&debug_string, &debug_string_size, &entry);
				uint64_t curr_timestamp = arch_timer_read_counter();

				if (ret == 0 && debug_string_size > 0 && debug_string) {
					cam_cci_schedule_event_wq(cci_dev, debug_string, debug_string_size, context_id,
									report_id, curr_timestamp, entry);
				} else if (ret == -ENOMEM) {
					CAM_ERR(CAM_CCI, "CCI_IRQ_STATUS_GPIO_Q1_REPORT_BMSK Ctx:%u Memory allocation failed", context_id);
				} else {
					CAM_ERR(CAM_CCI, "CCI_IRQ_STATUS_GPIO_Q1_REPORT_BMSK Ctx:%u report_id %u no string",
								context_id, report_id);
				}
			}
		}

		cci_gpio_info = &cci_dev->cci_gpio_info;
		spin_lock_irqsave(
			&cci_dev->cci_gpio_info.lock_q[GPIOQUEUE_1],
			flags);
		cci_gpio_info->status = 0;
		complete(&cci_gpio_info->report_q[GPIOQUEUE_1]);
		spin_unlock_irqrestore(
			&cci_dev->cci_gpio_info.lock_q[GPIOQUEUE_1],
			flags);
	}
	if (irq_status1 & CCI_IRQ_STATUS_1_GPIO_Q2_REPORT_BMSK) {
		struct cam_cci_gpio_info *cci_gpio_info;
		if (cci_dev->en_cci_event_debug && cci_dev->cci_event_worker) {
			uint32_t reg_offset = GPIOQUEUE_2 * 0x100;
			uint32_t report_id = 0xF & ((cam_io_r_mb(base + CCI_GPIO_Q0_REPORT_STATUS_ADDR + reg_offset)) >> 28);
			char *debug_string = NULL;
			uint32_t debug_string_size = 0;
			uint32_t context_id = cam_cci_find_context_for_gpio_queue(cci_dev, GPIOQUEUE_2);

			if (context_id < CONTEXT_ID_MAX) {
				/* Pop debug string from list queue with dynamic allocation */
				struct cam_cci_debug_entry *entry = NULL;
				int ret = cam_cci_debug_cmd_pop(&cci_dev->context_debug_info[context_id].gpio_debug_list[GPIOQUEUE_2],
						&debug_string, &debug_string_size, &entry);
				uint64_t curr_timestamp = arch_timer_read_counter();

				if (ret == 0 && debug_string_size > 0 && debug_string) {
					cam_cci_schedule_event_wq(cci_dev, debug_string, debug_string_size, context_id,
									report_id, curr_timestamp, entry);
				} else if (ret == -ENOMEM) {
					CAM_ERR(CAM_CCI, "CCI_IRQ_STATUS_GPIO_Q2_REPORT_BMSK Ctx:%u Memory allocation failed", context_id);
				} else {
					CAM_ERR(CAM_CCI, "CCI_IRQ_STATUS_GPIO_Q2_REPORT_BMSK Ctx:%u report_id %u no string",
								context_id, report_id);
				}
			}
		}

		cci_gpio_info = &cci_dev->cci_gpio_info;
		spin_lock_irqsave(
			&cci_dev->cci_gpio_info.lock_q[GPIOQUEUE_2],
			flags);
		cci_gpio_info->status = 0;
		complete(&cci_gpio_info->report_q[GPIOQUEUE_2]);
		spin_unlock_irqrestore(
			&cci_dev->cci_gpio_info.lock_q[GPIOQUEUE_2],
			flags);
	}

	rd_done_th_assert = false;
	if ((irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_RD_DONE_BMSK) &&
		(irq_status1 & CCI_IRQ_STATUS_1_I2C_M1_RD_THRESHOLD)) {
		cci_dev->cci_master_info[MASTER_1].status = 0;
		rd_done_th_assert = true;
		complete(&cci_dev->cci_master_info[MASTER_1].th_complete);
		complete(&cci_dev->cci_master_info[MASTER_1].rd_done);
	}
	if ((irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_RD_DONE_BMSK) &&
		(!rd_done_th_assert)) {
		cci_dev->cci_master_info[MASTER_1].status = 0;
		rd_done_th_assert = true;
		if (cci_dev->is_burst_read[MASTER_1])
			complete(
			&cci_dev->cci_master_info[MASTER_1].th_complete);
		complete(&cci_dev->cci_master_info[MASTER_1].rd_done);
	}
	if ((irq_status1 & CCI_IRQ_STATUS_1_I2C_M1_RD_THRESHOLD) &&
		(!rd_done_th_assert)) {
		cci_dev->cci_master_info[MASTER_1].status = 0;
		complete(&cci_dev->cci_master_info[MASTER_1].th_complete);
	}
	if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_Q0_REPORT_BMSK) {
		struct cam_cci_master_info *cci_master_info;
		if (cci_dev->en_cci_event_debug && cci_dev->cci_event_worker) {
			uint32_t reg_offset = MASTER_1 * 0x200 + QUEUE_0 * 0x100;
			uint32_t report_id = 0xF & ((cam_io_r_mb(base + CCI_I2C_M0_Q0_REPORT_STATUS_ADDR + reg_offset)) >> 28);
			char *debug_string = NULL;
			uint32_t debug_string_size = 0;
			uint32_t context_id = cam_cci_find_context_for_i2c_queue(cci_dev, MASTER_1, QUEUE_0);

			if (context_id < CONTEXT_ID_MAX) {
				/* Pop debug string from I2C list queue with dynamic allocation */
				struct cam_cci_debug_entry *entry = NULL;
				int ret = cam_cci_debug_cmd_pop(&cci_dev->context_debug_info[context_id].i2c_debug_list[MASTER_1][QUEUE_0],
						&debug_string, &debug_string_size, &entry);
				uint64_t curr_timestamp = arch_timer_read_counter();

				if (ret == 0 && debug_string_size > 0 && debug_string) {
					cam_cci_schedule_event_wq(cci_dev, debug_string, debug_string_size, context_id,
									report_id, curr_timestamp, entry);
				} else if (ret == -ENOMEM) {
					CAM_ERR(CAM_CCI, "CCI_IRQ_STATUS_I2C_M1_Q0_REPORT_BMSK Ctx:%u Memory allocation failed", context_id);
				} else {
					CAM_ERR(CAM_CCI, "CCI_IRQ_STATUS_I2C_M1_Q0_REPORT_BMSK Ctx:%u report_id %u no string",
							context_id, report_id);
				}
			}
		}
		cci_master_info = &cci_dev->cci_master_info[MASTER_1];
		spin_lock_irqsave(
			&cci_dev->cci_master_info[MASTER_1].lock_q[QUEUE_0],
			flags);
		atomic_set(&cci_master_info->q_free[QUEUE_0], 0);
		cci_master_info->status = 0;
		if (atomic_read(&cci_master_info->done_pending[QUEUE_0]) == 1) {
			complete(&cci_master_info->report_q[QUEUE_0]);
			atomic_set(&cci_master_info->done_pending[QUEUE_0], 0);
		}
		spin_unlock_irqrestore(
			&cci_dev->cci_master_info[MASTER_1].lock_q[QUEUE_0],
			flags);
	}
	if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_Q1_REPORT_BMSK) {
		struct cam_cci_master_info *cci_master_info;
		if (cci_dev->en_cci_event_debug && cci_dev->cci_event_worker) {
			uint32_t reg_offset = MASTER_1 * 0x200 + QUEUE_1 * 0x100;
			uint32_t report_id = 0xF & ((cam_io_r_mb(base + CCI_I2C_M0_Q0_REPORT_STATUS_ADDR + reg_offset)) >> 28);
			char *debug_string = NULL;
			uint32_t debug_string_size = 0;
			uint32_t context_id = cam_cci_find_context_for_i2c_queue(cci_dev, MASTER_1, QUEUE_1);

			if (context_id < CONTEXT_ID_MAX) {
				/* Pop debug string from I2C list queue with dynamic allocation */
				struct cam_cci_debug_entry *entry = NULL;
				int ret = cam_cci_debug_cmd_pop(&cci_dev->context_debug_info[context_id].i2c_debug_list[MASTER_1][QUEUE_1],
						&debug_string, &debug_string_size, &entry);
				uint64_t curr_timestamp = arch_timer_read_counter();

				if (ret == 0 && debug_string_size > 0 && debug_string) {
					cam_cci_schedule_event_wq(cci_dev, debug_string, debug_string_size, context_id,
									report_id, curr_timestamp, entry);
				} else if (ret == -ENOMEM) {
					CAM_ERR(CAM_CCI, "CCI_IRQ_STATUS_I2C_M1_Q1_REPORT_BMSK Ctx:%u Memory allocation failed", context_id);
				} else {
					CAM_ERR(CAM_CCI, "CCI_IRQ_STATUS_I2C_M1_Q1_REPORT_BMSK Ctx:%u report_id %u no string",
							context_id, report_id);
				}
			}
		}

		cci_master_info = &cci_dev->cci_master_info[MASTER_1];
		spin_lock_irqsave(
			&cci_dev->cci_master_info[MASTER_1].lock_q[QUEUE_1],
			flags);
		atomic_set(&cci_master_info->q_free[QUEUE_1], 0);
		cci_master_info->status = 0;
		if (atomic_read(&cci_master_info->done_pending[QUEUE_1]) == 1) {
			complete(&cci_master_info->report_q[QUEUE_1]);
			atomic_set(&cci_master_info->done_pending[QUEUE_1], 0);
		}
		spin_unlock_irqrestore(
			&cci_dev->cci_master_info[MASTER_1].lock_q[QUEUE_1],
			flags);
	}
	if (irq_status1 & CCI_IRQ_STATUS_1_I2C_M0_RD_PAUSE)
		CAM_DBG(CAM_CCI, "RD_PAUSE ON MASTER_0");

	if (irq_status1 & CCI_IRQ_STATUS_1_I2C_M1_RD_PAUSE)
		CAM_DBG(CAM_CCI, "RD_PAUSE ON MASTER_1");

	if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_Q0Q1_HALT_ACK_BMSK) {
		cci_dev->cci_master_info[MASTER_0].reset_pending = true;
		cam_io_w_mb(CCI_M0_RESET_RMSK,
			base + CCI_RESET_CMD_ADDR);
	}
	if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_Q0Q1_HALT_ACK_BMSK) {
		cci_dev->cci_master_info[MASTER_1].reset_pending = true;
		cam_io_w_mb(CCI_M1_RESET_RMSK,
			base + CCI_RESET_CMD_ADDR);
	}
	if (irq_status1 & CCI_IRQ_STATUS_1_GPIO_Q0_HALT_ACK_BMSK) {
		cci_dev->cci_gpio_info.reset_pending[GPIOQUEUE_0] = true;
		cam_io_w_mb(CCI_GPIO_Q0_RESET_RMSK,
			base + CCI_RESET_CMD_ADDR);
	}
	if (irq_status1 & CCI_IRQ_STATUS_1_GPIO_Q1_HALT_ACK_BMSK) {
		cci_dev->cci_gpio_info.reset_pending[GPIOQUEUE_1] = true;
		cam_io_w_mb(CCI_GPIO_Q1_RESET_RMSK,
			base + CCI_RESET_CMD_ADDR);
	}
	if (irq_status1 & CCI_IRQ_STATUS_1_GPIO_Q2_HALT_ACK_BMSK) {
		cci_dev->cci_gpio_info.reset_pending[GPIOQUEUE_2] = true;
		cam_io_w_mb(CCI_GPIO_Q2_RESET_RMSK,
			base + CCI_RESET_CMD_ADDR);
	}
	if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_ERROR_BMSK) {
		cci_dev->cci_master_info[MASTER_0].status = -EINVAL;
		if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_Q0_NACK_ERROR_BMSK) {
			CAM_ERR(CAM_CCI,
				"Base:%pK,cci: %d, M0_Q0 NACK ERROR: 0x%x",
				base, cci_dev->soc_info.index, irq_status0);
			cam_cci_dump_registers(cci_dev, MASTER_0,
					QUEUE_0);
			complete_all(&cci_dev->cci_master_info[MASTER_0]
				.report_q[QUEUE_0]);
		}
		if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_Q1_NACK_ERROR_BMSK) {
			CAM_ERR(CAM_CCI,
				"Base:%pK,cci: %d, M0_Q1 NACK ERROR: 0x%x",
				base, cci_dev->soc_info.index, irq_status0);
			cam_cci_dump_registers(cci_dev, MASTER_0,
					QUEUE_1);
			complete_all(&cci_dev->cci_master_info[MASTER_0]
			.report_q[QUEUE_1]);
		}
		if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_Q0Q1_ERROR_BMSK)
			CAM_ERR(CAM_CCI,
			"Base:%pK, cci: %d, M0 QUEUE_OVER/UNDER_FLOW OR CMD ERR: 0x%x",
				base, cci_dev->soc_info.index, irq_status0);
		if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M0_RD_ERROR_BMSK)
			CAM_ERR(CAM_CCI,
				"Base: %pK, M0 RD_OVER/UNDER_FLOW ERROR: 0x%x",
				base, irq_status0);

		cci_dev->cci_master_info[MASTER_0].reset_pending = true;
		cam_io_w_mb(CCI_M0_RESET_RMSK, base + CCI_RESET_CMD_ADDR);
	}
	if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_ERROR_BMSK) {
		cci_dev->cci_master_info[MASTER_1].status = -EINVAL;
		if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_Q0_NACK_ERROR_BMSK) {
			CAM_ERR(CAM_CCI,
				"Base:%pK, cci: %d, M1_Q0 NACK ERROR: 0x%x",
				base, cci_dev->soc_info.index, irq_status0);
			cam_cci_dump_registers(cci_dev, MASTER_1,
					QUEUE_0);
			complete_all(&cci_dev->cci_master_info[MASTER_1]
			.report_q[QUEUE_0]);
		}
		if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_Q1_NACK_ERROR_BMSK) {
			CAM_ERR(CAM_CCI,
				"Base:%pK, cci: %d, M1_Q1 NACK ERROR: 0x%x",
				base, cci_dev->soc_info.index, irq_status0);
			cam_cci_dump_registers(cci_dev, MASTER_1,
				QUEUE_1);
			complete_all(&cci_dev->cci_master_info[MASTER_1]
			.report_q[QUEUE_1]);
		}
		if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_Q0Q1_ERROR_BMSK)
			CAM_ERR(CAM_CCI,
			"Base:%pK, cci: %d, M1 QUEUE_OVER_UNDER_FLOW OR CMD ERROR:0x%x",
				base, cci_dev->soc_info.index, irq_status0);
		if (irq_status0 & CCI_IRQ_STATUS_0_I2C_M1_RD_ERROR_BMSK)
			CAM_ERR(CAM_CCI,
				"Base:%pK, cci: %d, M1 RD_OVER/UNDER_FLOW ERROR: 0x%x",
				base, cci_dev->soc_info.index, irq_status0);

		cci_dev->cci_master_info[MASTER_1].reset_pending = true;
		cam_io_w_mb(CCI_M1_RESET_RMSK, base + CCI_RESET_CMD_ADDR);
	}
	if (irq_status1 & CCI_IRQ_STATUS_1_GPIO_Q0_ERROR_BMSK) {
		cci_dev->cci_gpio_info.status = -EINVAL;
		CAM_ERR(CAM_CCI,
		"Base:%pK, cci: %d, Q0 QUEUE_OVER/UNDER_FLOW OR CMD ERR: 0x%x",
			base, cci_dev->soc_info.index, irq_status1);

		cci_dev->cci_gpio_info.reset_pending[GPIOQUEUE_0] = true;
		cam_io_w_mb(CCI_GPIO_Q0_RESET_RMSK, base + CCI_RESET_CMD_ADDR);
	}
	if (irq_status1 & CCI_IRQ_STATUS_1_GPIO_Q1_ERROR_BMSK) {
		cci_dev->cci_gpio_info.status = -EINVAL;
		CAM_ERR(CAM_CCI,
		"Base:%pK, cci: %d, Q1 QUEUE_OVER/UNDER_FLOW OR CMD ERR: 0x%x",
			base, cci_dev->soc_info.index, irq_status1);

		cci_dev->cci_gpio_info.reset_pending[GPIOQUEUE_1] = true;
		cam_io_w_mb(CCI_GPIO_Q1_RESET_RMSK, base + CCI_RESET_CMD_ADDR);
	}
	if (irq_status1 & CCI_IRQ_STATUS_1_GPIO_Q2_ERROR_BMSK) {
		cci_dev->cci_gpio_info.status = -EINVAL;
		CAM_ERR(CAM_CCI,
		"Base:%pK, cci: %d, Q2 QUEUE_OVER/UNDER_FLOW OR CMD ERR: 0x%x",
			base, cci_dev->soc_info.index, irq_status1);

		cci_dev->cci_gpio_info.reset_pending[GPIOQUEUE_2] = true;
		cam_io_w_mb(CCI_GPIO_Q2_RESET_RMSK, base + CCI_RESET_CMD_ADDR);
	}

	return IRQ_HANDLED;
}

static int cam_cci_irq_routine(struct v4l2_subdev *sd, u32 status,
	bool *handled)
{
	struct cci_device *cci_dev = v4l2_get_subdevdata(sd);
	irqreturn_t ret;
	struct cam_hw_soc_info *soc_info =
		&cci_dev->soc_info;
	ret = cam_cci_irq(soc_info->irq_num, cci_dev);
	*handled = true;
	return 0;
}

static struct v4l2_subdev_core_ops cci_subdev_core_ops = {
	.ioctl = cam_cci_subdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = cam_cci_subdev_compat_ioctl,
#endif
	.interrupt_service_routine = cam_cci_irq_routine,
};

static const struct v4l2_subdev_ops cci_subdev_ops = {
	.core = &cci_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops cci_subdev_intern_ops;

static int cam_cci_get_debug(void *data, u64 *val)
{
	struct cci_device *cci_dev = (struct cci_device *)data;

	*val = cci_dev->dump_en;

	return 0;
}

static int cam_cci_set_debug(void *data, u64 val)
{
	struct cci_device *cci_dev = (struct cci_device *)data;

	cci_dev->dump_en = val;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(cam_cci_debug,
	cam_cci_get_debug,
	cam_cci_set_debug, "%16llu\n");

static int cam_cci_create_debugfs_entry(struct cci_device *cci_dev)
{
	int rc = 0;
	struct dentry *dbgfileptr = NULL;
	char client_name[128] = {0};

	if (!debugfs_root) {
		dbgfileptr = debugfs_create_dir("cam_cci", NULL);
		if (!dbgfileptr) {
			CAM_ERR(CAM_CCI, "debugfs directory creation fail");
			rc = -ENOENT;
			goto end;
		}
		debugfs_root = dbgfileptr;
	}

	if (cci_dev->soc_info.index >= MAX_CCI) {
		CAM_WARN(CAM_CCI,
			"DebugFS not enabled for en_dump_cci%d", cci_dev->soc_info.index);
		goto end;
	}

	snprintf(client_name, sizeof(client_name), "en_dump_cci%d", cci_dev->soc_info.index);
	dbgfileptr = debugfs_create_file(client_name, 0644,
		debugfs_root, cci_dev, &cam_cci_debug);
	if (IS_ERR(dbgfileptr)) {
		if (PTR_ERR(dbgfileptr) == -ENODEV)
			CAM_WARN(CAM_CCI,
				"DebugFS not enabled for %s", client_name);
		else {
			rc = PTR_ERR(dbgfileptr);
		}
	}

	/* Create the CCI directory for this cci */
	snprintf(client_name, sizeof(client_name), "CCI%d",
		cci_dev->soc_info.index);
	dbgfileptr = debugfs_create_dir(client_name, debugfs_root);
	if (IS_ERR(dbgfileptr)) {
		CAM_ERR(CAM_CCI, "Could not create a debugfs PHY indx subdirectory. rc: %ld",
			dbgfileptr);
		return -ENOENT;
	}

        debugfs_create_bool("en_cci_event_debug", 0644,
                dbgfileptr, &cci_dev->en_cci_event_debug);

end:
	return rc;
}

static int cam_cci_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_cpas_register_params cpas_parms;
	struct cci_device *new_cci_dev;
	struct cam_hw_soc_info *soc_info = NULL;
	int rc = 0;
	struct platform_device *pdev = to_platform_device(dev);

	new_cci_dev = devm_kzalloc(&pdev->dev, sizeof(struct cci_device),
		GFP_KERNEL);
	if (!new_cci_dev) {
		CAM_ERR(CAM_CCI, "Memory allocation failed for cci_dev");
		return -ENOMEM;
	}
	soc_info = &new_cci_dev->soc_info;

	new_cci_dev->v4l2_dev_str.pdev = pdev;

	soc_info->pdev = pdev;
	soc_info->dev = &pdev->dev;
	soc_info->dev_name = pdev->name;

	rc = cam_cci_parse_dt_info(pdev, new_cci_dev);
	if (rc < 0) {
		CAM_ERR(CAM_CCI, "Resource get Failed rc:%d", rc);
		goto cci_no_resource;
	}

	new_cci_dev->v4l2_dev_str.internal_ops =
		&cci_subdev_intern_ops;
	new_cci_dev->v4l2_dev_str.ops =
		&cci_subdev_ops;
	strscpy(new_cci_dev->device_name, CAMX_CCI_DEV_NAME,
		sizeof(new_cci_dev->device_name));
	new_cci_dev->v4l2_dev_str.name =
		new_cci_dev->device_name;
	new_cci_dev->v4l2_dev_str.sd_flags = V4L2_SUBDEV_FL_HAS_EVENTS;
	new_cci_dev->v4l2_dev_str.ent_function =
		CAM_CCI_DEVICE_TYPE;
	new_cci_dev->v4l2_dev_str.token =
		new_cci_dev;

	rc = cam_register_subdev(&(new_cci_dev->v4l2_dev_str));
	if (rc < 0) {
		CAM_ERR(CAM_CCI, "Fail with cam_register_subdev rc: %d", rc);
		goto cci_no_resource;
	}

	platform_set_drvdata(pdev, &(new_cci_dev->v4l2_dev_str.sd));
	v4l2_set_subdevdata(&new_cci_dev->v4l2_dev_str.sd, new_cci_dev);
	if (soc_info->index >= MAX_CCI) {
		CAM_ERR(CAM_CCI, "Invalid index: %d max supported:%d",
			soc_info->index, MAX_CCI-1);
		goto cci_no_resource;
	}

	g_cci_subdev[soc_info->index] = &new_cci_dev->v4l2_dev_str.sd;
	mutex_init(&(new_cci_dev->init_mutex));
	mutex_init(&(new_cci_dev->ctx_mutex));
	CAM_DBG(CAM_CCI, "Device Type :%d", soc_info->index);

	cpas_parms.cam_cpas_client_cb = NULL;
	cpas_parms.cell_index = soc_info->index;
	cpas_parms.dev = &pdev->dev;
	cpas_parms.userdata = new_cci_dev;
	strscpy(cpas_parms.identifier, "cci", CAM_HW_IDENTIFIER_LENGTH);
	rc = cam_cpas_register_client(&cpas_parms);
	if (rc) {
		CAM_ERR(CAM_CCI, "CPAS registration failed rc:%d", rc);
		goto cci_unregister_subdev;
	}

	CAM_DBG(CAM_CCI, "CPAS registration successful handle=%d",
		cpas_parms.client_handle);
	new_cci_dev->cpas_handle = cpas_parms.client_handle;

	rc = cam_cci_create_debugfs_entry(new_cci_dev);
	if (rc) {
		CAM_WARN(CAM_CCI, "debugfs creation failed");
		rc = 0;
	}
	CAM_DBG(CAM_CCI, "Component bound successfully");
	return rc;

cci_unregister_subdev:
	cam_unregister_subdev(&(new_cci_dev->v4l2_dev_str));
cci_no_resource:
	devm_kfree(&pdev->dev, new_cci_dev);
	return rc;
}

static void cam_cci_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	int rc = 0;
	struct platform_device *pdev = to_platform_device(dev);

	struct v4l2_subdev *subdev = platform_get_drvdata(pdev);
	struct cci_device *cci_dev =
		v4l2_get_subdevdata(subdev);

	cam_cpas_unregister_client(cci_dev->cpas_handle);
	debugfs_remove_recursive(debugfs_root);
	debugfs_root = NULL;
	cam_cci_soc_remove(pdev, cci_dev);
	rc = cam_unregister_subdev(&(cci_dev->v4l2_dev_str));
	if (rc < 0)
		CAM_ERR(CAM_CCI, "Fail with cam_unregister_subdev. rc:%d", rc);

	devm_kfree(&pdev->dev, cci_dev);
}

const static struct component_ops cam_cci_component_ops = {
	.bind = cam_cci_component_bind,
	.unbind = cam_cci_component_unbind,
};

static int cam_cci_platform_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_CCI, "Adding CCI component");

	cam_soc_util_initialize_power_domain(&pdev->dev);

	rc = component_add(&pdev->dev, &cam_cci_component_ops);
	if (rc)
		CAM_ERR(CAM_CCI, "failed to add component rc: %d", rc);

	return rc;
}

#if KERNEL_VERSION(6, 10, 0) > LINUX_VERSION_CODE
static int cam_cci_device_remove(struct platform_device *pdev)
#else
static void cam_cci_device_remove(struct platform_device *pdev)
#endif
{
	component_del(&pdev->dev, &cam_cci_component_ops);

	cam_soc_util_uninitialize_power_domain(&pdev->dev);

#if KERNEL_VERSION(6, 10, 0) > LINUX_VERSION_CODE
	return 0;
#endif
}

static const struct of_device_id cam_cci_dt_match[] = {
	{.compatible = "qcom,cci"},
	{}
};

MODULE_DEVICE_TABLE(of, cam_cci_dt_match);

struct platform_driver cci_driver = {
	.probe = cam_cci_platform_probe,
	.remove = cam_cci_device_remove,
	.driver = {
		.name = CAMX_CCI_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = cam_cci_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_cci_init_module(void)
{
	return platform_driver_register(&cci_driver);
}

void cam_cci_exit_module(void)
{
	platform_driver_unregister(&cci_driver);
}

MODULE_DESCRIPTION("MSM CCI driver");
MODULE_LICENSE("GPL v2");
