/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_IFE_HW_MGR_H_
#define _CAM_IFE_HW_MGR_H_

#include <linux/completion.h>
#include <linux/time.h>
#include "cam_isp_hw_mgr.h"
#include "cam_vfe_hw_intf.h"
#include "cam_ife_csid_hw_intf.h"
#include "cam_top_tpg_hw_intf.h"
#include "cam_tasklet_util.h"
#include "cam_cdm_intf_api.h"

/*
 * enum cam_ife_ctx_master_type - HW master type
 * CAM_IFE_CTX_TYPE_NONE: IFE ctx/stream directly connected to CSID
 * CAM_IFE_CTX_TYPE_CUSTOM: IFE ctx/stream connected to custom HW
 * CAM_IFE_CTX_TYPE_SFE: IFE ctx/stream connected to SFE
 */
enum cam_ife_ctx_master_type {
	CAM_IFE_CTX_TYPE_NONE,
	CAM_IFE_CTX_TYPE_CUSTOM,
	CAM_IFE_CTX_TYPE_SFE,
	CAM_IFE_CTX_TYPE_MAX,
};

/* IFE resource constants */
#define CAM_IFE_HW_IN_RES_MAX            (CAM_ISP_IFE_IN_RES_MAX & 0xFF)
#define CAM_SFE_HW_OUT_RES_MAX           (CAM_ISP_SFE_OUT_RES_MAX & 0xFF)
#define CAM_IFE_HW_RES_POOL_MAX          64

/* IFE_HW_MGR ctx config */
#define CAM_IFE_CTX_CFG_FRAME_HEADER_TS   BIT(0)
#define CAM_IFE_CTX_CFG_SW_SYNC_ON        BIT(1)
#define CAM_IFE_CTX_CFG_SFE_FS_MODE       BIT(2)
#define CAM_IFE_CTX_CFG_DYNAMIC_SWITCH_ON BIT(3)

#define CAM_IFE_UBWC_COMP_EN                 BIT(1)

/**
 * struct cam_ife_hw_mgr_debug - contain the debug information
 *
 * @dentry:                    Debugfs entry
 * @csid_debug:                csid debug information
 * @enable_recovery:           enable recovery
 * @enable_csid_recovery:      enable csid recovery
 * @enable_diag_sensor_status: enable sensor diagnosis status
 * @enable_req_dump:           Enable request dump on HW errors
 * @per_req_reg_dump:          Enable per request reg dump
 * @disable_ubwc_comp:         Disable UBWC compression
 *
 */
struct cam_ife_hw_mgr_debug {
	struct dentry  *dentry;
	uint64_t       csid_debug;
	uint32_t       enable_recovery;
	uint32_t       camif_debug;
	uint32_t       enable_csid_recovery;
	bool           enable_req_dump;
	bool           per_req_reg_dump;
	bool           disable_ubwc_comp;
};

/**
 * struct cam_sfe_scratch_buf_info - Scratch buf info
 *
 * @width: Width in pixels
 * @height: Height in pixels
 * @stride: Stride in pixels
 * @slice_height: Height in lines
 * @io_addr: Buffer address
 * @res_id: Resource type
 * @is_secure: secure scratch buffer
 */
struct cam_sfe_scratch_buf_info {
	uint32_t   width;
	uint32_t   height;
	uint32_t   stride;
	uint32_t   slice_height;
	dma_addr_t io_addr;
	uint32_t   res_id;
	bool       is_secure;
};

/**
 * struct cam_sfe_scratch_buf_cfg - Scratch buf info
 *
 * @config_done: To indicate if stream received it's scratch cfg
 * @num_configs: Number of buffer configs [max of 3 currently]
 * @curr_num_exp: Current num of exposures
 * @buf_info: Info on each of the buffers
 *
 */
struct cam_sfe_scratch_buf_cfg {
	bool                            config_done;
	uint32_t                        num_config;
	uint32_t                        curr_num_exp;
	struct cam_sfe_scratch_buf_info buf_info[
		CAM_SFE_FE_RDI_NUM_MAX];
};

/**
 * struct cam_ife_hw_mgr_ctx - IFE HW manager Context object
 *
 * @list:                   used by the ctx list.
 * @common:                 common acquired context data
 * @ctx_index:              acquired context id.
 * @master_hw_idx:          hw index for master core
 * @slave_hw_idx:           hw index for slave core
 * @hw_mgr:                 IFE hw mgr which owns this context
 * @ctx_in_use:             flag to tell whether context is active
 * @res_list_tpg:           TPG resource list
 * @res_list_ife_in:        Starting resource(TPG,PHY0, PHY1...) Can only be
 *                          one.
 * @res_list_csid:          CSID resource list
 * @res_list_ife_src:       IFE input resource list
 * @res_list_sfe_src        SFE input resource list
 * @res_list_ife_in_rd      IFE/SFE input resource list for read path
 * @res_list_ife_out:       IFE output resoruces array
 * @res_list_sfe_out:       SFE output resources array
 * @free_res_list:          Free resources list for the branch node
 * @res_pool:               memory storage for the free resource list
 * @irq_status0_mask:       irq_status0_mask for the context
 * @irq_status1_mask:       irq_status1_mask for the context
 * @base                    device base index array contain the all IFE HW
 *                          instance associated with this context.
 * @num_base                number of valid base data in the base array
 * @cdm_handle              cdm hw acquire handle
 * @cdm_ops                 cdm util operation pointer for building
 *                          cdm commands
 * @cdm_cmd                 cdm base and length request pointer
 * @cdm_id                  cdm id of the acquired cdm
 * @sof_cnt                 sof count value per core, used for dual VFE
 * @epoch_cnt               epoch count value per core, used for dual VFE
 * @eof_cnt                 eof count value per core, used for dual VFE
 * @overflow_pending        flat to specify the overflow is pending for the
 *                          context
 * @cdm_done                flag to indicate cdm has finished writing shadow
 *                          registers
 * @last_cdm_done_req:      Last cdm done request
 * @is_rdi_only_context     flag to specify the context has only rdi resource
 * @is_lite_context         flag to specify the context has only uses lite
 *                          resources
 * @config_done_complete    indicator for configuration complete
 * @reg_dump_buf_desc:      cmd buffer descriptors for reg dump
 * @num_reg_dump_buf:       Count of descriptors in reg_dump_buf_desc
 * @applied_req_id:         Last request id to be applied
 * @last_dump_flush_req_id  Last req id for which reg dump on flush was called
 * @last_dump_err_req_id    Last req id for which reg dump on error was called
 * @init_done               indicate whether init hw is done
 * @is_fe_enabled           Indicate whether fetch engine\read path is enabled
 * @is_dual                 indicate whether context is in dual VFE mode
 * @ctx_type                Type of IFE ctx [CUSTOM/SFE etc.]
 * @custom_config           ife ctx config if custom is enabled [bit field]
 * @ts                      captured timestamp when the ctx is acquired
 * @is_tpg                  indicate whether context is using PHY TPG
 * @is_offline              Indicate whether context is for offline IFE
 * @dsp_enabled             Indicate whether dsp is enabled in this context
 * @hw_enabled              Array to indicate active HW
 * @internal_cdm            Indicate whether context uses internal CDM
 * @pf_mid_found            in page fault, mid found for this ctx.
 * @buf_done_controller     Buf done controller.
 * @need_csid_top_cfg       Flag to indicate if CSID top cfg is  needed.
 * @scratch_config          Scratch buffer config if any for this ctx
 *
 */
struct cam_ife_hw_mgr_ctx {
	struct list_head                list;
	struct cam_isp_hw_mgr_ctx       common;

	uint32_t                        ctx_index;
	uint32_t                        master_hw_idx;
	uint32_t                        slave_hw_idx;
	struct cam_ife_hw_mgr          *hw_mgr;
	uint32_t                        ctx_in_use;

	struct cam_isp_hw_mgr_res       res_list_ife_in;
	struct cam_isp_hw_mgr_res       res_list_tpg;
	struct list_head                res_list_ife_csid;
	struct list_head                res_list_ife_src;
	struct list_head                res_list_sfe_src;
	struct list_head                res_list_ife_in_rd;
	struct cam_isp_hw_mgr_res      *res_list_ife_out;
	struct cam_isp_hw_mgr_res       res_list_sfe_out[
						CAM_SFE_HW_OUT_RES_MAX];
	struct list_head                free_res_list;
	struct cam_isp_hw_mgr_res       res_pool[CAM_IFE_HW_RES_POOL_MAX];

	uint32_t                        irq_status0_mask[CAM_IFE_HW_NUM_MAX];
	uint32_t                        irq_status1_mask[CAM_IFE_HW_NUM_MAX];
	struct cam_isp_ctx_base_info    base[CAM_IFE_HW_NUM_MAX +
						CAM_SFE_HW_NUM_MAX];
	uint32_t                        num_base;
	uint32_t                        cdm_handle;
	struct cam_cdm_utils_ops       *cdm_ops;
	struct cam_cdm_bl_request      *cdm_cmd;
	enum cam_cdm_id                 cdm_id;
	uint32_t                        sof_cnt[CAM_IFE_HW_NUM_MAX];
	uint32_t                        epoch_cnt[CAM_IFE_HW_NUM_MAX];
	uint32_t                        eof_cnt[CAM_IFE_HW_NUM_MAX];
	atomic_t                        overflow_pending;
	atomic_t                        cdm_done;
	uint64_t                        last_cdm_done_req;
	uint32_t                        is_rdi_only_context;
	uint32_t                        is_lite_context;
	struct completion               config_done_complete;
	uint32_t                        hw_version;
	struct cam_cmd_buf_desc         reg_dump_buf_desc[
						CAM_REG_DUMP_MAX_BUF_ENTRIES];
	uint32_t                        num_reg_dump_buf;
	uint64_t                        applied_req_id;
	uint64_t                        last_dump_flush_req_id;
	uint64_t                        last_dump_err_req_id;
	bool                            init_done;
	bool                            is_fe_enabled;
	bool                            is_dual;
	enum cam_ife_ctx_master_type    ctx_type;
	uint32_t                        ctx_config;
	struct timespec64               ts;
	bool                            is_tpg;
	bool                            is_offline;
	bool                            dsp_enabled;
	bool                            internal_cdm;
	bool                            pf_mid_found;
	bool                            need_csid_top_cfg;
	void                           *buf_done_controller;
	struct cam_sfe_scratch_buf_cfg  scratch_config;
};

/**
 * struct cam_isp_bus_hw_caps - BUS capabilities
 *
 * @max_vfe_out_res_type  :  max ife out res type value from hw
 * @max_sfe_out_res_type  :  max sfe out res type value from hw
 * @support_consumed_addr :  indicate whether hw supports last consumed address
 */
struct cam_isp_bus_hw_caps {
	uint32_t     max_vfe_out_res_type;
	uint32_t     max_sfe_out_res_type;
	bool         support_consumed_addr;
};

/**
 * struct cam_ife_hw_mgr - IFE HW Manager
 *
 * @mgr_common:            common data for all HW managers
 * @csid_devices;          csid device instances array. This will be filled by
 *                         HW manager during the initialization.
 * @ife_devices:           IFE device instances array. This will be filled by
 *                         HW layer during initialization
 * @sfe_devices:           SFE device instance array
 * @ctx_mutex:             mutex for the hw context pool
 * @free_ctx_list:         free hw context list
 * @used_ctx_list:         used hw context list
 * @ctx_pool:              context storage
 * @csid_hw_caps           csid hw capability stored per core
 * @ife_dev_caps           ife device capability per core
 * @work q                 work queue for IFE hw manager
 * @debug_cfg              debug configuration
 * @ctx_lock               context lock
 * @hw_pid_support         hw pid support for this target
 * @csid_rup_en            Reg update at CSID side
 * @csid_global_reset_en   CSID global reset enable
 * @isp_bus_caps           Capability of underlying SFE/IFE bus HW
 */
struct cam_ife_hw_mgr {
	struct cam_isp_hw_mgr          mgr_common;
	struct cam_hw_intf            *tpg_devices[CAM_TOP_TPG_HW_NUM_MAX];
	struct cam_hw_intf            *csid_devices[CAM_IFE_CSID_HW_NUM_MAX];
	struct cam_isp_hw_intf_data   *ife_devices[CAM_IFE_HW_NUM_MAX];
	struct cam_hw_intf            *sfe_devices[CAM_SFE_HW_NUM_MAX];
	struct cam_soc_reg_map        *cdm_reg_map[CAM_IFE_HW_NUM_MAX];

	struct mutex                   ctx_mutex;
	atomic_t                       active_ctx_cnt;
	struct list_head               free_ctx_list;
	struct list_head               used_ctx_list;
	struct cam_ife_hw_mgr_ctx      ctx_pool[CAM_IFE_CTX_MAX];

	struct cam_ife_csid_hw_caps    csid_hw_caps[
						CAM_IFE_CSID_HW_NUM_MAX];
	struct cam_vfe_hw_get_hw_cap   ife_dev_caps[CAM_IFE_HW_NUM_MAX];
	struct cam_req_mgr_core_workq *workq;
	struct cam_ife_hw_mgr_debug    debug_cfg;
	spinlock_t                     ctx_lock;
	bool                           hw_pid_support;
	bool                           csid_rup_en;
	bool                           csid_global_reset_en;
	struct cam_isp_bus_hw_caps     isp_bus_caps;
};

/**
 * struct cam_ife_hw_event_recovery_data - Payload for the recovery procedure
 *
 * @error_type:               Error type that causes the recovery
 * @affected_core:            Array of the hardware cores that are affected
 * @affected_ctx:             Array of the hardware contexts that are affected
 * @no_of_context:            Actual number of the affected context
 *
 */
struct cam_ife_hw_event_recovery_data {
	uint32_t                   error_type;
	uint32_t                   affected_core[CAM_ISP_HW_NUM_MAX];
	struct cam_ife_hw_mgr_ctx *affected_ctx[CAM_IFE_CTX_MAX];
	uint32_t                   no_of_context;
};

/**
 * cam_ife_hw_mgr_init()
 *
 * @brief:              Initialize the IFE hardware manger. This is the
 *                      etnry functinon for the IFE HW manager.
 *
 * @hw_mgr_intf:        IFE hardware manager object returned
 * @iommu_hdl:          Iommu handle to be returned
 *
 */
int cam_ife_hw_mgr_init(struct cam_hw_mgr_intf *hw_mgr_intf, int *iommu_hdl);
void cam_ife_hw_mgr_deinit(void);

#endif /* _CAM_IFE_HW_MGR_H_ */
