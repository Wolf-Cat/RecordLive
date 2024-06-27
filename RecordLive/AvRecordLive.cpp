#include "AvRecordLive.h"
#include <QDebug>

extern "C" {
#include "libavdevice/avdevice.h"
}

AVRecordLive::AVRecordLive(QObject *parent) : QObject(parent)
{
    
}

void AVRecordLive::Init()
{
    if (m_isInited) {
        return;
    }

    m_isInited = true;
    m_filePath = "Record.flv";
    m_videoWidth = 1280;
    m_videoHeight = 720;
    m_audioBitrate = 128000;
    m_offsetx = 100;
    m_offsety = 100;

    // 一定要注册，不然打不开设备，av_register_all和avdevice_register_all是默认注册的
    avdevice_register_all();
}

void AVRecordLive::Uinit()
{
    m_isInited = false;
}

void AVRecordLive::Start()
{
    if (OpenVideoDevice() < 0) {
        qDebug() << "Open Video device failed";
        return;
    }

    if (OpenVideoDevice() < 0) {
        qDebug() << "Open Audio device failed";
        return;
    }
}

void AVRecordLive::Stop()
{

}


// 打开视频设备：屏幕
// gdigrab, "desktop"

int AVRecordLive::OpenVideoDevice()
{
    int ret = -1;

    // 输入文件容器格式
    AVInputFormat *inputFmt = av_find_input_format("gdigrab");
    AVDictionary *options = nullptr;
    AVCodec *decoder = nullptr;

    av_dict_set(&options, "fps", QString::number(m_fps).toStdString().c_str(), NULL);

    // 打开输入媒体流并读取其头部信息
    if (avformat_open_input(&m_pVideoFmtCtx, "desktop", inputFmt, &options) != 0) {
        qDebug() << "Can not open video input stream";
        return -1;
    }

    if (avformat_find_stream_info(m_pVideoFmtCtx, NULL) < 0) {
        qDebug() << "Can not find video stream info:";
        return -1;
    }

    for (int i = 0; i < m_pVideoFmtCtx->nb_streams; ++i) {
        AVStream *stream = m_pVideoFmtCtx->streams[i];

        // AV_CODEC_ID_H264 = 27, AV_CODEC_ID_BMP = 78
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            qDebug () << "video Id:" << stream->codecpar->codec_id;
            decoder = avcodec_find_decoder(stream->codecpar->codec_id);
            if (decoder == NULL) {
                qDebug () << "Can not found decoder";
            }

            // 从视频流中拷贝参数到codecCtx
            m_videoDecodecCtx = avcodec_alloc_context3(decoder);
            ret = avcodec_parameters_to_context(m_videoDecodecCtx, stream->codecpar);
            if (ret < 0) {
                qDebug () << "Video codec parameters to context: retcode = " << ret;
                return -1;
            }

            m_videoIndex = i;
            break;
        }
    }

    // 根据编码器上下文打开
    if (avcodec_open2(m_videoDecodecCtx, decoder, nullptr) < 0) {
        qDebug() << "Can not open video codec";
        return -1;
    }

    // 源宽高像素格式
    m_videoSwsCtx = sws_getContext(m_videoDecodecCtx->width, m_videoDecodecCtx->height, m_videoDecodecCtx->pix_fmt,
                   m_videoWidth, m_videoHeight, AV_PIX_FMT_YUV420P,   // 目标宽高像素格式
                   SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

    // 源参数：2560,1440 , 28:AV_PIX_FMT_BGRA
    qDebug() << "swr video width, height: " << m_videoDecodecCtx->width << "," << m_videoDecodecCtx->height
             << "pix_fmt: " << m_videoDecodecCtx->pix_fmt;

    return ret;
}

int AVRecordLive::OpenAudioDevice()
{

}

void AVRecordLive::MuxerProcessThread()
{

}

void AVRecordLive::VideoRecordThread()
{

}

void AVRecordLive::AudioRecordThread()
{

}
