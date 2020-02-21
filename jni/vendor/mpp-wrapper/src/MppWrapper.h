/*
 * $Id: $
 *
 * A Wrapper Class for Rockchip Media Process Platform (MPP)
 *
 * Copyright (c) 2019-2020 FoilPlanet. All rights reserved.
 *
 */

#pragma once

#include "rk_mpi.h"

#include "mpp_env.h"
#include "mpp_mem.h"
#include "mpp_log.h"
#include "mpp_time.h"
#include "mpp_common.h"

#ifdef MODULE_TAG
# undef MODULE_TAG
# define MODULE_TAG "mpp_wrapper"
#endif

/**
 * Wrapper class to Rockchip Media Process Platform (MPP)
 */
class MppWrapper
{
public:
    MppWrapper();

    ~MppWrapper();

    int init(uint32_t width, uint32_t height, MppCodingType type = MPP_VIDEO_CodingAVC);

    void deinit();

    /**
     * Allocate a block of buffer from mpp memory pool
     * @param size buffer size in bytes, 0 for default frame_size
     * @return pointer to buffer
     */
    MppBuffer get_buffer(size_t size = 0) {
        MppBuffer frmbuf = nullptr;
        MPP_RET ret;
        if (MPP_OK != (ret = mpp_buffer_get(NULL, &frmbuf, size ? size : m_frame_size))) {
            mpp_err("get_buffer failed: %d\n", ret);
        }
        return frmbuf;
    }

    /**
     * Free the allocated buffer to mpp memory pool
     * @param buf buffer alocated by get_buffer
     */
    void put_buffer(MppBuffer frmbuf) {
        mpp_buffer_put(frmbuf);
    }

    /**
     * Send video frame to encoder, and get encoded video stream (packet)
     * @param frmbuf The input video data buffer
     * @param frmfmt The frame format (RGBA or YUV, etc.)
     * @return The output compressed data, null for EOS
     */
    MppPacket encode_get_packet(MppBuffer frmbuf, MppFrameFormat frmfmt);

    /**
     * Free the packet resource
     * @param packet The output compressed data
     */
    void put_packet(MppPacket packet) {
        (void)mpp_packet_deinit(&packet);
    }

    void set_frate(int rate) {
        m_fps = rate;
    };

    RK_U32 get_width() const {
        return m_width;
    }

    RK_U32 get_height() const {
        return m_height;
    }

    /**
     * Check whether current encoder need yuv (chroma) as input. 
     * @param coding_type MppCodingType
     * @return true if input frame needs transform to YUV_420, elsewise just use
     *         default frame format (FORMAT_RGBA_8888 in android)
     */
    bool is_yuv(int coding_type) const;

private:
    MppApi         *m_mpi;
    MppCtx          m_ctx;
    MppEncCodecCfg  m_codec_cfg;
    MppEncPrepCfg   m_prep_cfg;
    MppEncRcCfg     m_rc_cfg;

    // input / output
    MppBufferGroup  m_frm_grp;
    MppBufferGroup  m_pkt_grp;

    MppEncOSDPlt    m_osd_plt;
    MppEncROIRegion m_roi_region[3];        /* can be more regions */
    MppEncSeiMode   m_sei_mode;

    // paramter for resource malloc
    RK_U32          m_width;
    RK_U32          m_height;
    RK_U32          m_hor_stride;
    RK_U32          m_ver_stride;
    MppFrameFormat  m_fmt;
    MppCodingType   m_type;
    RK_U32          m_num_frames;

    // resources
    size_t          m_frame_size;
    size_t          m_packet_size;
    RK_U32          m_plt_table[8];

    // rate control runtime parameter
    RK_S32          m_gop;
    RK_S32          m_fps;
    RK_S32          m_bps;
    RK_S32          m_qp_min;
    RK_S32          m_qp_max;
    RK_S32          m_qp_step;
    RK_S32          m_qp_init;
};
