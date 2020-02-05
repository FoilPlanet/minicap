/*
 * $Id: $
 *
 * Wrapper Class for MPP: implementation
 *
 * Copyright (c) 2019-2020 FoilPlanet. All rights reserved.
 *
 */

#include "MppWrapper.h"

#include <thread>
#include <chrono>

MppWrapper::MppWrapper()
  : m_mpi(nullptr), 
    m_ctx(nullptr),
    m_fps(30)
{
    // NOTHING
}

MppWrapper::~MppWrapper()
{
    // deinit();
}

static inline MPP_RET mpi_enc_gen_osd_plt(MppEncOSDPlt *osd_plt, RK_U32 *table)
{
    RK_U32 k = 0;
    for (k = 0; k < 256; k++)
        osd_plt->buf[k] = table[k % 8];
    return MPP_OK;
}

int MppWrapper::init(uint32_t width, uint32_t height, MppCodingType type)
{
    MPP_RET ret = MPP_OK;
    
    while (ret == MPP_OK) {
        if (MPP_OK != (ret = mpp_create(&m_ctx, &m_mpi))) {
            mpp_err("mpp_create failed ret %d\n", ret);
            break;
        }

        if (MPP_OK != (ret = mpp_init(m_ctx, MPP_CTX_ENC, type))) {
            mpp_err("mpp_init failed ret %d\n", ret);
            break;
        }
    
        // allocate success, setup default parameter
        m_type = type;
     // m_fps = 30;
        m_gop = 60;
        m_bps = width * height / 8 * m_fps;
        m_qp_init  = (type == MPP_VIDEO_CodingMJPEG) ? (10) : (26);

        m_prep_cfg.change        = MPP_ENC_PREP_CFG_CHANGE_INPUT |
                                   MPP_ENC_PREP_CFG_CHANGE_ROTATION |
                                   MPP_ENC_PREP_CFG_CHANGE_FORMAT;
        m_prep_cfg.width         = m_width  = width;
        m_prep_cfg.height        = m_height = height;
        m_prep_cfg.hor_stride    = m_hor_stride = MPP_ALIGN(width, 16);
        m_prep_cfg.ver_stride    = m_ver_stride = MPP_ALIGN(height, 16);
        m_prep_cfg.format        = m_fmt = MPP_FMT_ARGB8888; // MPP_FMT_YUV420SP_VU;
        m_prep_cfg.rotation      = MPP_ENC_ROT_0;

        // MPP_FMT_YUV420SP_VU
        // m_frame_size = m_hor_stride * m_ver_stride * 3 / 2;
        // MPP_FMT_YUV422_UYVY
        // m_frame_size = m_hor_stride * m_ver_stride
        m_frame_size = m_hor_stride * m_ver_stride * 4;

        mpp_log_f("%dx%d frame_size=%d\n", m_width, m_height, m_frame_size);

        if (MPP_OK != (ret = m_mpi->control(m_ctx, MPP_ENC_SET_PREP_CFG, &m_prep_cfg))) {
            mpp_err("mpi control enc set prep cfg failed ret %d\n", ret);
            break;
        }

        m_rc_cfg.change  = MPP_ENC_RC_CFG_CHANGE_ALL;
        m_rc_cfg.rc_mode = MPP_ENC_RC_MODE_VBR; /* MPP_ENC_RC_MODE_CBR */
        m_rc_cfg.quality = MPP_ENC_RC_QUALITY_MEDIUM; /* MPP_ENC_RC_QUALITY_CQP*/

        if (m_rc_cfg.quality == MPP_ENC_RC_QUALITY_CQP) {
            /* constant QP does not have bps */
            m_rc_cfg.bps_target   = -1;
            m_rc_cfg.bps_max      = -1;
            m_rc_cfg.bps_min      = -1;
        } else {
            /* variable bitrate has large bps range */
            m_rc_cfg.bps_target   = m_bps;
            m_rc_cfg.bps_max      = m_bps * 17 / 16;
            m_rc_cfg.bps_min      = m_bps * 1 / 16;
        }

        /* fix input / output frame rate */
        m_rc_cfg.fps_in_flex      = 0;
        m_rc_cfg.fps_in_num       = m_fps;
        m_rc_cfg.fps_in_denorm    = 1;
        m_rc_cfg.fps_out_flex     = 0;
        m_rc_cfg.fps_out_num      = m_fps;
        m_rc_cfg.fps_out_denorm   = 1;

        m_rc_cfg.gop              = m_gop;
        m_rc_cfg.skip_cnt         = 0;

        mpp_log("bps %d fps %d gop %d\n",
                m_rc_cfg.bps_target, m_rc_cfg.fps_out_num, m_rc_cfg.gop);

        if (MPP_OK != (ret = m_mpi->control(m_ctx, MPP_ENC_SET_RC_CFG, &m_rc_cfg))) {
            mpp_err("mpi control enc set rc cfg failed ret %d\n", ret);
            break;
        }

        m_codec_cfg.coding = type;
        switch (m_codec_cfg.coding) {
        case MPP_VIDEO_CodingAVC:
            m_codec_cfg.h264.change = MPP_ENC_H264_CFG_CHANGE_PROFILE |
                                      MPP_ENC_H264_CFG_CHANGE_ENTROPY |
                                      MPP_ENC_H264_CFG_CHANGE_TRANS_8x8 |
                                      MPP_ENC_H264_CFG_CHANGE_QP_LIMIT;
            /*
            * H.264 profile_idc parameter
            * 66  - Baseline profile
            * 77  - Main profile
            * 100 - High profile
            */
            m_codec_cfg.h264.profile  = 100;

            /*
            * H.264 level_idc parameter
            * 10 / 11 / 12 / 13    - qcif@15fps / cif@7.5fps / cif@15fps / cif@30fps
            * 20 / 21 / 22         - cif@30fps / half-D1@@25fps / D1@12.5fps
            * 30 / 31 / 32         - D1@25fps / 720p@30fps / 720p@60fps
            * 40 / 41 / 42         - 1080p@30fps / 1080p@30fps / 1080p@60fps
            * 50 / 51 / 52         - 4K@30fps
            */
            m_codec_cfg.h264.level    = 40;
            m_codec_cfg.h264.entropy_coding_mode  = 1;
            m_codec_cfg.h264.cabac_init_idc  = 0;
            m_codec_cfg.h264.transform8x8_mode = 1;

            if (m_rc_cfg.quality == MPP_ENC_RC_QUALITY_CQP) {
                /* constant QP mode qp is fixed */
                m_qp_max   = m_qp_init;
                m_qp_min   = m_qp_init;
                m_qp_step  = 0;
            } else if (m_rc_cfg.rc_mode == MPP_ENC_RC_MODE_CBR) {
                /* constant bitrate do not limit qp range */
                m_qp_max   = 48;
                m_qp_min   = 4;
                m_qp_step  = 16;
                m_qp_init  = 0;
            } else {
                /* variable bitrate has qp min limit */
                m_qp_max   = 40;
                m_qp_min   = 12;
                m_qp_step  = 8;
                m_qp_init  = 0;
            }
            m_codec_cfg.h264.qp_max      = m_qp_max;
            m_codec_cfg.h264.qp_min      = m_qp_min;
            m_codec_cfg.h264.qp_max_step = m_qp_step;
            m_codec_cfg.h264.qp_init     = m_qp_init;
            break;

        case MPP_VIDEO_CodingMJPEG:
            m_codec_cfg.jpeg.change = MPP_ENC_JPEG_CFG_CHANGE_QP;
            // m_codec_cfg.jpeg.quant  = m_qp_init;
            m_codec_cfg.jpeg.quant  = 10;
            break;

        case MPP_VIDEO_CodingVP8:
            // TODO
            break;

        case MPP_VIDEO_CodingHEVC:
            m_codec_cfg.h265.change = MPP_ENC_H265_CFG_INTRA_QP_CHANGE;
            m_codec_cfg.h265.intra_qp = 26;
            break;

        default:
            mpp_err_f("support encoder coding type %d\n", m_codec_cfg.coding);
            break;
        } // switch (type)

        if (MPP_OK != (ret = m_mpi->control(m_ctx, MPP_ENC_SET_CODEC_CFG, &m_codec_cfg))) {
            mpp_err("mpi control enc set codec cfg failed ret %d\n", ret);
            break;
        }

        /* optional */
        m_sei_mode = MPP_ENC_SEI_MODE_ONE_FRAME;
        if (MPP_OK != (ret = m_mpi->control(m_ctx, MPP_ENC_SET_SEI_CFG, &m_sei_mode))) {
            mpp_err("mpi control enc set sei cfg failed ret %d\n", ret);
            break;
        }

        /* gen and cfg osd plt */
        mpi_enc_gen_osd_plt(&m_osd_plt, m_plt_table);
        if (MPP_OK != (ret = m_mpi->control(m_ctx, MPP_ENC_SET_OSD_PLT_CFG, &m_osd_plt))) {
            mpp_err("mpi control enc set osd plt failed ret %d\n", ret);
            break;
        }

        // test_mpp_preprare(): get and write sps/pps for H.264
        if (m_type == MPP_VIDEO_CodingAVC) {
            MppPacket packet = NULL;
            if (MPP_OK != (ret = m_mpi->control(m_ctx, MPP_ENC_GET_EXTRA_INFO, &packet))) {
                mpp_err("mpi control enc get extra info failed\n");
                break;
            }

            /* get and write sps/pps for H.264 */
            if (packet) {
                void  *ptr  = mpp_packet_get_pos(packet);
                size_t len  = mpp_packet_get_length(packet);
              #if 0
                if (p->fp_output)
                    fwrite(ptr, 1, len, p->fp_output);
             #endif
                packet = NULL;
            }
        } // if (MPP_VIDEO_CodingAVC)
        
        return MPP_OK;
    }

    // something error
    deinit();

    return (int)ret;
}

void MppWrapper::deinit()
{
    MPP_RET ret;
    if (m_mpi) {
        if (MPP_OK != (ret = m_mpi->reset(m_ctx))) {
            mpp_err("mpi reset failed\n");
        }
    }

    if (m_ctx) {
        mpp_destroy(m_ctx);
        m_ctx = nullptr;
        m_mpi = nullptr;
    }
}

MppPacket MppWrapper::encode_get_packet(MppBuffer frmbuf, MppFrameFormat frmfmt)
{
    MPP_RET ret;
    MppPacket packet = nullptr;
    MppFrame  frame  = nullptr;
    void *pbuf = mpp_buffer_get_ptr(frmbuf);

    if (MPP_OK != (ret = mpp_frame_init(&frame))) {
        mpp_err_f("mpp_frame_init failed\n");
        return nullptr;
    }

    mpp_log_f("%ux%u %u:%u\n", m_width, m_height, m_hor_stride, m_ver_stride);

    mpp_frame_set_width(frame, m_width);
    mpp_frame_set_height(frame, m_height);
    mpp_frame_set_hor_stride(frame, m_hor_stride);
    mpp_frame_set_ver_stride(frame, m_ver_stride);
    mpp_frame_set_fmt(frame, frmfmt);
    mpp_frame_set_eos(frame, frmbuf == nullptr ? 1 : 0);

    mpp_frame_set_buffer(frame, frmbuf);

    if (MPP_OK != (ret = m_mpi->encode_put_frame(m_ctx, frame))) {
        mpp_err("mpp encode_put_frame: %d\n", ret);
        return nullptr;
    }

    int n = 0;
    do {
        if (MPP_OK != (ret = m_mpi->encode_get_packet(m_ctx, &packet))) {
            mpp_err("mpp encode_get_packet: %d\n", ret);
            return nullptr;
        }
        if (nullptr == packet) {
            mpp_err("encode_get_packet timeout %d\n", ++n);
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // usleep(1000);
        }
    } while (nullptr == packet);

    if (nullptr != packet) {
        //pkt_ptr = mpp_packet_get_pos(packet);
        //pkt_len = mpp_packet_get_length(packet);
        //pkt_eos = mpp_packet_get_eos(packet);
    }

    return packet;
}