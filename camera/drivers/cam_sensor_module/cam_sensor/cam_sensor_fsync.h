/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _CAM_SENSOR_FSYNC_H_
#define _CAM_SENSOR_FSYNC_H_

#include <linux/types.h>
#include <media/cam_sensor.h>

struct cam_sensor_ctrl_t;

/* Upper bound on cci_sync_info.config_size to cap the kernel allocation
 * used to stage the mode-specific payload copied in from config_ptr.
 */
#define CAM_SENSOR_FSYNC_MAX_CONFIG_SIZE 4096

/**
 * struct sync_info_data - Per-frame sync info storage
 * @sync_info:         CCI sync configuration for this frame
 * @request_id:        Request ID this sync info belongs to
 * @is_settings_valid: Flag indicating if settings are valid
 */
struct sync_info_data {
	struct cci_sync_info sync_info;
	int64_t request_id;
	int32_t is_settings_valid;
};

/**
 * cam_sensor_fsync_handle_blob - Handle CAM_SENSOR_GENERIC_BLOB_SYNC_INFO blob
 * @blob_data: Raw pointer to blob payload
 * @blob_size: Byte length of the blob payload
 * @s_ctrl:    Sensor control structure to populate
 *
 * Decodes and validates a cci_sync_info blob received from userspace,
 * stores the result in per_frame_sync_info and per_frame_cmd_buf.
 *
 * Returns: 0 on success, negative error code on failure
 */
int cam_sensor_fsync_handle_blob(uint8_t *blob_data, uint32_t blob_size,
	struct cam_sensor_ctrl_t *s_ctrl);

/**
 * cam_sensor_fsync_apply - Load GPIO queue for a specific request at apply time
 * @s_ctrl:  Sensor control structure
 * @req_id:  Request ID to apply
 *
 * Loads the pre-converted GPIO command buffer into the CCI hardware GPIO
 * queue for the given request, then starts the queue.
 *
 * Returns: 0 on success, negative error code on failure
 */
int cam_sensor_fsync_apply(struct cam_sensor_ctrl_t *s_ctrl, int64_t req_id);

#endif /* _CAM_SENSOR_FSYNC_H_ */
