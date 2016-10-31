/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/scoped_vpx_codec.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CODEC__SCOPED_VPX_CODEC_H
#define _ASPIA_CODEC__SCOPED_VPX_CODEC_H

#include "aspia_config.h"

#include <memory>

#include "base/logging.h"

extern "C"
{
typedef struct vpx_codec_ctx vpx_codec_ctx_t;
}

struct VpxCodecDeleter
{
    void operator()(vpx_codec_ctx_t *codec);
};

typedef std::unique_ptr<vpx_codec_ctx_t, VpxCodecDeleter> ScopedVpxCodec;

#endif // _ASPIA_CODEC__SCOPED_VPX_CODEC_H
