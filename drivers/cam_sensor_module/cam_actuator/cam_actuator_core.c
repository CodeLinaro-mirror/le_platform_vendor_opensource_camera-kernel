// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/module.h>
#include <cam_sensor_cmn_header.h>
#include "cam_actuator_core.h"
#include "cam_sensor_util.h"
#include "cam_trace.h"
#include "cam_common_util.h"
#include "cam_packet_util.h"

#define WITH_NO_CRM_MASK BIT(0)

int32_t cam_actuator_construct_default_power_setting(
	struct cam_sensor_power_ctrl_t *power_info)
{
	int rc = 0;

	power_info->power_setting_size = 1;
	power_info->power_setting =
		kzalloc(sizeof(struct cam_sensor_power_setting),
			GFP_KERNEL);
	if (!power_info->power_setting)
		return -ENOMEM;

	power_info->power_setting[0].seq_type = SENSOR_VAF;
	power_info->power_setting[0].seq_val = CAM_VAF;
	power_info->power_setting[0].config_val = 1;
	power_info->power_setting[0].delay = 2;

	power_info->power_down_setting_size = 1;
	power_info->power_down_setting =
		kzalloc(sizeof(struct cam_sensor_power_setting),
			GFP_KERNEL);
	if (!power_info->power_down_setting) {
		rc = -ENOMEM;
		goto free_power_settings;
	}

	power_info->power_down_setting[0].seq_type = SENSOR_VAF;
	power_info->power_down_setting[0].seq_val = CAM_VAF;
	power_info->power_down_setting[0].config_val = 0;

	return rc;

free_power_settings:
	kfree(power_info->power_setting);
	power_info->power_setting = NULL;
	power_info->power_setting_size = 0;
	return rc;
}

static int32_t cam_actuator_power_up(struct cam_actuator_ctrl_t *a_ctrl)
{
	int rc = 0;
	struct cam_hw_soc_info  *soc_info =
		&a_ctrl->soc_info;
	struct cam_actuator_soc_private  *soc_private;
	struct cam_sensor_power_ctrl_t *power_info;

	soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;

	if ((power_info->power_setting == NULL) &&
		(power_info->power_down_setting == NULL)) {
		CAM_INFO(CAM_ACTUATOR,
			"Using default power settings");
		rc = cam_actuator_construct_default_power_setting(power_info);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Construct default actuator power setting failed.");
			return rc;
		}
	}

	/* Parse and fill vreg params for power up settings */
	rc = msm_camera_fill_vreg_params(
		&a_ctrl->soc_info,
		power_info->power_setting,
		power_info->power_setting_size);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR,
			"failed to fill vreg params for power up rc:%d", rc);
		return rc;
	}

	/* Parse and fill vreg params for power down settings*/
	rc = msm_camera_fill_vreg_params(
		&a_ctrl->soc_info,
		power_info->power_down_setting,
		power_info->power_down_setting_size);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR,
			"failed to fill vreg params power down rc:%d", rc);
		return rc;
	}

	power_info->dev = soc_info->dev;

	rc = cam_sensor_core_power_up(power_info, soc_info);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR,
			"failed in actuator power up rc %d", rc);
		return rc;
	}

	rc = camera_io_init(&a_ctrl->io_master_info);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "cci init failed: rc: %d", rc);
		goto cci_failure;
	}

	return rc;
cci_failure:
	if (cam_sensor_util_power_down(power_info, soc_info))
		CAM_ERR(CAM_ACTUATOR, "Power down failure");

	return rc;
}

static int32_t cam_actuator_power_down(struct cam_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	struct cam_sensor_power_ctrl_t *power_info;
	struct cam_hw_soc_info *soc_info = &a_ctrl->soc_info;
	struct cam_actuator_soc_private  *soc_private;

	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "failed: a_ctrl %pK", a_ctrl);
		return -EINVAL;
	}

	soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;
	soc_info = &a_ctrl->soc_info;

	if (!power_info) {
		CAM_ERR(CAM_ACTUATOR, "failed: power_info %pK", power_info);
		return -EINVAL;
	}
	rc = cam_sensor_util_power_down(power_info, soc_info);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR, "power down the core is failed:%d", rc);
		return rc;
	}

	camera_io_release(&a_ctrl->io_master_info);

	return rc;
}

int cam_actuator_no_crm_notify_dev(uint32_t dev_hdl,
       struct cam_req_mgr_no_crm_notify_device *notify_subdev)
{
	int rc = 0;
	struct cam_actuator_ctrl_t *a_ctrl = NULL;
	uint32_t line_no = 0;

	a_ctrl = (struct cam_actuator_ctrl_t *)
		cam_get_device_priv(notify_subdev->dev_hdl);

	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "Device data is NULL");
		return -EINVAL;
	}

	if (a_ctrl->actuator_trigger_data.actuator_no != a_ctrl->id)
	{
		return -EINVAL;
	}

	line_no = a_ctrl->actuator_trigger_data.line_no;
	a_ctrl->actuator_trigger_data = *((struct cam_actuator_trigger_data *)notify_subdev->data);
	a_ctrl->actuator_trigger_data.line_no = line_no;
	CAM_DBG(CAM_ACTUATOR, "csid %d cid %d actuator no %d",
		a_ctrl->actuator_trigger_data.csid, a_ctrl->actuator_trigger_data.cid,
		a_ctrl->actuator_trigger_data.actuator_no);

	return rc;
}

static int32_t cam_actuator_get_cci_contextid (
	struct cam_actuator_ctrl_t *a_ctrl)
{
	struct cam_cci_trigger_data trigger_data = {0};

	a_ctrl->cci_contextId = CONTEXT_ID_MAX;
	if (!a_ctrl->bridge_intf.enable_crm) {
		int rc = 0;
		trigger_data.cid = a_ctrl->actuator_trigger_data.cid;
		trigger_data.csid = a_ctrl->actuator_trigger_data.csid;
		trigger_data.is_sensor_ctx = false;
		if (a_ctrl->actuator_trigger_data.gpio_mask >= 0) {
			trigger_data.gpio_mask = a_ctrl->actuator_trigger_data.gpio_mask;
		} else {
			CAM_ERR(CAM_ACTUATOR, "Invalid cci-timer");
			return -EINVAL;
		}
		rc = camera_io_contextid(&(a_ctrl->io_master_info), &trigger_data);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR, "Actuator[%d] Unable to fetch context id", a_ctrl->soc_info.index);
			return -EINVAL;
		}
		a_ctrl->cci_contextId = trigger_data.context_id;
		CAM_DBG(CAM_ACTUATOR, "idx:%d csid %d cid %d", trigger_data.context_id, trigger_data.csid, trigger_data.cid);
		return rc;
	} else {
		CAM_DBG(CAM_ACTUATOR, "crm is enable");
		return -EINVAL;
	}
}

static int32_t cam_actuator_i2c_modes_util(
	struct camera_io_master *io_master_info,
	struct i2c_settings_list *i2c_list)
{
	int32_t rc = 0;
	uint32_t i, size;

	if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_RANDOM) {
		rc = camera_io_dev_write(io_master_info,
			&(i2c_list->i2c_settings));
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed to random write I2C settings: %d",
				rc);
			return rc;
		}
	} else if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_SEQ) {
		rc = camera_io_dev_write_continuous(
			io_master_info,
			&(i2c_list->i2c_settings),
			CAM_SENSOR_I2C_WRITE_SEQ);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed to seq write I2C settings: %d",
				rc);
			return rc;
			}
	} else if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_BURST) {
		rc = camera_io_dev_write_continuous(
			io_master_info,
			&(i2c_list->i2c_settings),
			CAM_SENSOR_I2C_WRITE_BURST);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed to burst write I2C settings: %d",
				rc);
			return rc;
		}
	} else if (i2c_list->op_code == CAM_SENSOR_I2C_POLL) {
		size = i2c_list->i2c_settings.size;
		for (i = 0; i < size; i++) {
			rc = camera_io_dev_poll(
			io_master_info,
			i2c_list->i2c_settings.reg_setting[i].reg_addr,
			i2c_list->i2c_settings.reg_setting[i].reg_data,
			i2c_list->i2c_settings.reg_setting[i].data_mask,
			i2c_list->i2c_settings.addr_type,
			i2c_list->i2c_settings.data_type,
			i2c_list->i2c_settings.reg_setting[i].delay);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR,
					"i2c poll apply setting Fail: %d", rc);
				return rc;
			}
		}
	}

	return rc;
}

int32_t cam_actuator_slaveInfo_pkt_parser(struct cam_actuator_ctrl_t *a_ctrl,
	uint32_t *cmd_buf, size_t len)
{
	int32_t rc = 0;
	struct cam_cmd_i2c_info *i2c_info;

	if (!a_ctrl || !cmd_buf || (len < sizeof(struct cam_cmd_i2c_info))) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	i2c_info = (struct cam_cmd_i2c_info *)cmd_buf;
	if (a_ctrl->io_master_info.master_type == CCI_MASTER) {
		a_ctrl->io_master_info.cci_client->cci_i2c_master =
			a_ctrl->cci_i2c_master;
		a_ctrl->io_master_info.cci_client->i2c_freq_mode =
			i2c_info->i2c_freq_mode;
		a_ctrl->io_master_info.cci_client->sid =
			i2c_info->slave_addr >> 1;
		CAM_DBG(CAM_ACTUATOR, "Slave addr: 0x%x Freq Mode: %d",
			i2c_info->slave_addr, i2c_info->i2c_freq_mode);
	} else if (a_ctrl->io_master_info.master_type == I2C_MASTER) {
		a_ctrl->io_master_info.client->addr = i2c_info->slave_addr;
		CAM_DBG(CAM_ACTUATOR, "Slave addr: 0x%x", i2c_info->slave_addr);
	} else {
		CAM_ERR(CAM_ACTUATOR, "Invalid Master type: %d",
			a_ctrl->io_master_info.master_type);
		 rc = -EINVAL;
	}

	return rc;
}

static int32_t cam_actuator_event_modes_util(
	struct camera_io_master *io_master_info,
	struct cam_sensor_event_list *event_list,
	uint32_t context_id)
{
	int32_t rc = 0;

	rc = camera_io_dev_event_write(io_master_info,
		event_list, context_id);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR,
			"Failed to random write I2C settings: %d",
			rc);
	}
	return rc;
}


int32_t cam_actuator_apply_settings(struct cam_actuator_ctrl_t *a_ctrl,
	struct i2c_settings_array *i2c_set)
{
	struct i2c_settings_list *i2c_list;
	int32_t rc = 0;

	if (a_ctrl == NULL || i2c_set == NULL) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	if (i2c_set->is_settings_valid != 1) {
		CAM_ERR(CAM_ACTUATOR, " Invalid settings");
		return -EINVAL;
	}

	list_for_each_entry(i2c_list,
		&(i2c_set->list_head), list) {
		rc = cam_actuator_i2c_modes_util(
			&(a_ctrl->io_master_info),
			i2c_list);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed to apply settings: %d",
				rc);
		} else {
			CAM_DBG(CAM_ACTUATOR,
				"Success:request ID: %d",
				i2c_set->request_id);
		}
	}

	return rc;
}

int32_t cam_actuator_apply_request(struct cam_req_mgr_apply_request *apply)
{
	int32_t rc = 0, request_id, del_req_id;
	struct cam_actuator_ctrl_t *a_ctrl = NULL;

	if (!apply) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Input Args");
		return -EINVAL;
	}

	a_ctrl = (struct cam_actuator_ctrl_t *)
		cam_get_device_priv(apply->dev_hdl);
	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "Device data is NULL");
		return -EINVAL;
	}
	request_id = apply->request_id % MAX_PER_FRAME_ARRAY;

	trace_cam_apply_req("Actuator", a_ctrl->soc_info.index, apply->request_id, apply->link_hdl);

	CAM_DBG(CAM_ACTUATOR, "Request Id: %lld", apply->request_id);
	mutex_lock(&(a_ctrl->actuator_mutex));
	if ((apply->request_id ==
		a_ctrl->i2c_data.per_frame[request_id].request_id) &&
		(a_ctrl->i2c_data.per_frame[request_id].is_settings_valid)
		== 1) {
		rc = cam_actuator_apply_settings(a_ctrl,
			&a_ctrl->i2c_data.per_frame[request_id]);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed in applying the request: %lld\n",
				apply->request_id);
			goto release_mutex;
		}
	}
	del_req_id = (request_id +
		MAX_PER_FRAME_ARRAY - MAX_SYSTEM_PIPELINE_DELAY) %
		MAX_PER_FRAME_ARRAY;

	if (apply->request_id >
		a_ctrl->i2c_data.per_frame[del_req_id].request_id) {
		a_ctrl->i2c_data.per_frame[del_req_id].request_id = 0;
		rc = delete_request(&a_ctrl->i2c_data.per_frame[del_req_id]);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Fail deleting the req: %d err: %d\n",
				del_req_id, rc);
			goto release_mutex;
		}
	} else {
		CAM_DBG(CAM_ACTUATOR, "No Valid Req to clean Up");
	}

release_mutex:
	mutex_unlock(&(a_ctrl->actuator_mutex));
	return rc;
}

static int cam_actuator_delete_perframe_event_settings(struct cam_actuator_ctrl_t *a_ctrl,
	uint64_t req_id)
{
	int rc = 0;
	uint64_t top = 0, del_req_id = 0, i, j;
	struct cci_trigger_cam_setting_array *cci_set =
		a_ctrl->i2c_data.per_frame_event_settings;
	if (!cci_set) {
		CAM_ERR(CAM_ACTUATOR,
			"Invalid trigger setting array for req %lld", req_id);
		return -EINVAL;
	}
	/* Change the logic dynamically */
	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
		if ((req_id >=
			cci_set[i].request_id) &&
			(top <
			cci_set[i].request_id) &&
			(cci_set[i].is_settings_valid
				== 1)) {
			del_req_id = top;
			top = cci_set[i].request_id;
		}
	}

	if (top < req_id) {
		if ((((top % MAX_PER_FRAME_ARRAY) - (req_id %
			MAX_PER_FRAME_ARRAY)) >= BATCH_SIZE_MAX) ||
			(((top % MAX_PER_FRAME_ARRAY) - (req_id %
			MAX_PER_FRAME_ARRAY)) <= -BATCH_SIZE_MAX))
			del_req_id = req_id;
	}
	CAM_DBG(CAM_ACTUATOR, "top: %llu, del_req_id:%llu",
		top, del_req_id);
	if (!del_req_id)
		return rc;

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
		if ((del_req_id >
			cci_set[i].request_id) && (
			cci_set[i].is_settings_valid == 1)) {
			cci_set[i].request_id = 0;
			for (j = 0; j < MAX_CMD_BUFFER; j++) {
				struct i2c_settings_array *i2c_settings =
					&cci_set[i].event_data[j].trigger_sensor_cmd_buf_info.i2c_settings;
				if(cci_set[i].event_data[j].cmd_type ==
					CAM_SENSOR_CMD_TYPE_I2C_SETTING &&
					i2c_settings->is_settings_valid == 1) {
					rc = delete_i2c_event_settings(i2c_settings);
					if (rc < 0)
						CAM_ERR(CAM_ACTUATOR,
							"Delete request Fail:%lld rc:%d",
							del_req_id, rc);
				}
				memset(&cci_set[i].event_data[j], 0,
					sizeof(struct cam_sensor_per_frame_event_data));
			}
			cci_set[i].is_settings_valid = 0;
			memset(&cci_set[i].event_list, 0, sizeof(struct cam_sensor_event_list));
		}
	}
	return rc;
}

int cam_actuator_apply_event_settings(struct cam_actuator_ctrl_t *a_ctrl,
	uint64_t req_id)
{
	int rc = 0, offset;
	struct cam_sensor_event_list *event_list = NULL;
	struct cci_trigger_cam_setting_array *cci_set =
		a_ctrl->i2c_data.per_frame_event_settings;
	if (!cci_set) {
		CAM_ERR(CAM_ACTUATOR,
			"Invalid trigger setting array for req %lld", req_id);
		return -EINVAL;
	}

	offset = req_id % MAX_PER_FRAME_ARRAY;
	event_list = &cci_set[offset].event_list;
	if (!event_list) {
		CAM_ERR(CAM_ACTUATOR,
			"Invalid event list req %lld", req_id);
		return -EINVAL;
	}

	if ((cci_set[offset].is_settings_valid == 1) &&
			((cci_set[offset].request_id == req_id) ||
			 (cci_set[offset].setting_id == req_id))) {
		CAM_DBG(CAM_ACTUATOR, "reqid %d", req_id);
		rc = cam_actuator_event_modes_util(
				&(a_ctrl->io_master_info),
				event_list, a_ctrl->cci_contextId);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed to apply settings: %d",
				rc);
			return rc;
		}
	} else {
		CAM_ERR(CAM_ACTUATOR,
			"Invalid request to apply: %lld", req_id);
		return -EINVAL;
	}

	rc = cam_actuator_delete_perframe_event_settings(a_ctrl, req_id);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR,
			"slot[%d] req[%lld] failed clear previous req, err:%d",
			a_ctrl->soc_info.index,
			req_id, rc);
	}
	return rc;
}

int32_t cam_actuator_establish_link(
	struct cam_req_mgr_core_dev_link_setup *link)
{
	struct cam_actuator_ctrl_t *a_ctrl = NULL;

	if (!link) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	a_ctrl = (struct cam_actuator_ctrl_t *)
		cam_get_device_priv(link->dev_hdl);
	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "Device data is NULL");
		return -EINVAL;
	}

	mutex_lock(&(a_ctrl->actuator_mutex));
	if (link->link_enable) {
		a_ctrl->bridge_intf.link_hdl = link->link_hdl;
		a_ctrl->bridge_intf.crm_cb = link->crm_cb;
	} else {
		a_ctrl->bridge_intf.link_hdl = -1;
		a_ctrl->bridge_intf.crm_cb = NULL;
	}
	mutex_unlock(&(a_ctrl->actuator_mutex));

	return 0;
}

static int32_t cam_actuator_fill_event_data(
	struct cci_trigger_cam_setting_array *cci_settings)
{
	int32_t rc = 0;
	struct cam_sensor_event_list *event_list = NULL;
	struct i2c_settings_list *i2c_list = NULL;
	int32_t index, i, j;
	struct cam_sensor_per_frame_event_data *event_data = NULL;

	if (cci_settings == NULL) {
		CAM_ERR(CAM_ACTUATOR, "Trigger settings data is NULL");
		return -EINVAL;
	}
	event_list = &cci_settings->event_list;

	/* fill cmd buffer data to event_list, need to pass it to cci */
	for (i = 0; i < event_list->event_count; i++) {
		for (j = 0; j < event_list->event_info[i].event_arg_count; j++) {
			index = event_list->event_info[i].event_arg_sequence[j].index;
			if (index >= MAX_CMD_BUFFER && index < 0) {
				CAM_ERR(CAM_ACTUATOR, "Invalid event arg sequence index");
				return -EINVAL;
			}
			event_data = &cci_settings->event_data[index];

			event_list->event_info[i].event_arg_sequence[j].cmd_type =
				event_data->cmd_type;

			if (event_data->cmd_type == CAM_SENSOR_CMD_TYPE_I2C_SETTING) {
				// cmd buffer is of i2c setting type
				if (cci_settings->is_settings_valid == 1) {
					list_for_each_entry(i2c_list,
						&(event_data->trigger_sensor_cmd_buf_info.i2c_settings.list_head),
						list) {

						event_list->event_info[i].event_arg_sequence[j].payload =
							&(i2c_list->i2c_settings);
					}
				}
			} else {
				// cmd buffer is of blob type
				event_list->event_info[i].event_arg_sequence[j].payload =
					&event_data->trigger_sensor_cmd_buf_info;
			}
		}

		for (j = 0; j < event_list->event_info[i].cmd_count; j++) {
			index = event_list->event_info[i].cmd_sequence[j].index;
			if (index >= MAX_CMD_BUFFER && index < 0) {
				CAM_ERR(CAM_ACTUATOR, "Invalid cmd sequence index");
				return -EINVAL;
			}
			event_data = &cci_settings->event_data[index];

			event_list->event_info[i].cmd_sequence[j].cmd_type =
				event_data->cmd_type;

			if (event_data->cmd_type == CAM_SENSOR_CMD_TYPE_I2C_SETTING) {
				// cmd buffer is of i2c setting type
				if (cci_settings->is_settings_valid == 1) {
					list_for_each_entry(i2c_list,
						&(event_data->trigger_sensor_cmd_buf_info.i2c_settings.list_head),
						list) {

						event_list->event_info[i].cmd_sequence[j].payload =
							&(i2c_list->i2c_settings);
					}
				}
			} else {
				// cmd buffer is of blob type
				event_list->event_info[i].cmd_sequence[j].payload =
					&event_data->trigger_sensor_cmd_buf_info;
			}
		}
	}

	return rc;
}

static int cam_actuator_handle_event_info(
	struct cam_sensor_event_info *event_info,
	struct cam_actuator_ctrl_t *a_ctrl, uint64_t req_id)
{
	int rc = 0, offset, i = 0, j = 0, k = 0;
	struct cam_sensor_event_list *event_list = NULL;
	uint32_t event_offset = event_info->event_offset;

	if (!a_ctrl || !event_info) {
		CAM_ERR(CAM_ACTUATOR, "Invalid params: event_info: %s, a_ctrl: %s",
			CAM_IS_NULL_TO_STR(event_info),
			CAM_IS_NULL_TO_STR(a_ctrl));
		return -EINVAL;
	}

	if (event_info->version > 1) {
		CAM_ERR(CAM_ACTUATOR, "Invalid event_info version %d", event_info->version);
		return -EINVAL;
	}
	offset = req_id % MAX_PER_FRAME_ARRAY;
	/* preparing event list, need to send it to cci */
	event_list = &a_ctrl->i2c_data.per_frame_event_settings[offset].event_list;
	event_list->event_count = event_info->event_count;

	for (i = 0; i < event_list->event_count; i++) {
		uint32_t version = *(uint32_t *)((uint8_t *)event_info + event_offset);

		CAM_DBG(CAM_ACTUATOR, "offset version: %u", version);
		struct cam_sensor_events_v2 *event_sequence;
		event_sequence = (struct cam_sensor_events_v2 *)((uint8_t*)event_info + event_offset);
		event_list->event_info[i].event = event_sequence->event_name;
		event_list->event_info[i].cmd_count = event_sequence->cmd_count;
		event_list->event_info[i].event_arg_count = event_sequence->event_arg_count;
		event_list->event_info[i].event_flag = event_sequence->event_flag &
						CAM_SENSOR_CCI_CMD_EXEC_PARALLEL;

		CAM_DBG(CAM_ACTUATOR, "event_name %d event_arg_count %d cmd count %d",
					event_sequence->event_name,
					event_sequence->event_arg_count,
					event_sequence->cmd_count);

		uint32_t *event_arg_sequence = (uint32_t *)((uint8_t *)event_sequence +
							event_sequence->event_arg_offset);

		for (j = 0; j < event_sequence->event_arg_count; j++) {
			event_list->event_info[i].event_arg_sequence[j].index =
				event_arg_sequence[j];
			CAM_DBG(CAM_ACTUATOR, "event_arg_sequence %d",
				event_arg_sequence[j]);
		}

		uint32_t *cmd_flag = (uint32_t *)((uint8_t *)event_sequence +
						event_sequence->cmd_flag_offset);
		uint32_t *cmd_sequence = (uint32_t *)((uint8_t *)event_sequence +
						event_sequence->cmd_sequence_offset);

		for (k = 0; k < event_sequence->cmd_count; k++) {
			event_list->event_info[i].cmd_sequence[k].index =
				cmd_sequence[k];
			event_list->event_info[i].cmd_sequence[k].cmd_flag =
				cmd_flag[k] &
				CAM_SENSOR_CCI_CMD_EXEC_PARALLEL;
			CAM_DBG(CAM_ACTUATOR, "cmd_sequence %d",
				cmd_sequence[k]);
		}
		event_offset += sizeof(struct cam_sensor_events_v2) +
			(event_sequence->event_arg_count * sizeof(uint32_t)) +
			(event_sequence->cmd_count * sizeof(uint32_t)) * 2;
	}

	return rc;
}

static int cam_actuator_handle_event_controlled_info(
	struct cam_actuator_event_control_info *event_control_info,
	struct cam_actuator_ctrl_t *a_ctrl)
{
	int rc = 0;

	if (!a_ctrl || !event_control_info) {
		CAM_ERR(CAM_ACTUATOR, "Invalid params: event_control_info: %s, a_ctrl: %s",
			CAM_IS_NULL_TO_STR(event_control_info),
			CAM_IS_NULL_TO_STR(a_ctrl));
		return -EINVAL;
	}
	a_ctrl->is_precise_actuator_control = true;
	a_ctrl->frame_event.frame_event = event_control_info->event_name;
	a_ctrl->actuator_trigger_data.line_no = event_control_info->value;
	CAM_DBG(CAM_ACTUATOR,
		"actuator[%s] enable_event_controlled %d event_name %d value %d",
		a_ctrl->device_name,
		event_control_info->enable_event_controlled,
		event_control_info->event_name,
		event_control_info->value);

	return rc;
}

static int cam_actuator_handle_frame_event_info(
	struct cam_sensor_frame_event_info *frame_event_info,
	struct cam_actuator_ctrl_t *a_ctrl, uint64_t req_id,
	struct cam_sensor_per_frame_event_data *event_data)
{
	int rc = 0;
	if (!a_ctrl || !frame_event_info || !event_data) {
		CAM_ERR(CAM_ACTUATOR, "Invalid params: frame_event_info: %s, a_ctrl: %s",
			CAM_IS_NULL_TO_STR(frame_event_info),
			CAM_IS_NULL_TO_STR(a_ctrl),
			CAM_IS_NULL_TO_STR(event_data));
		return -EINVAL;
	}

	if (frame_event_info->version > 1) {
		CAM_ERR(CAM_ACTUATOR, "Invalid frame_event_info version %d",
						frame_event_info->version);
		return -EINVAL;
	}
	event_data->trigger_sensor_cmd_buf_info.frame_event_info.streamId =
		frame_event_info->stream_id;
	event_data->trigger_sensor_cmd_buf_info.frame_event_info.vc =
		frame_event_info->vc;
	event_data->trigger_sensor_cmd_buf_info.frame_event_info.dt =
		frame_event_info->dt;
	event_data->trigger_sensor_cmd_buf_info.frame_event_info.frame_event =
		frame_event_info->frame_event;
	event_data->trigger_sensor_cmd_buf_info.frame_event_info.line_no =
		a_ctrl->actuator_trigger_data.line_no;
	event_data->cmd_type = CAM_SENSOR_CMD_TYPE_FRAME_EVENT;


	/* If request id is 0, it will be during an initial config/acquire */
	CAM_DBG(CAM_ACTUATOR,
		"actuator[%s] reqId: %llu frame_event %d vc %d dt %d width %d height %d lineno %d",
		a_ctrl->device_name,
		req_id,
		frame_event_info->frame_event, frame_event_info->vc,
		frame_event_info->dt, frame_event_info->width, frame_event_info->height,
		a_ctrl->actuator_trigger_data.line_no);
	return rc;
}

static int32_t cam_actuator_generic_blob_handler(void *user_data,
	uint32_t blob_type, uint32_t blob_size, uint8_t *blob_data)
{
	int rc = 0;
	uint64_t req_id = 0;
	struct actuator_userdata *a_userdata = (struct actuator_userdata *)user_data;
	struct cam_actuator_ctrl_t *a_ctrl = NULL;
	struct cam_sensor_per_frame_event_data *event_data = NULL;

	if(!a_userdata) {
		CAM_ERR(CAM_ACTUATOR, "userdata is NULL");
		return -EINVAL;
	}

	req_id = a_userdata->reqid;
	a_ctrl = a_userdata->actuator_ctrl;

	if (!blob_data || !blob_size) {
		CAM_ERR(CAM_ACTUATOR, "Invalid blob info %pK %u", blob_data,
			blob_size);
		return -EINVAL;
	}

	switch (blob_type) {
	case CAM_ACTUATOR_GENERIC_BLOB_FRAME_EVENT_INFO: {
		// Frame Event blob type cmd buffer
		struct cam_sensor_frame_event_info *frame_event_info =
			(struct cam_sensor_frame_event_info *) blob_data;
		event_data = a_userdata->event_data;

		if (blob_size < sizeof(struct cam_sensor_frame_event_info)) {
			CAM_ERR(CAM_ACTUATOR, "Invalid blob size expected: 0x%x actual: 0x%x",
				sizeof(struct cam_sensor_frame_event_info), blob_size);
			return -EINVAL;
		}

		rc = cam_actuator_handle_frame_event_info(frame_event_info, a_ctrl, req_id, event_data);
		break;
	}
	case CAM_ACTUATOR_GENERIC_BLOB_EVENT_CMD_INFO: {
		// Frame Event blob type cmd buffer
		struct cam_actuator_event_control_info *event_controlled_info =
			(struct cam_actuator_event_control_info *) blob_data;

		if (blob_size < sizeof(struct cam_actuator_event_control_info)) {
			CAM_ERR(CAM_ACTUATOR, "Invalid blob size expected: 0x%x actual: 0x%x",
				sizeof(struct cam_actuator_event_control_info), blob_size);
			return -EINVAL;
		}

		rc = cam_actuator_handle_event_controlled_info(event_controlled_info, a_ctrl);
		break;
	}
	case CAM_ACTUATOR_GENERIC_BLOB_EVENT_INFO: {
		// Event data blob type cmd buffer
		struct cam_sensor_event_info *event_info =
			(struct cam_sensor_event_info *) blob_data;
		if (blob_size < sizeof(struct cam_sensor_event_info)) {
			CAM_ERR(CAM_ACTUATOR, "Invalid blob size expected: 0x%x actual: 0x%x",
				sizeof(struct cam_sensor_event_info), blob_size);
			return -EINVAL;
		}
		rc = cam_actuator_handle_event_info(event_info, a_ctrl, req_id);
		break;
	}
	default:
		CAM_WARN(CAM_ACTUATOR, "Invalid blob type %d", blob_type);
		break;
	}

	return rc;
}

static int cam_actuator_update_req_mgr(
	struct cam_actuator_ctrl_t *a_ctrl,
	struct cam_packet *csl_packet)
{
	int rc = 0;
	struct cam_req_mgr_add_request add_req;

	if (a_ctrl->bridge_intf.enable_crm != 1) {
		return rc;
	}

	memset(&add_req, 0, sizeof(add_req));
	add_req.link_hdl = a_ctrl->bridge_intf.link_hdl;
	add_req.req_id = csl_packet->header.request_id;
	add_req.dev_hdl = a_ctrl->bridge_intf.device_hdl;

	if (a_ctrl->bridge_intf.crm_cb &&
		a_ctrl->bridge_intf.crm_cb->add_req) {
		rc = a_ctrl->bridge_intf.crm_cb->add_req(&add_req);
		if (rc) {
			CAM_ERR(CAM_ACTUATOR,
				"Adding request: %llu failed: rc: %d",
				csl_packet->header.request_id, rc);
			return rc;
		}
		CAM_DBG(CAM_ACTUATOR, "Request Id: %lld added to CRM",
			add_req.req_id);
	} else {
		CAM_ERR(CAM_ACTUATOR, "Can't add Request ID: %lld to CRM",
			csl_packet->header.request_id);
		rc = -EINVAL;
	}

	return rc;
}

int32_t cam_actuator_publish_dev_info(struct cam_req_mgr_device_info *info)
{
	if (!info) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	info->dev_id = CAM_REQ_MGR_DEVICE_ACTUATOR;
	strscpy(info->name, CAM_ACTUATOR_NAME, sizeof(info->name));
	info->p_delay = 1;
	info->trigger = CAM_TRIGGER_POINT_SOF;

	return 0;
}

int32_t cam_actuator_cmd_buffer(struct cam_actuator_ctrl_t *a_ctrl,
	struct cci_trigger_cam_setting_array *cci_settings,
	struct cam_cmd_buf_desc *cmd_desc,
	struct cam_packet *csl_packet,
	int32_t i)
{
	int32_t rc = 0;

	switch(cmd_desc->type) {
		case CAM_CMD_BUF_I2C: {
			struct i2c_settings_array *i2c_settings =
				&cci_settings->event_data[i].trigger_sensor_cmd_buf_info.i2c_settings;

			CAM_DBG(CAM_ACTUATOR, "reqid  %d ", csl_packet->header.request_id);
			cci_settings->event_data[i].cmd_type =
				CAM_SENSOR_CMD_TYPE_I2C_SETTING;
			INIT_LIST_HEAD(&(
				i2c_settings->list_head));

			rc = cam_sensor_i2c_command_parser(&a_ctrl->io_master_info,
				i2c_settings, cmd_desc, 1, NULL);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR, "Fail parsing I2C Pkt: %d", rc);
				cci_settings->is_settings_valid = 0;
				return rc;
			}

			cci_settings->is_settings_valid =
				i2c_settings->is_settings_valid;

			break;
		}
		case CAM_CMD_BUF_GENERIC: {
			struct actuator_userdata a_userdata = {0};
			struct cam_sensor_per_frame_event_data *event_data = NULL;
			event_data = &cci_settings->event_data[i];
			a_userdata.actuator_ctrl = a_ctrl;
			a_userdata.reqid = csl_packet->header.request_id;
			a_userdata.event_data = event_data;
			rc = cam_packet_util_process_generic_cmd_buffer(cmd_desc,
				cam_actuator_generic_blob_handler, &a_userdata);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR, "Processing Generic Blob Handler Failure");
				cci_settings->is_settings_valid = 0;
				return rc;
			}
			cci_settings->is_settings_valid = 1;
			break;
		}
	}
	return rc;
}

int32_t cam_actuator_move_lens(struct cam_actuator_ctrl_t *a_ctrl,
	struct cci_trigger_cam_setting_array *cci_settings,
	struct cam_packet *csl_packet)
{
	int32_t rc = 0;
	int32_t  i = 0;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	uint32_t *offset = NULL;

	if (a_ctrl->cci_contextId == CONTEXT_ID_MAX) {
		rc = cam_actuator_get_cci_contextid(a_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR, "Invalid context id");
				return rc;
			}
	}

	offset = (uint32_t *)&csl_packet->payload;
	offset += (csl_packet->cmd_buf_offset / sizeof(uint32_t));
	CAM_ERR(CAM_ACTUATOR, "num_cmd_buf %d", csl_packet->num_cmd_buf);
	for (i = 0; i < csl_packet->num_cmd_buf; i++) {
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);
		rc = cam_actuator_cmd_buffer(a_ctrl, cci_settings, cmd_desc, csl_packet, i);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR, "Cmd Buffer Fail: %d", rc);
			cci_settings->is_settings_valid = 0;
			return rc;
		}
		offset += (sizeof(struct cam_cmd_buf_desc)/4);
	}

	cci_settings->request_id = csl_packet->header.request_id;
	rc = cam_actuator_fill_event_data(cci_settings);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "Fail parsing event Pkt: %d", rc);
		cci_settings->is_settings_valid = 0;
		return rc;
	}

	return rc;
}

int32_t cam_actuator_i2c_pkt_parse(struct cam_actuator_ctrl_t *a_ctrl,
	void *arg)
{
	int32_t  rc = 0;
	int32_t  i = 0;
	uint32_t total_cmd_buf_in_bytes = 0;
	size_t   len_of_buff = 0;
	size_t   remain_len = 0;
	uint32_t *offset = NULL;
	uint32_t *cmd_buf = NULL;
	uintptr_t generic_ptr;
	uintptr_t generic_pkt_ptr;
	struct common_header      *cmm_hdr = NULL;
	struct cam_control        *ioctl_ctrl = NULL;
	struct cam_packet         *csl_packet = NULL;
	struct cam_packet         *csl_packet_u = NULL;
	struct cam_config_dev_cmd config;
	struct i2c_data_settings  *i2c_data = NULL;
	struct i2c_settings_array *i2c_reg_settings = NULL;
	struct cam_cmd_buf_desc   *cmd_desc = NULL;
	struct cam_actuator_soc_private *soc_private = NULL;
	struct cam_sensor_power_ctrl_t  *power_info = NULL;
	struct cci_trigger_cam_setting_array *cci_settings = NULL;

	if (!a_ctrl || !arg) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;

	power_info = &soc_private->power_info;

	ioctl_ctrl = (struct cam_control *)arg;
	if (copy_from_user(&config,
		u64_to_user_ptr(ioctl_ctrl->handle),
		sizeof(config)))
		return -EFAULT;
	rc = cam_mem_get_cpu_buf(config.packet_handle,
		&generic_pkt_ptr, &len_of_buff);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "Error in converting command Handle %d",
			rc);
		return rc;
	}

	remain_len = len_of_buff;
	if ((sizeof(struct cam_packet) > len_of_buff) ||
		((size_t)config.offset >= len_of_buff -
		sizeof(struct cam_packet))) {
		CAM_ERR(CAM_ACTUATOR,
			"Inval cam_packet strut size: %zu, len_of_buff: %zu",
			 sizeof(struct cam_packet), len_of_buff);
		rc = -EINVAL;
		goto put_buf;
	}

	remain_len -= (size_t)config.offset;
	csl_packet_u = (struct cam_packet *)
		(generic_pkt_ptr + (uint32_t)config.offset);
	rc = cam_packet_util_copy_pkt_to_kmd(csl_packet_u, &csl_packet, remain_len);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR, "Copying packet to KMD failed");
		goto end;
	}

	CAM_DBG(CAM_ACTUATOR, "Pkt opcode: %d",	csl_packet->header.op_code);

	if ((csl_packet->header.op_code & 0xFFFFFF) !=
		CAM_ACTUATOR_PACKET_OPCODE_INIT &&
		csl_packet->header.request_id <= a_ctrl->last_flush_req
		&& a_ctrl->last_flush_req != 0) {
		CAM_DBG(CAM_ACTUATOR,
			"reject request %lld, last request to flush %lld",
			csl_packet->header.request_id, a_ctrl->last_flush_req);
		rc = -EINVAL;
		goto end;
	}

	if (csl_packet->header.request_id > a_ctrl->last_flush_req)
		a_ctrl->last_flush_req = 0;

	switch (csl_packet->header.op_code & 0xFFFFFF) {
	case CAM_ACTUATOR_PACKET_OPCODE_INIT:
		offset = (uint32_t *)&csl_packet->payload_flex;
		offset += (csl_packet->cmd_buf_offset / sizeof(uint32_t));
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);
		if (a_ctrl->is_precise_actuator_control &&
				a_ctrl->cam_act_state == CAM_ACTUATOR_START) {
			i2c_data = &(a_ctrl->i2c_data);
			cci_settings =
				&i2c_data->per_frame_event_settings[csl_packet->header.request_id %
						MAX_PER_FRAME_ARRAY];
		}
		/* Loop through multiple command buffers */
		CAM_DBG(CAM_ACTUATOR, "num cmd buf %d", csl_packet->num_cmd_buf);

		for (i = 0; i < csl_packet->num_cmd_buf; i++) {
			rc = cam_packet_util_validate_cmd_desc(&cmd_desc[i]);
			if (rc)
				goto end;

			total_cmd_buf_in_bytes = cmd_desc[i].length;
			if (!total_cmd_buf_in_bytes)
				continue;
			rc = cam_mem_get_cpu_buf(cmd_desc[i].mem_handle,
					&generic_ptr, &len_of_buff);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR, "Failed to get cpu buf");
				goto end;
			}
			cmd_buf = (uint32_t *)generic_ptr;
			if (!cmd_buf) {
				CAM_ERR(CAM_ACTUATOR, "invalid cmd buf");
				rc = -EINVAL;
				goto end;
			}
			if ((len_of_buff < sizeof(struct common_header)) ||
				(cmd_desc[i].offset > (len_of_buff -
				sizeof(struct common_header)))) {
				CAM_ERR(CAM_ACTUATOR,
					"Invalid length for sensor cmd");
				rc = -EINVAL;
				goto end;
			}
			remain_len = len_of_buff - cmd_desc[i].offset;
			cmd_buf += cmd_desc[i].offset / sizeof(uint32_t);
			cmm_hdr = (struct common_header *)cmd_buf;

			CAM_DBG(CAM_ACTUATOR, "cmdhdr_type: %d cmd_type: %d",
								cmm_hdr->cmd_type, cmd_desc[i].type);
			switch (cmm_hdr->cmd_type) {
			case CAMERA_SENSOR_CMD_TYPE_I2C_INFO:
				CAM_DBG(CAM_ACTUATOR,
					"Received slave info buffer");
				a_ctrl->is_precise_actuator_control = false;
				rc = cam_actuator_slaveInfo_pkt_parser(
					a_ctrl, cmd_buf, remain_len);
				if (rc < 0) {
					CAM_ERR(CAM_ACTUATOR,
					"Failed to parse slave info: %d", rc);
					goto end;
				}
				break;
			case CAMERA_SENSOR_CMD_TYPE_PWR_UP:
			case CAMERA_SENSOR_CMD_TYPE_PWR_DOWN:
				CAM_DBG(CAM_ACTUATOR,
					"Received power settings buffer");
				rc = cam_sensor_update_power_settings(
					cmd_buf,
					total_cmd_buf_in_bytes,
					power_info, remain_len);
				if (rc) {
					CAM_ERR(CAM_ACTUATOR,
					"Failed:parse power settings: %d",
					rc);
					goto end;
				}
				break;
			default:
				if (a_ctrl->is_precise_actuator_control &&
						a_ctrl->cam_act_state == CAM_ACTUATOR_START) {
					if (a_ctrl->cci_contextId == CONTEXT_ID_MAX) {
						rc = cam_actuator_get_cci_contextid(a_ctrl);
						if (rc < 0) {
							CAM_ERR(CAM_ACTUATOR, "Invalid context id");
							return rc;
						}
					}
					rc = cam_actuator_cmd_buffer(a_ctrl, cci_settings, &cmd_desc[i], csl_packet, i);
					if (rc < 0) {
						CAM_ERR(CAM_ACTUATOR, "Cmd Buffer Fail: %d", rc);
						cci_settings->is_settings_valid = 0;
						return rc;
					}

				} else {
					switch(cmd_desc[i].type) {
					case CAM_CMD_BUF_I2C: {
						CAM_DBG(CAM_ACTUATOR,
							"Received initSettings buffer req %lld", csl_packet->header.request_id);
						i2c_data = &(a_ctrl->i2c_data);
						i2c_reg_settings =
							&i2c_data->init_settings;

						i2c_reg_settings->request_id = 0;
						i2c_reg_settings->is_settings_valid = 1;
						rc = cam_sensor_i2c_command_parser(
							&a_ctrl->io_master_info,
							i2c_reg_settings,
							&cmd_desc[i], 1, NULL);
						if (rc < 0) {
							CAM_ERR(CAM_ACTUATOR,
							"Failed:parse init settings: %d",
							rc);
							goto end;
						}
						break;
					}
					case CAM_CMD_BUF_GENERIC: {
						struct actuator_userdata a_userdata = {0};
						a_userdata.actuator_ctrl = a_ctrl;
						a_userdata.reqid = csl_packet->header.request_id;
						rc = cam_packet_util_process_generic_cmd_buffer(&cmd_desc[i],
							cam_actuator_generic_blob_handler, &a_userdata);
						if (rc) {
							CAM_ERR(CAM_ACTUATOR, "Processing Generic Blob Handler Failure");
							return rc;
						}
						break;
					}
					}
				}
			}
			cam_mem_put_cpu_buf(cmd_desc[i].mem_handle);
		}
		if (a_ctrl->is_precise_actuator_control &&
				a_ctrl->cam_act_state == CAM_ACTUATOR_START) {
			cci_settings->request_id = csl_packet->header.request_id;
			rc = cam_actuator_fill_event_data(cci_settings);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR, "Fail parsing event Pkt: %d", rc);
				cci_settings->is_settings_valid = 0;
				return rc;
			}

			if (cci_settings[csl_packet->header.request_id % MAX_PER_FRAME_ARRAY].is_settings_valid) {
				rc = cam_actuator_apply_event_settings(a_ctrl,
							csl_packet->header.request_id);
				if (!rc) {
					CAM_DBG(CAM_ACTUATOR, "slot[%d] apply[%llu]",
									a_ctrl->soc_info.index,
									csl_packet->header.request_id);
				}
			}
		} else {
			if (a_ctrl->cam_act_state == CAM_ACTUATOR_ACQUIRE) {
				rc = cam_actuator_power_up(a_ctrl);
				if (rc < 0) {
					CAM_ERR(CAM_ACTUATOR,
						" Actuator Power up failed");
					goto end;
				}
				a_ctrl->cam_act_state = CAM_ACTUATOR_CONFIG;
			}

			rc = cam_actuator_apply_settings(a_ctrl,
				&a_ctrl->i2c_data.init_settings);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR, "Cannot apply Init settings");
				goto end;
			}

			/* Delete the request even if the apply is failed */
			rc = delete_request(&a_ctrl->i2c_data.init_settings);
			if (rc < 0) {
				CAM_WARN(CAM_ACTUATOR,
					"Fail in deleting the Init settings");
				rc = 0;
			}
		}
		break;
	case CAM_ACTUATOR_PACKET_AUTO_MOVE_LENS:
		CAM_DBG(CAM_ACTUATOR, "Auto move lens, req id %lld", csl_packet->header.request_id);
		if (a_ctrl->cam_act_state < CAM_ACTUATOR_CONFIG) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
				"Not in right state to move lens: %d",
				a_ctrl->cam_act_state);
			goto end;
		}
		a_ctrl->setting_apply_state = ACT_APPLY_SETTINGS_NOW;
		i2c_data = &(a_ctrl->i2c_data);
		if (a_ctrl->is_precise_actuator_control) {
			cci_settings =
				&i2c_data->per_frame_event_settings[csl_packet->header.request_id %
						MAX_PER_FRAME_ARRAY];
			rc = cam_actuator_move_lens(a_ctrl, cci_settings, csl_packet);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR, "Move Lens Cmd Buffer Fail: %d", rc);
				return rc;
			}

			if (cci_settings[csl_packet->header.request_id %
						MAX_PER_FRAME_ARRAY].is_settings_valid) {
				rc = cam_actuator_apply_event_settings(a_ctrl,
							csl_packet->header.request_id);
				if (!rc) {
					CAM_DBG(CAM_ACTUATOR, "slot[%d] apply[%llu]",
									a_ctrl->soc_info.index,
									csl_packet->header.request_id);
				}
			}
		} else {
			i2c_reg_settings = &i2c_data->init_settings;

			i2c_data->init_settings.request_id =
				csl_packet->header.request_id;
			i2c_reg_settings->is_settings_valid = 1;
			offset = (uint32_t *)&csl_packet->payload_flex;
			offset += csl_packet->cmd_buf_offset / sizeof(uint32_t);
			cmd_desc = (struct cam_cmd_buf_desc *)(offset);
			rc = cam_sensor_i2c_command_parser(
				&a_ctrl->io_master_info,
				i2c_reg_settings,
				cmd_desc, 1, NULL);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR,
					"Auto move lens parsing failed: %d", rc);
				goto end;
			}
			rc = cam_actuator_update_req_mgr(a_ctrl, csl_packet);
			if (rc) {
				CAM_ERR(CAM_ACTUATOR,
					"Failed in adding request to request manager");
				goto end;
			}
		}
		break;
	case CAM_ACTUATOR_PACKET_MANUAL_MOVE_LENS:
		CAM_INFO(CAM_ACTUATOR, "Manual move lens, req id %lld", csl_packet->header.request_id);
		if (a_ctrl->cam_act_state < CAM_ACTUATOR_CONFIG) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
				"Not in right state to move lens: %d",
				a_ctrl->cam_act_state);
			goto end;
		}
		a_ctrl->setting_apply_state = ACT_APPLY_SETTINGS_LATER;
		i2c_data = &(a_ctrl->i2c_data);
		if (a_ctrl->is_precise_actuator_control) {
			cci_settings =
				&i2c_data->per_frame_event_settings[csl_packet->header.request_id %
						MAX_PER_FRAME_ARRAY];
			rc = cam_actuator_move_lens(a_ctrl, cci_settings, csl_packet);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR, "Move Lens Cmd Buffer Fail: %d", rc);
				return rc;
			}
		} else {
			i2c_reg_settings = &i2c_data->per_frame[
				csl_packet->header.request_id % MAX_PER_FRAME_ARRAY];

			 i2c_reg_settings->request_id =
				csl_packet->header.request_id;
			i2c_reg_settings->is_settings_valid = 1;
			offset = (uint32_t *)&csl_packet->payload_flex;
			offset += csl_packet->cmd_buf_offset / sizeof(uint32_t);
			cmd_desc = (struct cam_cmd_buf_desc *)(offset);
			rc = cam_sensor_i2c_command_parser(
				&a_ctrl->io_master_info,
				i2c_reg_settings,
				cmd_desc, 1, NULL);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR,
					"Manual move lens parsing failed: %d", rc);
				goto end;
			}

			rc = cam_actuator_update_req_mgr(a_ctrl, csl_packet);
			if (rc) {
				CAM_ERR(CAM_ACTUATOR,
					"Failed in adding request to request manager");
				goto end;
			}
		}
		break;
	case CAM_PKT_NOP_OPCODE:
		CAM_DBG(CAM_ACTUATOR, "NOP packet, req %lld", csl_packet->header.request_id);
		if (a_ctrl->cam_act_state < CAM_ACTUATOR_CONFIG) {
			CAM_WARN(CAM_ACTUATOR,
				"Received NOP packets in invalid state: %d",
				a_ctrl->cam_act_state);
			rc = -EINVAL;
			goto end;
		}
		rc = cam_actuator_update_req_mgr(a_ctrl, csl_packet);
		if (rc) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed in adding request to request manager");
			goto end;
		}
		break;
	case CAM_ACTUATOR_PACKET_OPCODE_READ: {
		struct cam_buf_io_cfg *io_cfg;
		struct i2c_settings_array i2c_read_settings;

		if (a_ctrl->cam_act_state < CAM_ACTUATOR_CONFIG) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
				"Not in right state to read actuator: %d",
				a_ctrl->cam_act_state);
			goto end;
		}
		CAM_DBG(CAM_ACTUATOR, "number of I/O configs: %d:",
			csl_packet->num_io_configs);
		if (csl_packet->num_io_configs == 0) {
			CAM_ERR(CAM_ACTUATOR, "No I/O configs to process");
			rc = -EINVAL;
			goto end;
		}

		INIT_LIST_HEAD(&(i2c_read_settings.list_head));

		io_cfg = (struct cam_buf_io_cfg *) ((uint8_t *)
			&csl_packet->payload_flex +
			csl_packet->io_configs_offset);

		if (io_cfg == NULL) {
			CAM_ERR(CAM_ACTUATOR, "I/O config is invalid(NULL)");
			rc = -EINVAL;
			goto end;
		}

		offset = (uint32_t *)&csl_packet->payload_flex;
		offset += (csl_packet->cmd_buf_offset / sizeof(uint32_t));
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);
		i2c_read_settings.is_settings_valid = 1;
		i2c_read_settings.request_id = 0;
		rc = cam_sensor_i2c_command_parser(&a_ctrl->io_master_info,
			&i2c_read_settings,
			cmd_desc, 1, io_cfg);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"actuator read pkt parsing failed: %d", rc);
			goto end;
		}

		rc = cam_sensor_i2c_read_data(
			&i2c_read_settings,
			&a_ctrl->io_master_info);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR, "cannot read data, rc:%d", rc);
			delete_request(&i2c_read_settings);
			goto end;
		}

		rc = delete_request(&i2c_read_settings);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed in deleting the read settings");
			goto end;
		}
		break;
		}
	default:
		CAM_ERR(CAM_ACTUATOR, "Wrong Opcode: %d",
			csl_packet->header.op_code & 0xFFFFFF);
		rc = -EINVAL;
		goto end;
	}

end:
	cam_common_mem_free(csl_packet);
put_buf:
	cam_mem_put_cpu_buf(config.packet_handle);
	return rc;
}

void cam_actuator_shutdown(struct cam_actuator_ctrl_t *a_ctrl)
{
	int rc = 0, i, j;
	struct cam_actuator_soc_private  *soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;
	struct cam_sensor_power_ctrl_t *power_info =
		&soc_private->power_info;
	struct cci_trigger_cam_setting_array *cci_set = NULL;

	if (a_ctrl->cam_act_state == CAM_ACTUATOR_INIT)
		return;

	if (a_ctrl->io_master_info.master_type == CCI_MASTER) {
		if (a_ctrl->is_precise_actuator_control) {
			if (a_ctrl->cci_contextId < CONTEXT_ID_MAX) {
				rc = camera_io_contextid_release(&(a_ctrl->io_master_info),
						a_ctrl->cci_contextId, FALSE);
				if (rc < 0) {
					CAM_ERR(CAM_ACTUATOR, "Shutdown[%d] contextid release failed",
						a_ctrl->soc_info.index);
					return;
				}
			}
		}
	}

	if (a_ctrl->i2c_data.per_frame_event_settings != NULL) {
		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			cci_set = &(a_ctrl->i2c_data.per_frame_event_settings[i]);
			for (j = 0; j < MAX_CMD_BUFFER; j++) {
				// delete request for trigger sensor
				struct i2c_settings_array *i2c_settings =
					&cci_set->event_data[j].trigger_sensor_cmd_buf_info.i2c_settings;
				if (cci_set->event_data[j].cmd_type ==
					CAM_SENSOR_CMD_TYPE_I2C_SETTING &&
					i2c_settings->is_settings_valid == 1) {
					rc = delete_i2c_event_settings(
						i2c_settings);
					if (rc < 0)
						CAM_ERR(CAM_ACTUATOR,
							"delete request rc: %d", rc);
				}
			}
			cci_set->is_settings_valid = 0;
		}
	}

	if (a_ctrl->cam_act_state >= CAM_ACTUATOR_CONFIG) {
		rc = cam_actuator_power_down(a_ctrl);
		if (rc < 0)
			CAM_ERR(CAM_ACTUATOR, "Actuator Power down failed");
		a_ctrl->cam_act_state = CAM_ACTUATOR_ACQUIRE;
	}

	if (a_ctrl->cam_act_state >= CAM_ACTUATOR_ACQUIRE) {
		rc = cam_destroy_device_hdl(a_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_ACTUATOR, "destroying  dhdl failed");
		a_ctrl->bridge_intf.device_hdl = -1;
		a_ctrl->bridge_intf.link_hdl = -1;
		a_ctrl->bridge_intf.session_hdl = -1;
	}

	kfree(power_info->power_setting);
	kfree(power_info->power_down_setting);
	power_info->power_setting = NULL;
	power_info->power_down_setting = NULL;
	power_info->power_setting_size = 0;
	power_info->power_down_setting_size = 0;
	a_ctrl->last_flush_req = 0;

	a_ctrl->cam_act_state = CAM_ACTUATOR_INIT;
	CAM_DBG(CAM_ACTUATOR, "cam_actuator_shutdown success");
}

int32_t cam_actuator_no_crm_handshake(
	struct cam_req_mgr_no_crm_handshake_data *info)
{
	int32_t rc = 0;
	struct cam_actuator_ctrl_t *a_ctrl = NULL;

	if (!info) {
		CAM_ERR(CAM_ACTUATOR, "handshake data: NULL");
		return -EINVAL;
	}

	a_ctrl = (struct cam_actuator_ctrl_t *)
				cam_get_device_no_crm_priv(info->dev_hdl);

	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "Device data is NULL");
		return -EINVAL;
	}
	info->trigger = CAM_TRIGGER_POINT_SOF;
	a_ctrl->anchor_pd = info->anchor_pd;
	info->pipeline_delay = CAM_PIPELINE_DELAY_1;

	return rc;
}

int32_t cam_actuator_no_crm_apply_req_lock(
	struct cam_actuator_ctrl_t *a_ctrl,
	struct cam_req_mgr_no_crm_apply_request *apply)
{
	int32_t  rc = 0, request_id, del_req_id;
	uint64_t isp_req_id = 0;
	uint32_t isp_pd = 0;
	uint64_t actuator_req_id = 0;
	int      actuator_pd = 0;

	isp_req_id = apply->anchor_req_id;
	actuator_pd = 1;
	isp_pd = a_ctrl->anchor_pd;
	if (actuator_pd < isp_pd)
	{
		actuator_pd = isp_pd;
	}
	actuator_req_id = isp_req_id + (actuator_pd - isp_pd);

	request_id = actuator_req_id % MAX_PER_FRAME_ARRAY;

	trace_cam_apply_req("Actuator", a_ctrl->soc_info.index, actuator_req_id, apply->link_hdl);
	if (a_ctrl->is_precise_actuator_control &&
			(a_ctrl->setting_apply_state == ACT_APPLY_SETTINGS_LATER)) {
		struct cci_trigger_cam_setting_array *cci_set =
			a_ctrl->i2c_data.per_frame_event_settings;
		if (cci_set != NULL) {
			CAM_DBG(CAM_ACTUATOR, "set valid %d reqid %d request_id %d",
					cci_set[request_id].is_settings_valid,
					cci_set[request_id].request_id, request_id);

			if (cci_set[request_id].is_settings_valid) {
				if (cci_set[request_id].request_id != actuator_req_id) {
					CAM_INFO(CAM_ACTUATOR,
						"slot[%d] RequestId[%d] not in queue ",
						a_ctrl->soc_info.index,
						actuator_req_id);
				} else {
					rc = cam_actuator_apply_event_settings(a_ctrl,
							actuator_req_id);
					if (!rc) {
						CAM_DBG(CAM_ACTUATOR, "slot[%d] apply[%llu]",
								a_ctrl->soc_info.index,
								actuator_req_id);
					}
				}
			} else {
				CAM_DBG(CAM_ACTUATOR, "No Valid Req to clean Up");
			}
		}
	} else {
		CAM_DBG(CAM_ACTUATOR, "Request: %lld index %d recved %lld  valid %d", actuator_req_id, request_id,
				a_ctrl->i2c_data.per_frame[request_id].request_id,
				a_ctrl->i2c_data.per_frame[request_id].is_settings_valid);
		if ((actuator_req_id ==
			a_ctrl->i2c_data.per_frame[request_id].request_id) &&
			(a_ctrl->i2c_data.per_frame[request_id].is_settings_valid)
			== 1) {
			rc = cam_actuator_apply_settings(a_ctrl,
				&a_ctrl->i2c_data.per_frame[request_id]);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR,
					"Failed in applying the request: %lld\n",
					actuator_req_id);
			}
		}
		del_req_id = (request_id +
			MAX_PER_FRAME_ARRAY - MAX_SYSTEM_PIPELINE_DELAY) %
			MAX_PER_FRAME_ARRAY;

		if (actuator_req_id >
			a_ctrl->i2c_data.per_frame[del_req_id].request_id) {
			a_ctrl->i2c_data.per_frame[del_req_id].request_id = 0;
			rc = delete_request(&a_ctrl->i2c_data.per_frame[del_req_id]);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR,
					"Fail deleting the req: %d err: %d\n",
					del_req_id, rc);
			}
		} else {
			CAM_DBG(CAM_ACTUATOR, "No Valid Req to clean Up");
		}
	}

	return rc;
}


int cam_actuator_no_crm_apply_req(
	struct cam_req_mgr_no_crm_apply_request *apply)
{
	int32_t rc = 0;
	struct cam_actuator_ctrl_t *a_ctrl = NULL;

	if (!apply) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Input Args");
		return -EINVAL;
	}

	a_ctrl = cam_get_device_no_crm_priv(apply->dev_hdl);

	if (!a_ctrl)
	{
		CAM_ERR(CAM_ACTUATOR, "Invalid private data req[%llu]", apply->anchor_req_id);
		return -EINVAL;
	}

	mutex_lock(&(a_ctrl->actuator_mutex));
	rc = cam_actuator_no_crm_apply_req_lock(a_ctrl, apply);
	mutex_unlock(&(a_ctrl->actuator_mutex));
	return rc;
}

int32_t cam_actuator_driver_cmd(struct cam_actuator_ctrl_t *a_ctrl,
	void *arg)
{
	int rc = 0;
	struct cam_control *cmd = (struct cam_control *)arg;
	struct cam_actuator_soc_private *soc_private = NULL;
	struct cam_sensor_power_ctrl_t  *power_info = NULL;

	if (!a_ctrl || !cmd) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;

	power_info = &soc_private->power_info;

	if (cmd->handle_type != CAM_HANDLE_USER_POINTER) {
		CAM_ERR(CAM_ACTUATOR, "Invalid handle type: %d",
			cmd->handle_type);
		return -EINVAL;
	}

	CAM_DBG(CAM_ACTUATOR, "Opcode to Actuator: %d", cmd->op_code);

	mutex_lock(&(a_ctrl->actuator_mutex));
	switch (cmd->op_code) {
	case CAM_ACQUIRE_DEV: {
		struct cam_sensor_acquire_dev actuator_acq_dev;
		struct cam_create_dev_hdl bridge_params;

		if (a_ctrl->bridge_intf.device_hdl != -1) {
			CAM_ERR(CAM_ACTUATOR, "Device is already acquired");
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = copy_from_user(&actuator_acq_dev,
			u64_to_user_ptr(cmd->handle),
			sizeof(actuator_acq_dev));
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR, "Failed Copying from user\n");
			goto release_mutex;
		}

		bridge_params.session_hdl = actuator_acq_dev.session_handle;
		bridge_params.ops = &a_ctrl->bridge_intf.ops;
		bridge_params.no_crm_ops = NULL;
		bridge_params.no_crm_priv = NULL;
		bridge_params.v4l2_sub_dev_flag = 0;
		bridge_params.media_entity_flag = 0;
		bridge_params.priv = a_ctrl;
		bridge_params.dev_id = CAM_ACTUATOR;
		a_ctrl->bridge_intf.enable_crm = 1;
		a_ctrl->cci_contextId = CONTEXT_ID_MAX;

		if (actuator_acq_dev.info_handle & WITH_NO_CRM_MASK) {
			a_ctrl->bridge_intf.enable_crm = 0;
			bridge_params.no_crm_ops  = &a_ctrl->bridge_intf.no_crm_ops;
			bridge_params.no_crm_priv = a_ctrl;
		}

		actuator_acq_dev.device_handle =
			cam_create_device_hdl(&bridge_params);
		if (actuator_acq_dev.device_handle <= 0) {
			rc = -EFAULT;
			CAM_ERR(CAM_ACTUATOR, "Can not create device handle");
			goto release_mutex;
		}
		a_ctrl->bridge_intf.device_hdl = actuator_acq_dev.device_handle;
		a_ctrl->bridge_intf.session_hdl =
			actuator_acq_dev.session_handle;

		CAM_DBG(CAM_ACTUATOR, "Device Handle: %d",
			actuator_acq_dev.device_handle);
		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&actuator_acq_dev,
			sizeof(struct cam_sensor_acquire_dev))) {
			CAM_ERR(CAM_ACTUATOR, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}

		a_ctrl->cam_act_state = CAM_ACTUATOR_ACQUIRE;
	}
		break;
	case CAM_RELEASE_DEV: {
		if (a_ctrl->cam_act_state == CAM_ACTUATOR_START) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
				"Cant release actuator: in start state");
			goto release_mutex;
		}

		if (a_ctrl->bridge_intf.device_hdl == -1) {
			CAM_ERR(CAM_ACTUATOR, "link hdl: %d device hdl: %d",
				a_ctrl->bridge_intf.device_hdl,
				a_ctrl->bridge_intf.link_hdl);
			rc = -EINVAL;
			goto release_mutex;
		}

		if (a_ctrl->cam_act_state == CAM_ACTUATOR_CONFIG) {
			rc = cam_actuator_power_down(a_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR,
					"Actuator Power Down Failed");
				goto release_mutex;
			}
		}

		if (a_ctrl->bridge_intf.link_hdl != -1) {
			CAM_ERR(CAM_ACTUATOR,
				"Device [%d] still active on link 0x%x",
				a_ctrl->cam_act_state,
				a_ctrl->bridge_intf.link_hdl);
			rc = -EAGAIN;
			goto release_mutex;
		}

		rc = cam_destroy_device_hdl(a_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_ACTUATOR, "destroying the device hdl");
		a_ctrl->is_precise_actuator_control = false;
		a_ctrl->bridge_intf.device_hdl = -1;
		a_ctrl->bridge_intf.link_hdl = -1;
		a_ctrl->bridge_intf.session_hdl = -1;
		a_ctrl->cam_act_state = CAM_ACTUATOR_INIT;
		a_ctrl->last_flush_req = 0;
		kfree(power_info->power_setting);
		kfree(power_info->power_down_setting);
		power_info->power_setting = NULL;
		power_info->power_down_setting = NULL;
		power_info->power_down_setting_size = 0;
		power_info->power_setting_size = 0;
		CAM_INFO(CAM_ACTUATOR, "CAM_RELEASE_DEV Success for Actuator[%d]",
				a_ctrl->soc_info.index);
	}
		break;
	case CAM_QUERY_CAP: {
		struct cam_actuator_query_cap actuator_cap = {0};

		actuator_cap.slot_info = a_ctrl->soc_info.index;
		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&actuator_cap,
			sizeof(struct cam_actuator_query_cap))) {
			CAM_ERR(CAM_ACTUATOR, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}
	}
		break;
	case CAM_START_DEV: {
		if (a_ctrl->cam_act_state != CAM_ACTUATOR_CONFIG) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
			"Not in right state to start : %d",
			a_ctrl->cam_act_state);
			goto release_mutex;
		}
		a_ctrl->cam_act_state = CAM_ACTUATOR_START;
		a_ctrl->last_flush_req = 0;
		CAM_INFO(CAM_ACTUATOR, "cam_start_dev success for actuator[%d]",
				a_ctrl->soc_info.index);
	}
		break;
	case CAM_STOP_DEV: {
		struct i2c_settings_array *i2c_set = NULL;
		struct cci_trigger_cam_setting_array *cci_set = NULL;
		int i, j;
		if (a_ctrl->cam_act_state != CAM_ACTUATOR_START) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
			"Not in right state to stop : %d",
			a_ctrl->cam_act_state);
			goto release_mutex;
		}

		if (a_ctrl->is_precise_actuator_control) {
			if (a_ctrl->cci_contextId < CONTEXT_ID_MAX) {
				rc = camera_io_contextid_release(&(a_ctrl->io_master_info),
						a_ctrl->cci_contextId, FALSE);
				if (rc < 0) {
					CAM_ERR(CAM_ACTUATOR, "Slot[%d] contextid release failed",
						a_ctrl->soc_info.index);
					goto release_mutex;
				}
			}
		}
		a_ctrl->cci_contextId = CONTEXT_ID_MAX;

		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			i2c_set = &(a_ctrl->i2c_data.per_frame[i]);

			if (i2c_set->is_settings_valid == 1) {
				rc = delete_request(i2c_set);
				if (rc < 0)
					CAM_ERR(CAM_ACTUATOR,
						"delete request: %lld rc: %d",
						i2c_set->request_id, rc);
			}
		}

		if (a_ctrl->i2c_data.per_frame_event_settings != NULL) {
			for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
				cci_set = &(a_ctrl->i2c_data.per_frame_event_settings[i]);
				for (j = 0; j < MAX_CMD_BUFFER; j++) {
					// delete request for trigger sensor
					struct i2c_settings_array *i2c_settings =
						&cci_set->event_data[j].trigger_sensor_cmd_buf_info.i2c_settings;
					if (cci_set->event_data[j].cmd_type ==
						CAM_SENSOR_CMD_TYPE_I2C_SETTING &&
						i2c_settings->is_settings_valid == 1) {
						rc = delete_i2c_event_settings(
							i2c_settings);
						if (rc < 0)
							CAM_ERR(CAM_ACTUATOR,
								"delete request: %lld rc: %d",
								i2c_set->request_id, rc);
					}
				}
				cci_set->is_settings_valid = 0;
			}
		}

		a_ctrl->last_flush_req = 0;
		a_ctrl->cam_act_state = CAM_ACTUATOR_CONFIG;
		CAM_INFO(CAM_ACTUATOR, "cam_stop_dev success for actuator[%d]",
				a_ctrl->soc_info.index);
	}
		break;
	case CAM_CONFIG_DEV: {
		a_ctrl->setting_apply_state =
			ACT_APPLY_SETTINGS_LATER;
		rc = cam_actuator_i2c_pkt_parse(a_ctrl, arg);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR, "Failed in actuator Parsing");
			goto release_mutex;
		}

		if (a_ctrl->setting_apply_state ==
			ACT_APPLY_SETTINGS_NOW && !a_ctrl->is_precise_actuator_control) {
			rc = cam_actuator_apply_settings(a_ctrl,
				&a_ctrl->i2c_data.init_settings);
			if ((rc == -EAGAIN) &&
			(a_ctrl->io_master_info.master_type == CCI_MASTER)) {
				CAM_WARN(CAM_ACTUATOR,
					"CCI HW is in resetting mode:: Reapplying Init settings");
				usleep_range(1000, 1010);
				rc = cam_actuator_apply_settings(a_ctrl,
					&a_ctrl->i2c_data.init_settings);
			}

			if (rc < 0)
				CAM_ERR(CAM_ACTUATOR,
					"Failed to apply Init settings: rc = %d",
					rc);
			/* Delete the request even if the apply is failed */
			rc = delete_request(&a_ctrl->i2c_data.init_settings);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR,
					"Failed in Deleting the Init Pkt: %d",
					rc);
				goto release_mutex;
			}
		}
	}
		break;
	default:
		CAM_ERR(CAM_ACTUATOR, "Invalid Opcode %d", cmd->op_code);
	}

release_mutex:
	mutex_unlock(&(a_ctrl->actuator_mutex));

	return rc;
}

int32_t cam_actuator_flush_request(struct cam_req_mgr_flush_request *flush_req)
{
	int32_t rc = 0, i, j;
	uint32_t cancel_req_id_found = 0;
	struct cam_actuator_ctrl_t *a_ctrl = NULL;
	struct i2c_settings_array *i2c_set = NULL;
	struct cci_trigger_cam_setting_array *cci_set = NULL;

	if (!flush_req)
		return -EINVAL;

	a_ctrl = (struct cam_actuator_ctrl_t *)
		cam_get_device_priv(flush_req->dev_hdl);
	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "Device data is NULL");
		return -EINVAL;
	}

	if (a_ctrl->i2c_data.per_frame == NULL) {
		CAM_ERR(CAM_ACTUATOR, "i2c frame data is NULL");
		return -EINVAL;
	}

	if (a_ctrl->i2c_data.per_frame_event_settings == NULL) {
		CAM_ERR(CAM_ACTUATOR, "i2c frame data is NULL");
		return -EINVAL;
	}

	mutex_lock(&(a_ctrl->actuator_mutex));
	if (flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_ALL) {
		a_ctrl->last_flush_req = flush_req->req_id;
		CAM_DBG(CAM_ACTUATOR, "last reqest to flush is %lld",
			flush_req->req_id);
	}

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
		i2c_set = &(a_ctrl->i2c_data.per_frame[i]);

		if ((flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ)
				&& (i2c_set->request_id != flush_req->req_id))
			continue;

		if (i2c_set->is_settings_valid == 1) {
			rc = delete_request(i2c_set);
			if (rc < 0)
				CAM_ERR(CAM_ACTUATOR,
					"delete request: %lld rc: %d",
					i2c_set->request_id, rc);

			if (flush_req->type ==
				CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ) {
				cancel_req_id_found = 1;
				break;
			}
		}
	}

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
		cci_set = &(a_ctrl->i2c_data.per_frame_event_settings[i]);

		if ((flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ)
				&& (cci_set->request_id != flush_req->req_id))
			continue;

		for (j = 0; j < MAX_CMD_BUFFER; j++) {
			struct i2c_settings_array *i2c_settings =
				&cci_set->event_data[j].trigger_sensor_cmd_buf_info.i2c_settings;
			if(cci_set->event_data[j].cmd_type ==
				CAM_SENSOR_CMD_TYPE_I2C_SETTING &&
				i2c_settings->is_settings_valid == 1) {
				rc = delete_i2c_event_settings(
					i2c_settings);
				if (rc < 0)
					CAM_ERR(CAM_ACTUATOR,
						"delete request: %lld rc: %d",
						cci_set->request_id, rc);
			}

			if (flush_req->type ==
				CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ) {
				cancel_req_id_found = 1;
				break;
			}
		}
		cci_set->is_settings_valid = 0;
	}

	if (flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ &&
		!cancel_req_id_found)
		CAM_DBG(CAM_ACTUATOR,
			"Flush request id:%lld not found in the pending list",
			flush_req->req_id);
	mutex_unlock(&(a_ctrl->actuator_mutex));
	return rc;
}
