#include "AvRecordLive.h"
#include "Utils.h"
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

    if (OpenAudioDevice() < 0) {
        qDebug() << "Open Audio device failed";
        return;
    }

    if (OpenOutput() < 0) {
        qDebug() << "Open OpenOutput device failed";
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
    int ret = -1;
    AVCodec *decoder = NULL;

    qDebug() << "MicrophoneDeviceName: " << Utils::GetMicrophoneDeviceName();

    // MicrophoneDeviceName:  "麦克风 (USB2.0 MIC)"
    AVInputFormat *inputFmt = av_find_input_format("dshow");
    QString micphoneName = "audio=" + Utils::GetMicrophoneDeviceName();

    ret = avformat_open_input(&m_pAudioFmtCtx, micphoneName.toStdString().c_str(), inputFmt, NULL);
    if (ret < 0) {
        qDebug() << "Can not open audio input stream";
        return ret;
    }

    if (avformat_find_stream_info(m_pAudioFmtCtx, NULL) < 0) {
        return -1;
    }

    for (int i = 0; i < m_pAudioFmtCtx->nb_streams; ++i) {
        AVStream *stream = m_pAudioFmtCtx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            decoder = avcodec_find_decoder(stream->codecpar->codec_id);
            qDebug() << "Audio decoder id: " << stream->codecpar->codec_id << " name: " << decoder->name;
            // Audio decoder id:  65536  name:  pcm_s16le

            if (decoder == NULL) {
                qDebug() << "Codec not found";
                return -1;
            }

            m_audioDecodecCtx = avcodec_alloc_context3(decoder);
            ret = avcodec_parameters_to_context(m_audioDecodecCtx, stream->codecpar);
            if (ret < 0) {
                qDebug() << "Audio avcodec_parameters_to_context failed, errCode = " << ret;
                return ret;
            }

            m_audioIndex = i;
            break;
        }
    }

    if (avcodec_open2(m_audioDecodecCtx, decoder, NULL) < 0) {
        qDebug() << "Can not find or open audio decoder";
        return -1;
    }

    return ret;
}

int AVRecordLive::OpenOutput()
{
    int ret = -1;

    AVStream *videoStream = NULL;
    AVStream *audioStream = NULL;

    const char *outFilePath = m_filePath.toStdString().c_str();
    if (m_filePath.toLower().endsWith(".flv") ||             // FLV存储或者RTMP推流
         m_filePath.toLower().startsWith("rtmp://")) {
        ret = avformat_alloc_output_context2(&m_pOutFmtCtx, NULL, "flv", outFilePath);
    } else {
        // 其余格式：MP4, ts, mkv等
        ret = avformat_alloc_output_context2(&m_pOutFmtCtx, NULL, NULL, outFilePath);
    }

    if (ret < 0) {
        qDebug() << "avformat_alloc_output_context failed";
        return -1;
    }

    // 根据视频信息获取
    if (m_pVideoFmtCtx->streams[m_videoIndex]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
        videoStream = avformat_new_stream(m_pOutFmtCtx, NULL);
        if (videoStream == NULL) {
            qDebug() << "can not new video stream for output";
            return -1;
        }

        // AVFormatContext第一个创建的流索引是0, 后续 + 1
        m_outVideoIndex = videoStream->index;

        // 视频流的时间基，一般都取帧率的倒数
        videoStream->time_base = AVRational {1, m_fps};

        m_outVideoEncodecCtx = avcodec_alloc_context3(NULL);

        m_outVideoEncodecCtx->width = m_videoWidth;
        m_outVideoEncodecCtx->height = m_videoHeight;
        m_outVideoEncodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;

        // 编码器的时间基： 帧率的倒数
        m_outVideoEncodecCtx->time_base.num = 1;
        m_outVideoEncodecCtx->time_base.den = m_fps;

        m_outVideoEncodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
        m_outVideoEncodecCtx->codec_id = AV_CODEC_ID_H264;

        m_outVideoEncodecCtx->bit_rate = 800 * 1000;   // the average bitrate
        m_outVideoEncodecCtx->rc_max_rate = 800 * 1000;
        m_outVideoEncodecCtx->rc_buffer_size = 500 * 1000;

        // 设置图像组层的大小，gop_size 越大，文件越小，因为gop越大，就会使得整个文件的I帧越少
        m_outVideoEncodecCtx->gop_size = 30;

        // maximum number of B-frames between non-B-frames
        // Note: The output will be delayed by max_b_frames+1 relative to the input.
        m_outVideoEncodecCtx->max_b_frames = 3;

        // 设置H264中相关的参数，不设置的话 avcodec_open2会失败
        m_outVideoEncodecCtx->qmin = 10;
        m_outVideoEncodecCtx->qmax = 31;
        m_outVideoEncodecCtx->max_qdiff = 4;
        m_outVideoEncodecCtx->me_range = 16;
        m_outVideoEncodecCtx->max_qdiff = 4;
        m_outVideoEncodecCtx->qcompress = 0.6;

        // 根据编码器ID查找对应的视频编码器
        AVCodec *videoEncoder = avcodec_find_encoder(m_outVideoEncodecCtx->codec_id);
        m_outVideoEncodecCtx->codec_tag = 0;

        // 正确设置sps/pps
        m_outVideoEncodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;  // Place global headers in extradata instead of every keyframe.

        // 打开视频编码器
        ret = avcodec_open2(m_outVideoEncodecCtx, videoEncoder, NULL);
        if (ret < 0) {
            qDebug() << "Can not open encoder, encoder id = " << videoEncoder->id << "ret = " << ret;
            return ret;
        }

        // 将codecCtx 中的参数传给输出流
        ret = avcodec_parameters_from_context(videoStream->codecpar, m_outVideoEncodecCtx);
        if (ret < 0) {
            qDebug() << "out video avcodec_parameters_from_context failed";
            return -1;
        }
    }

    return ret;
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
