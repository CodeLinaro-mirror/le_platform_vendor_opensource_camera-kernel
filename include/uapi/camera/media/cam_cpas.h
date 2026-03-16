/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __UAPI_CAM_CPAS_H__
#define __UAPI_CAM_CPAS_H__

#include <media/cam_defs.h>

#define CAM_FAMILY_CAMERA_SS     1
#define CAM_FAMILY_CPAS_SS       2

/* AXI BW Voting Version */
#define CAM_AXI_BW_VOTING_V2                2

/* AXI BW Voting Transaction Type */
#define CAM_AXI_TRANSACTION_READ            0
#define CAM_AXI_TRANSACTION_WRITE           1

/* AXI BW Voting Path Data Type */
#define CAM_AXI_PATH_DATA_IFE_START_OFFSET 0
#define CAM_AXI_PATH_DATA_IFE_LINEAR    (CAM_AXI_PATH_DATA_IFE_START_OFFSET + 0)
#define CAM_AXI_PATH_DATA_IFE_VID       (CAM_AXI_PATH_DATA_IFE_START_OFFSET + 1)
#define CAM_AXI_PATH_DATA_IFE_DISP      (CAM_AXI_PATH_DATA_IFE_START_OFFSET + 2)
#define CAM_AXI_PATH_DATA_IFE_STATS     (CAM_AXI_PATH_DATA_IFE_START_OFFSET + 3)
#define CAM_AXI_PATH_DATA_IFE_RDI0      (CAM_AXI_PATH_DATA_IFE_START_OFFSET + 4)
#define CAM_AXI_PATH_DATA_IFE_RDI1      (CAM_AXI_PATH_DATA_IFE_START_OFFSET + 5)
#define CAM_AXI_PATH_DATA_IFE_RDI2      (CAM_AXI_PATH_DATA_IFE_START_OFFSET + 6)
#define CAM_AXI_PATH_DATA_IFE_RDI3      (CAM_AXI_PATH_DATA_IFE_START_OFFSET + 7)
#define CAM_AXI_PATH_DATA_IFE_PDAF      (CAM_AXI_PATH_DATA_IFE_START_OFFSET + 8)
#define CAM_AXI_PATH_DATA_IFE_PIXEL_RAW \
	(CAM_AXI_PATH_DATA_IFE_START_OFFSET + 9)
#define CAM_AXI_PATH_DATA_IFE_MAX_OFFSET \
	(CAM_AXI_PATH_DATA_IFE_START_OFFSET + 31)

#define CAM_AXI_PATH_DATA_IPE_START_OFFSET 32
#define CAM_AXI_PATH_DATA_IPE_RD_IN     (CAM_AXI_PATH_DATA_IPE_START_OFFSET + 0)
#define CAM_AXI_PATH_DATA_IPE_RD_REF    (CAM_AXI_PATH_DATA_IPE_START_OFFSET + 1)
#define CAM_AXI_PATH_DATA_IPE_WR_VID    (CAM_AXI_PATH_DATA_IPE_START_OFFSET + 2)
#define CAM_AXI_PATH_DATA_IPE_WR_DISP   (CAM_AXI_PATH_DATA_IPE_START_OFFSET + 3)
#define CAM_AXI_PATH_DATA_IPE_WR_REF    (CAM_AXI_PATH_DATA_IPE_START_OFFSET + 4)
#define CAM_AXI_PATH_DATA_IPE_MAX_OFFSET \
	(CAM_AXI_PATH_DATA_IPE_START_OFFSET + 31)

#define CAM_AXI_PATH_DATA_OPE_START_OFFSET 64
#define CAM_AXI_PATH_DATA_OPE_RD_IN     (CAM_AXI_PATH_DATA_OPE_START_OFFSET + 0)
#define CAM_AXI_PATH_DATA_OPE_RD_REF    (CAM_AXI_PATH_DATA_OPE_START_OFFSET + 1)
#define CAM_AXI_PATH_DATA_OPE_WR_VID    (CAM_AXI_PATH_DATA_OPE_START_OFFSET + 2)
#define CAM_AXI_PATH_DATA_OPE_WR_DISP   (CAM_AXI_PATH_DATA_OPE_START_OFFSET + 3)
#define CAM_AXI_PATH_DATA_OPE_WR_REF    (CAM_AXI_PATH_DATA_OPE_START_OFFSET + 4)
#define CAM_AXI_PATH_DATA_OPE_MAX_OFFSET \
	(CAM_AXI_PATH_DATA_OPE_START_OFFSET + 31)

#define CAM_AXI_PATH_DATA_SFE_START_OFFSET 96
#define CAM_AXI_PATH_DATA_SFE_NRDI      (CAM_AXI_PATH_DATA_SFE_START_OFFSET + 0)
#define CAM_AXI_PATH_DATA_SFE_RDI0      (CAM_AXI_PATH_DATA_SFE_START_OFFSET + 1)
#define CAM_AXI_PATH_DATA_SFE_RDI1      (CAM_AXI_PATH_DATA_SFE_START_OFFSET + 2)
#define CAM_AXI_PATH_DATA_SFE_RDI2      (CAM_AXI_PATH_DATA_SFE_START_OFFSET + 3)
#define CAM_AXI_PATH_DATA_SFE_RDI3      (CAM_AXI_PATH_DATA_SFE_START_OFFSET + 4)
#define CAM_AXI_PATH_DATA_SFE_RDI4      (CAM_AXI_PATH_DATA_SFE_START_OFFSET + 5)
#define CAM_AXI_PATH_DATA_SFE_STATS     (CAM_AXI_PATH_DATA_SFE_START_OFFSET + 6)
#define CAM_AXI_PATH_DATA_SFE_MAX_OFFSET \
	(CAM_AXI_PATH_DATA_SFE_START_OFFSET + 31)

#define CAM_AXI_PATH_DATA_CRE_START_OFFSET    (CAM_AXI_PATH_DATA_SFE_MAX_OFFSET + 1)
#define CAM_AXI_PATH_DATA_CRE_RD_IN           (CAM_AXI_PATH_DATA_CRE_START_OFFSET + 0)
#define CAM_AXI_PATH_DATA_CRE_WR_OUT          (CAM_AXI_PATH_DATA_CRE_START_OFFSET + 1)
#define CAM_AXI_PATH_DATA_CRE_MAX_OFFSET \
	(CAM_AXI_PATH_DATA_CRE_START_OFFSET + 31)

#define CAM_AXI_PATH_DATA_ALL  256
#define CAM_CPAS_FUSES_MAX     32

#define CAM_NO_COHERENCY              1
#define CAM_DMA_COHERENT              2
#define CAM_DMA_COHERENT_HINT_CACHED  3

/* Total number of sys cache */
#define CAM_NUM_SYS_CACHE    20

/* sys cache type */
#define CAM_LLCC_SMALL_1                   0
#define CAM_LLCC_SMALL_2                   1
#define CAM_LLCC_LARGE_1                   2
#define CAM_LLCC_LARGE_2                   3
#define CAM_LLCC_LARGE_3                   4
#define CAM_LLCC_LARGE_4                   5
#define CAM_LLCC_OFE_IP                    6
#define CAM_LLCC_IPE_RT_IP                 7
#define CAM_LLCC_IPE_SRT_IP                8
#define CAM_LLCC_IPE_RT_RF                 9
#define CAM_LLCC_IPE_SRT_RF                10
#define CAM_LLCC_IPE_SRT_STRIPE_OVERLAP    11
#define CAM_LLCC_IPE_RT_STRIPE_OVERLAP     12
#define CAM_LLCC_OFE_STRIPE_OVERLAP        13
#define CAM_LLCC_IPE_TO_VIDEO              14
#define CAM_LLCC_IPE_TO_CSC                15

/* cam sys cache llcc staling mode */
#define CAM_LLCC_STALING_MODE_CAPACITY    1
#define CAM_LLCC_STALING_MODE_NOTIFY      2


/* cam sys cache operating type */
#define CAM_LLCC_NOTIFY_STALING_EVICT         1
#define CAM_LLCC_NOTIFY_STALING_FORGET        2

/* cam cpas query type */
#define CAM_CPAS_QUERY_BLOB_BASE       CAM_COMMON_QUERY_BLOB_END
#define CAM_CPAS_QUERY_BLOB_V4        (CAM_CPAS_QUERY_BLOB_BASE + 1)
#define CAM_CPAS_QUERY_BLOB_SYSCACHE  (CAM_CPAS_QUERY_BLOB_BASE + 2)

/**
 * struct cam_cpas_fuse_value - CPAS fuse value
 *
 * @fuse_id     : Camera fuse identification
 * @fuse_val    : Camera Fuse Value
 */
struct cam_cpas_fuse_value {
	__u32 fuse_id;
	__u32 fuse_val;
};

/**
 * struct cam_cpas_fuse_info - CPAS fuse info
 *
 * @num_fuses     : Number of fuses
 * @fuse_val      : Array of different fuse info.
 */
struct cam_cpas_fuse_info {
	__u32  num_fuses;
	struct cam_cpas_fuse_value fuse_val[CAM_CPAS_FUSES_MAX];
};

/**
 * struct cam_cpas_query_cap - CPAS query device capability payload
 *
 * @camera_family     : Camera family type
 * @reserved          : Reserved field for alignment
 * @camera_version    : Camera platform version
 * @cpas_version      : Camera CPAS version within camera platform
 *
 */
struct cam_cpas_query_cap {
	__u32                 camera_family;
	__u32                 reserved;
	struct cam_hw_version camera_version;
	struct cam_hw_version cpas_version;
};

/**
 * struct cam_cpas_query_cap - CPAS query device capability payload
 *
 * @camera_family     : Camera family type
 * @reserved          : Reserved field for alignment
 * @camera_version    : Camera platform version
 * @cpas_version      : Camera CPAS version within camera platform
 * @fuse_info         : Camera fuse info
 *
 */
struct cam_cpas_query_cap_v2 {
	__u32                     camera_family;
	__u32                     reserved;
	struct cam_hw_version     camera_version;
	struct cam_hw_version     cpas_version;
	struct cam_cpas_fuse_info fuse_info;
};

/**
 * struct cam_cpas_query_cap - CPAS query device capability payload
 *
 * @camera_family       : Camera family type
 * @reserved            : Reserved field for alignment
 * @camera_version      : Camera platform version
 * @cpas_version        : Camera CPAS version within camera platform
 * @fuse_info           : Camera fuse info
 * @rt_bw_voting_needed : RT BW voting required
 *
 */
struct cam_cpas_query_cap_v3 {
	__u32                     camera_family;
	__u32                     reserved;
	struct cam_hw_version     camera_version;
	struct cam_hw_version     cpas_version;
	struct cam_cpas_fuse_info fuse_info;
	__u32                     rt_bw_voting_needed;
};

/**
 * struct cam_cpas_query_cap - CPAS query device capability payload
 *
 * @version             : Camera cpas query version
 * @camera_family       : Camera family type
 * @cam_caps            : Camera_capability
 * @coherency_mode      : Coherency mode
 * @camera_version      : Camera platform version
 * @cpas_version        : Camera CPAS version within camera platform
 * @fuse_info           : Camera fuse info
 * @rt_bw_voting_needed : Rt bw voting need
 * @reserved            : Reserved field for alignment
 *
 */
struct cam_cpas_query_cap_v4 {
	__u32                     version;
	__u32                     camera_family;
	__u32                     cam_caps;
	__u32                     coherency_mode;
	struct cam_hw_version     camera_version;
	struct cam_hw_version     cpas_version;
	struct cam_cpas_fuse_info fuse_info;
	__u32                     rt_bw_voting_needed;
	__u32                     reserved;
};

/**
 * struct cam_cpas_sys_cache_cap - sys cache payload information
 *
 * @version         : struct version
 * @scid_id         : sys cache id
 * @scid_num        : sys cache number
 * @concur_usage    : concurrent usage
 *
 */
struct cam_cpas_sys_cache_cap {
	__u32                      version;
	__u32                      scid_id;
	__u32                      scid_num;
	__u32                      concur_usage;
	__u32                      num_valid_params;
	__u32                      valid_param_mask;
	__u32                      params[10];
};

/**
 * struct cam_cpas_sys_cache_query - cache query capability payload
 *
 * @num_cache         : number of cache
 * @reserved          : reserved paramas
 * @sys_cache_cap     : information of sys cache
 */
struct cam_cpas_sys_cache_query {
	__u32                             num_cache;
	__u32                             reserved;
	struct cam_cpas_sys_cache_cap     sys_cache_cap[CAM_NUM_SYS_CACHE];
};

/**
 * struct cam_axi_per_path_bw_vote - Per path bandwidth vote information
 *
 * @usage_data               client usage data (left/right/rdi)
 * @transac_type             Transaction type on the path (read/write)
 * @path_data_type           Path for which vote is given (video, display, rdi)
 * @reserved                 Reserved for alignment
 * @camnoc_bw                CAMNOC bw for this path
 * @mnoc_ab_bw               MNOC AB bw for this path
 * @mnoc_ib_bw               MNOC IB bw for this path
 * @ddr_ab_bw                DDR AB bw for this path
 * @ddr_ib_bw                DDR IB bw for this path
 */
struct cam_axi_per_path_bw_vote {
	__u32                      usage_data;
	__u32                      transac_type;
	__u32                      path_data_type;
	__u32                      reserved;
	__u64                      camnoc_bw;
	__u64                      mnoc_ab_bw;
	__u64                      mnoc_ib_bw;
	__u64                      ddr_ab_bw;
	__u64                      ddr_ib_bw;
};

/**
 * struct cam_sys_cache_config - sys cache config information
 *
 * @version                struct version
 * @scid_id                cache scid id
 * @activate               sys cache need to activate or not
 * @deactivate             to deactivate any specific sys cache
 * @staling_distance       staling distance used for notification
 * @llcc_staling_mode      staling mode evict/forget
 * @llcc_staling_op_type   operation type capacity/notify
 * @change_params          this parameter to tell param change needed or not
 * @num_valid_params:      Number of valid params
 * @valid_param_mask:      Valid param mask
 * @params:                params
 */
struct cam_sys_cache_config {
	__u32                       version;
	__u32                       scid_id;
	__u32                       activate;
	__u32                       deactivate;
	__u32                       staling_distance;
	__s32                       llcc_staling_mode;
	__s32                       llcc_staling_op_type;
	__u32                       change_params;
	__u32                       num_valid_params;
	__u32                       valid_param_mask;
	__u32                       params[6];
};

/**
 * struct cam_sys_cache_config_request - sys cache config request
 *
 * @num:                   num of cache
 * @reserved:              reserved params
 * @sys_cache_config:      sys cache config data
 */
struct cam_sys_cache_config_request {
	__u32                       num;
	__u32                       reserved;
	union {
		struct cam_sys_cache_config sys_cache_config[1];
		__DECLARE_FLEX_ARRAY(struct cam_sys_cache_config, sys_cache_config_flex);
	};
};

#endif /* __UAPI_CAM_CPAS_H__ */
