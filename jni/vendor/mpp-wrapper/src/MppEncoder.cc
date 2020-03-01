/*
 * $Id: $
 *
 * Encoder interface to Rockchip Media Process Platform (MPP)
 *
 * Copyright (c) 2019-2020 FoilPlanet. All rights reserved.
 *
 */

#include <stdexcept>
#include <string.h>
#include <thread>
#include <chrono>

#include "MppEncoder.h"
#include "MppWrapper.h"

const int MppEncoder::VIDEO_CODING_JPEG = MPP_VIDEO_CodingMJPEG;
const int MppEncoder::VIDEO_CODING_AVC  = MPP_VIDEO_CodingAVC;

MppEncoder::MppEncoder(unsigned int prePadding, unsigned int postPadding)
  : mMppInstance(nullptr),
    mPacket(nullptr),
    mEncodeCodec(VIDEO_CODING_JPEG),
    mEncodedSize(0L),
    mPrePadding(prePadding),
    mPostPadding(postPadding),
    mMaxWidth(0),
    mMaxHeight(0)
{
    mMppInstance = new MppWrapper();
}

MppEncoder::~MppEncoder()
{
    if (mMppInstance) {
        if (mPacket) {
            mMppInstance->put_packet(mPacket);
        }
        mMppInstance->deinit();
        delete mMppInstance;
    }
}

MppFrameFormat convertFormat(Minicap::Format format)
{
    MppFrameFormat fmt = MPP_FMT_YUV420SP;

    switch (format) {
    case Minicap::FORMAT_RGBA_8888:
        fmt = MPP_FMT_ARGB8888;     // android ordering
        break;
    case Minicap::FORMAT_RGB_888:
        fmt = MPP_FMT_RGB888;
        break;
    case Minicap::FORMAT_RGB_565:
        fmt = MPP_FMT_RGB565;
        break;
    case Minicap::FORMAT_BGRA_8888:
        fmt = MPP_FMT_ABGR8888;     // android ordering
        break;

    default:
        mpp_err("Unsupport Minicap::Format %d\n", format);
        break;
    }

    return fmt;
}

#include <turbojpeg.h>
static tjhandle _handle = NULL;

size_t rgba2yuv(void *yuv, const void *rgb, size_t rgb_size, int w, int h)
{
    int pixelfmt  = TJPF_RGBA;
    int subsample = TJSAMP_420;
    int flags   = 0;
    int padding = 1;
    int ret     = 0;

    if (NULL == _handle) {
        _handle = tjInitCompress();
    }

    ret = tjEncodeYUV3(_handle,
                       (unsigned char *)rgb, w, 0, h, pixelfmt, 
                       (unsigned char *)yuv, padding, subsample, flags);
    if (ret < 0) {
        mpp_err("tjEncodeYUV3 failed: %s\n", tjGetErrorStr());
        return 0;
    }

    return tjBufSizeYUV2(w, padding, h, subsample);
}

static MppBuffer frmbuf = nullptr;

bool MppEncoder::encode(Minicap::Frame *frame, unsigned int quality)
{
    mpp_log_f("frame s:%d fmt:%d q:%u\n", frame->size, frame->format, quality);

    RK_S64 start = mpp_time();

    // TODO: use buffer allocated from Surfaceflinger (mpp_buffer_get_fd / MPP_BUFFER_TYPE_ION)
    while (nullptr == frmbuf) {
        if (nullptr == (frmbuf = mMppInstance->get_buffer())) {
            mpp_err_f("get_buffer timeout\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // usleep(1000);
            // return false;
        }
    }

    // mpp_log_f("stride: %u \n", frame->stride * frame->bpp);
    if (mPacket) {
        mMppInstance->put_packet(mPacket);
    }

    MppFrameFormat mppfmt = MPP_FMT_RGB888;
    if (mMppInstance->is_yuv(mEncodeCodec)) {
        // transform FORMAT_RGBA_8888 to YUV-I420
        int bsize = rgba2yuv(mpp_buffer_get_ptr(frmbuf), frame->data, frame->size,
                             mMppInstance->get_width(), mMppInstance->get_height());
        mppfmt = MPP_FMT_YUV420P;
    } else {
        // encoder format is same to frame (e.g. FORMAT_RGBA_8888)
        memcpy(mpp_buffer_get_ptr(frmbuf), frame->data, frame->size);
        mppfmt = convertFormat(frame->format);
    }

    // mpp_log_f("mpp format %d", mppfmt);

    mPacket = mMppInstance->encode_get_packet(frmbuf, mppfmt);
    // mMppInstance->put_buffer(frmbuf);

    if (mPacket != nullptr) {
        uint8_t *pkt_ptr = (uint8_t *)mpp_packet_get_pos(mPacket);
        size_t   pkt_len = mpp_packet_get_length(mPacket);
        // pkt_eos = mpp_packet_get_eos(packet);

        mpp_log_f("packet s:%u cost %lld us\n", pkt_len, mpp_time() - start);

        mEncodedSize = pkt_len;

        return true;
    }

    return false;
}

int MppEncoder::getEncodedSize()
{
    return mEncodedSize;
}

unsigned char *MppEncoder::getEncodedData()
{
    unsigned char *pbuf;
    pbuf = reinterpret_cast<unsigned char *>(mpp_packet_get_pos(mPacket));
    return pbuf + mPrePadding;
}

bool
MppEncoder::reserveData(uint32_t width, uint32_t height)
{
    if (width == mMaxWidth && height == mMaxHeight) {
        return false;
    }

    mMaxWidth  = MPP_ALIGN(width, 16);
    mMaxHeight = MPP_ALIGN(height, 16);

    mMppInstance->init(width, height, static_cast<MppCodingType>(mEncodeCodec));

    return true;
}

size_t MppEncoder::getSyncPacket(unsigned char **ppkt)
{
    MppPacket packet = mMppInstance->get_sync_packet();
    mpp_assert(ppkt != nullptr);
    if (packet != nullptr) {
        *ppkt = (unsigned char *)mpp_packet_get_pos(packet);
        return mpp_packet_get_length(packet);
    }
    return 0;
}