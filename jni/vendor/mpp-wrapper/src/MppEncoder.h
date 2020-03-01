/*
 * $Id: $
 *
 * Encoder interface to Rockchip Media Process Platform (MPP)
 *
 * Copyright (c) 2019-2020 FoilPlanet. All rights reserved.
 *
 */

#ifndef _MINICAP_MPP_ENCODER_H_
#define _MINICAP_MPP_ENCODER_H_

#include "Minicap.hpp"

class MppWrapper;

/**
 * Encoder interface to Rockchip Media Process Platform (MPP)
 *  
 * Reuse api as minicap's JpgEncoder
 */
class MppEncoder
{
public:
    static const int VIDEO_CODING_JPEG;
    static const int VIDEO_CODING_AVC;

public:
    MppEncoder(unsigned int prePadding, unsigned int postPadding);
    
    virtual ~MppEncoder();

    bool encode(Minicap::Frame *frame, unsigned int quality);

    int getEncodedSize();

    unsigned char *getEncodedData();

    bool reserveData(uint32_t width, uint32_t height);

    int setEncodeCodec(int new_codec) {
        if (new_codec >= 0) {
            mEncodeCodec = new_codec;
        }
        return mEncodeCodec;
    }

    size_t getSyncPacket(unsigned char **ppkt);

private:
    MppWrapper     *mMppInstance;
    void           *mPacket;
    int             mEncodeCodec;
    unsigned long   mEncodedSize;
    unsigned int    mPrePadding;
    unsigned int    mPostPadding;
    unsigned int    mMaxWidth;
    unsigned int    mMaxHeight;
};

#endif /* _MINICAP_MPP_ENCODER_H_ */
