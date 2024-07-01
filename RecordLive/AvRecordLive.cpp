#include "AvRecordLive.h"
#include "Utils.h"
#include <QDebug>
#include <thread>
#include <QThread>

extern "C" {
#include "libavdevice/avdevice.h"
#include "libavutil/imgutils.h"
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
    m_fps = 5;
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

    InitVideoBuffer();
    InitAudioBuffer();

    if (m_state != RecordState::START) {
        m_state = RecordState::START;
        std::thread muxerThread(&AVRecordLive::MuxerProcessThread, this);
        muxerThread.detach();
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
            // Can not open encoder, encoder id =  27 ret =  -22,
            return ret;
        }

        // 将codecCtx 中的参数传给输出流
        ret = avcodec_parameters_from_context(videoStream->codecpar, m_outVideoEncodecCtx);
        if (ret < 0) {
            qDebug() << "out video avcodec_parameters_from_context failed";
            return -1;
        }
    }

    // 音频输出流信息
    if (m_pAudioFmtCtx->streams[m_audioIndex]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
        audioStream = avformat_new_stream(m_pOutFmtCtx, NULL);
        m_outAudioIndex = audioStream->index;

        AVCodec *audioEncoder = avcodec_find_decoder(AV_CODEC_ID_AAC);
        m_outAudioEnCodecCtx = avcodec_alloc_context3(audioEncoder);
        m_outAudioEnCodecCtx->sample_fmt = audioEncoder->sample_fmts ? audioEncoder->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
        m_outAudioEnCodecCtx->bit_rate = m_audioBitrate;
        m_outAudioEnCodecCtx->sample_rate = 44100;

        int supportSamplesRateCount = 0;
        if (audioEncoder->supported_samplerates) {  // array of supported audio samplerates, or NULL if unknown, array is terminated by 0
            m_outAudioEnCodecCtx->sample_rate = audioEncoder->supported_samplerates[0];
            for (int i = 0; audioEncoder->supported_samplerates[i]; ++i) {
                supportSamplesRateCount++;
                if (audioEncoder->supported_samplerates[i] == 44100) {
                    m_outAudioEnCodecCtx->sample_rate = 44100;
                }
            }
        }

        qDebug() << "supportSamplesRateCount : " << supportSamplesRateCount;

        m_outAudioEnCodecCtx->channels = av_get_channel_layout_nb_channels(m_outAudioEnCodecCtx->channel_layout);
        m_outAudioEnCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;   // 立体声

        int channelLayouts = 0;
        if (audioEncoder->channel_layouts) {
            m_outAudioEnCodecCtx->channel_layout = audioEncoder->channel_layouts[0];
            for (int i = 0; audioEncoder->channel_layouts[i]; ++i) {
                if (audioEncoder->channel_layouts[i] == AV_CH_LAYOUT_STEREO) {
                    m_outAudioEnCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
                    ++channelLayouts;
                }
            }
        }

        qDebug() << "audioEncoder->channel_layouts[i]:" << channelLayouts;

        m_outAudioEnCodecCtx->channels = av_get_channel_layout_nb_channels(m_outAudioEnCodecCtx->channel_layout);
        audioStream->time_base = AVRational {1, m_outAudioEnCodecCtx->sample_rate};

        m_outAudioEnCodecCtx->codec_tag = 0;
        m_outAudioEnCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        audioEncoder->sample_fmts;
        m_outAudioEnCodecCtx->sample_fmt;
        if (!CheckSampleFmt(audioEncoder, m_outAudioEnCodecCtx->sample_fmt)) {
            qDebug() << "Audio Encoder does not support sample format:" << av_get_sample_fmt_name(m_outAudioEnCodecCtx->sample_fmt);
            return -1;
        }

        // 打开音频编码器
        ret = avcodec_open2(m_outAudioEnCodecCtx, audioEncoder, NULL);
        if (ret < 0) {
            qDebug() << "Can not open the audio encoder, codecid=" << audioEncoder->id << " error code:" << ret;
            return ret;
        }

        // 将CodecCtx中的参数传给音频输出流
        ret = avcodec_parameters_from_context(audioStream->codecpar, m_outAudioEnCodecCtx);
        if (ret < 0) {
            qDebug() << "Output audio avcodec_parameters_from_context failed err code:" << ret;
            return ret;
        }

        // 音频重采样初始化
        m_audioSwrCtx = swr_alloc();
        av_opt_set_int(m_audioSwrCtx, "in_channel_count", m_audioDecodecCtx->channels, 0);
        av_opt_set_int(m_audioSwrCtx, "in_sample_rate", m_audioDecodecCtx->sample_rate, 0);
        av_opt_set_sample_fmt(m_audioSwrCtx, "in_sample_fmt", m_audioDecodecCtx->sample_fmt, 0);

        av_opt_set_int(m_audioSwrCtx, "out_channel_count", m_outAudioEnCodecCtx->channels, 0);
        av_opt_set_int(m_audioSwrCtx, "out_sample_rate", m_outAudioEnCodecCtx->sample_rate, 0);
        av_opt_set_int(m_audioSwrCtx, "out_sample_fmt", m_outAudioEnCodecCtx->sample_fmt, 0);

        ret = swr_init(m_audioSwrCtx);
        if (ret < 0) {
            qDebug() << "swr init failed: ret = " << ret;
            return ret;
        }
    }

    // 打开输出文件
    if (m_pOutFmtCtx->oformat->flags & AVFMT_NOFILE) {
        if (avio_open(&m_pOutFmtCtx->pb, outFilePath, AVIO_FLAG_WRITE) < 0) {
            qDebug() << "avio_open failed, outFilePath:" << outFilePath;
            return -1;
        }
    }

    // 写文件头
    if (avformat_write_header(m_pOutFmtCtx, NULL) < 0) {
        qDebug() << "Can not write the header of the output file";
        return -1;
    }

    return ret;
}

bool AVRecordLive::CheckSampleFmt(const AVCodec *codec, AVSampleFormat sampleFmt)
{
    const enum AVSampleFormat *pSampleFmt = codec->sample_fmts;

    while (*pSampleFmt != AV_SAMPLE_FMT_NONE) {
        if (*pSampleFmt == sampleFmt) {
            return true;
        }

        pSampleFmt++;
    }

    return false;
}

void AVRecordLive::MuxerProcessThread()
{
    std::thread videoRecordThread(&AVRecordLive::VideoRecordThread, this);
    std::thread audioRecordThread(&AVRecordLive::AudioRecordThread, this);
    videoRecordThread.detach();
    audioRecordThread.detach();

    QThread::msleep(200);

    bool isDone = false;
    int videoFrameIndex = 0;
    int audioFrameIndex = 0;
    int ret = -1;

    // 从共享队列中读取数据，然后编码，存储或直播推流
    while(1) {
        if (m_state == RecordState::STOP && !isDone) {
            isDone = true;
        }

        if (isDone) {
            // 用这个defer_lock的前提是你不能自己先把mutex给lock()，否则会报异常
            // defer_lock的意思就是并没有给mutex加锁，只初始化了一个没有加锁的mutex
            std::unique_lock<std::mutex> vBufLock(m_mutexVideoBuf, std::defer_lock);
            std::unique_lock<std::mutex> aBufLock(m_mutexAudioBuf, std::defer_lock);

            // std::lock()可以一次锁住两个或者两个以上的互斥量。(最少锁两个)
            // 优点：它不存在这种因为多个线程中因为锁的顺序问题导致死锁的风险问题。
            std::lock(vBufLock, aBufLock);
            if (av_fifo_size(m_videoFifoBuffer) < m_videoOutFrameSize &&
                 av_audio_fifo_size(m_audioFifoBuffer) < m_outnbSamplesPeerFrame)
            {
                qDebug() << "Both video and audio fifo buf are empty";
                break;
            }

            // 比较音视频pts, 大于0表示视频帧在前，音频还需要连续编码
            // 小于0表示，音频帧在前，此时至少编码一帧视频
            if (av_compare_ts(m_videoCurPts, m_pOutFmtCtx->streams[m_outVideoIndex]->time_base,
                              m_audioCurPts, m_pOutFmtCtx->streams[m_outAudioIndex]->time_base) <= 0)
            {
                if (isDone) {
                    std::lock_guard<std::mutex> lockguard(m_mutexVideoBuf);
                    if (av_fifo_size(m_videoFifoBuffer) < m_videoOutFrameSize) {
                        m_videoCurPts = INT_MAX;   // 0x7ffffffffffffffe
                        continue;
                    }
                } else {
                    std::unique_lock<std::mutex> uniqueLock(m_mutexVideoBuf);
                    m_cvVideoBufNotEmpty.wait(uniqueLock, [this] {return av_fifo_size(m_videoFifoBuffer) >= m_videoOutFrameSize;});
                }

                // 从队列中拿出一帧，开始编码
                av_fifo_generic_read(m_videoFifoBuffer, m_videoOutFrameBuf, m_videoOutFrameSize, NULL);

                // 已拿出一帧，队列肯定不满了
                m_cvVideoBufNotFull.notify_one();

                // 设置视频帧参数
                m_videoOutFrame->pts = videoFrameIndex++;
                m_videoOutFrame->format = m_outVideoEncodecCtx->pix_fmt;
                m_videoOutFrame->width = m_outVideoEncodecCtx->width;
                m_videoOutFrame->height = m_outVideoEncodecCtx->height;

                AVPacket outPacket;
                av_init_packet(&outPacket);

                // 将拿出来的YUV420P帧，设定pts发给编码器
                ret = avcodec_send_frame(m_outVideoEncodecCtx, m_videoOutFrame);
                if (ret != 0) {
                    qDebug () << "out video avcodec_send_frame failed, ret=" <<ret;
                    av_packet_unref(&outPacket);
                }

                // 从编码器读取编码压缩后的包AVPacket
                ret = avcodec_receive_packet(m_outVideoEncodecCtx, &outPacket);
                if (ret != 0) {
                    qDebug() << "video avcodec_receive_packet failed, ret:" << ret;
                    av_packet_unref(&outPacket);
                    continue;
                }

                outPacket.stream_index = m_outVideoIndex;

                // 将pts从编码层的timebase转成封装层的time_base
                // 该函数通过计算两个时间基之间的比例，对AVPacket中的pts(显示时间戳)和dts(解码时间戳)进行相应的缩放。
                // 这对于确保媒体处理管道中各环节的时间戳一致性至关重要
                av_packet_rescale_ts(&outPacket, m_outVideoEncodecCtx->time_base, m_pOutFmtCtx->streams[m_outVideoIndex]->time_base);
                m_videoCurPts = outPacket.pts;
                qDebug() << "m_videoCurPts:" << m_videoCurPts;

                // 向数据包写入输出的媒体文件
                ret = av_interleaved_write_frame(m_pOutFmtCtx, &outPacket);
                if (ret != 0) {
                    qDebug() << "video av_interleaved_write_frame failed, ret:" << ret;
                }

                av_packet_unref(&outPacket);

            } else {    // 视频帧pts，在前面，此时音频需要编码
                if (isDone) {
                    std::lock_guard<std::mutex> lk(m_mutexAudioBuf);
                    if (av_audio_fifo_size(m_audioFifoBuffer) < m_outnbSamplesPeerFrame) {
                        m_audioCurPts = INT_MAX;
                        continue;
                    }
                } else {
                    std::unique_lock<std::mutex> lk(m_mutexAudioBuf);
                    m_cvAudioBufNotEmpty.wait(lk, [this] {return av_audio_fifo_size(m_audioFifoBuffer) >= m_outnbSamplesPeerFrame;});
                }

                int ret = -1;
                AVFrame *audioFrame = av_frame_alloc();
                audioFrame->nb_samples = m_outnbSamplesPeerFrame;
                audioFrame->channel_layout = m_outAudioEnCodecCtx->channel_layout;
                audioFrame->format = m_outAudioEnCodecCtx->sample_fmt;
                audioFrame->sample_rate = m_outAudioEnCodecCtx->sample_rate;
                audioFrame->pts = m_outnbSamplesPeerFrame * audioFrameIndex++;

                // 分配databuf
                ret = av_frame_get_buffer(audioFrame, 0);
                av_audio_fifo_read(m_audioFifoBuffer, (void **)audioFrame->data, m_outnbSamplesPeerFrame);
                m_cvAudioBufNotFull.notify_one();

                AVPacket pkt;
                av_init_packet(&pkt);
                ret = avcodec_send_frame(m_outAudioEnCodecCtx, audioFrame);
                if (ret != 0) {
                    /*
                    AVERROR（EAGAIN）值为-11.
                    返回值为-11意味着需要新的输入数据才能返回新的输出。AAC默认是两帧
                    在解码或编码开始时，编解码器可能会接收多个输入帧/数据包而不返回帧，直到其内部缓冲区被填充为止。
                    AVERROR(EINVAL): 值为-22. 意味着编码器打开失败
                    */
                    qDebug() << "audio avcodec_send_frame failed, ret: " << ret;
                    av_frame_free(&audioFrame);
                    av_packet_unref(&pkt);
                    continue;
                }

                pkt.stream_index = m_outAudioIndex;

                av_packet_rescale_ts(&pkt, m_outAudioEnCodecCtx->time_base, m_pOutFmtCtx->streams[m_outAudioIndex]->time_base);
                m_audioCurPts = pkt.pts;

                ret = av_interleaved_write_frame(m_pOutFmtCtx, &pkt);
                if (ret != 0) {
                    qDebug() << "audio av_interleaved_write_frame failed, ret: " << ret;
                }

                av_frame_free(&audioFrame);
                av_packet_unref(&pkt);
            }
        }
    }
}

void AVRecordLive::VideoRecordThread()
{
    int ret = -1;
    AVPacket pkt;
    av_init_packet(&pkt);

    int pixNumPeerFrame = m_videoWidth * m_videoHeight;
    AVFrame *oldFrame = av_frame_alloc();  // 桌面视频帧（bmp）送给解码器，解压之后的AVFrame(rgb)
    AVFrame *newFrame = av_frame_alloc();  // 颜色空间转换，缩放

    // 转为的YUV420P一帧的字节数
    int newFrameBufSize = av_image_get_buffer_size(m_outVideoEncodecCtx->pix_fmt, m_videoWidth, m_videoHeight, 1);
    uint8_t *newFrameBuf = (uint8_t *)av_malloc(newFrameBufSize);
    av_image_fill_arrays(newFrame->data, newFrame->linesize, newFrameBuf,
                 m_outVideoEncodecCtx->pix_fmt, m_videoWidth, m_videoHeight, 1);

    // 循环处理屏幕的每一帧数据
    while (m_state != RecordState::STOP) {
        ret = av_read_frame(m_pVideoFmtCtx, &pkt);
        if (ret < 0) {
            qDebug() << "VideoRecordThread av_read_frame < 0, ret = " << ret;
            continue;
        }

        if (pkt.stream_index != m_videoIndex) {
            qDebug() << "read not a video packet";
            av_packet_unref(&pkt);
            continue;
        }

        // 将包发给解码器
        ret = avcodec_send_packet(m_videoDecodecCtx, &pkt);
        if (ret != 0) {
            qDebug() << "video avcodec_send_packet failed, ret=" << ret;
            av_packet_unref(&pkt);
            continue;
        }

        // 从解码器拿到解码数据帧AVFrame后（不是YUV420P, 桌面帧格式是RGBA）
        ret = avcodec_receive_frame(m_videoDecodecCtx, oldFrame);
        if (ret != 0) {
            qDebug() << "video avcodec_receive_frame failed, ret:" << ret;
            av_packet_unref(&pkt);
            continue;
        }

        // 颜色空间转换，将AV_PIX_FMT_RGBA转为AV_PIX_FMT_YUV420P
        sws_scale(m_videoSwsCtx, (const uint8_t* const*)oldFrame->data, oldFrame->linesize, 0,
                  m_outVideoEncodecCtx->height, newFrame->data, newFrame->linesize);

        {
            std::unique_lock<std::mutex> uniquelk(m_mutexVideoBuf);
            // 条件：判断队列中是否足够容纳一帧大小的空间 （之前设置的是30帧的大小缓存）
            // 如果不够则一直阻塞等待
            m_cvVideoBufNotFull.wait(uniquelk, [this] {
                return av_fifo_space(m_videoFifoBuffer) >= m_videoOutFrameSize;
            });
        }

        av_fifo_generic_write(m_videoFifoBuffer, newFrame->data[0], pixNumPeerFrame, NULL);
        av_fifo_generic_write(m_videoFifoBuffer, newFrame->data[1], pixNumPeerFrame / 4, NULL);
        av_fifo_generic_write(m_videoFifoBuffer, newFrame->data[1], pixNumPeerFrame / 4, NULL);

        // 此时已经写入一帧数据，队列不为空
        m_cvVideoBufNotEmpty.notify_one();

        av_packet_unref(&pkt);
    }

    // TODO: 线程结束时候的剩余数据的释放问题
}

void AVRecordLive::AudioRecordThread()
{
    int ret = -1;
    AVPacket pkt;
    av_init_packet(&pkt);

    int nbSamples = m_outnbSamplesPeerFrame;
    int dstNbSamples = 0;
    int maxDstNbSamples = 0;

    AVFrame *oldFrame = av_frame_alloc();
    AVFrame *newFrame;

    // 重新计算(rescale)并且取整(round)方便我们记忆 a * b / c
    maxDstNbSamples = av_rescale_rnd(nbSamples, m_outAudioEnCodecCtx->sample_rate,
                      m_audioDecodecCtx->sample_rate, AV_ROUND_UP);
    dstNbSamples = maxDstNbSamples;

    while (m_state != RecordState::STOP) {
        if (av_read_frame(m_pAudioFmtCtx, &pkt) < 0) {
            qDebug() << "audio av_read_frame < 0";
            continue;
        }

        if (pkt.stream_index != m_audioIndex) {
            av_packet_unref(&pkt);
            continue;
        }

        ret = avcodec_send_packet(m_audioDecodecCtx, &pkt);
        if (ret < 0) {
            qDebug() << "audio avcodec_send_packet failed, ret:" << ret;
            av_packet_unref(&pkt);
            continue;
        }

        ret = avcodec_receive_frame(m_audioDecodecCtx, oldFrame);
        if (ret < 0) {
            qDebug() << "audio avcodec_receive_frame failed, ret: " << ret;
            av_packet_unref(&pkt);
            continue;
        }

        // swr_get_delay, 计算积压的延迟数据
        dstNbSamples = av_rescale_rnd(swr_get_delay(m_audioSwrCtx, m_audioDecodecCtx->sample_rate) + oldFrame->nb_samples,
                    m_outAudioEnCodecCtx->sample_rate, m_audioDecodecCtx->sample_rate, AV_ROUND_UP);

        if (dstNbSamples > maxDstNbSamples) {
            qDebug() << "audio new frame realloc";
            av_freep(&newFrame->data[0]);
            ret = av_samples_alloc(newFrame->data, newFrame->linesize, m_outAudioEnCodecCtx->channels,
                                  dstNbSamples, m_outAudioEnCodecCtx->sample_fmt, 1);
            if (ret < 0) {
                qDebug() << "av_samples_alloc failed";
                return;
            }

            maxDstNbSamples = dstNbSamples;
            m_outAudioEnCodecCtx->frame_size = dstNbSamples;
            m_outnbSamplesPeerFrame = newFrame->nb_samples; // 1024
        }

        // 音频重采样
        newFrame->nb_samples = swr_convert(m_audioSwrCtx, newFrame->data, dstNbSamples,
                       (const uint8_t **)oldFrame->data, oldFrame->nb_samples);

        if (newFrame->nb_samples < 0) {
            qDebug() << "swr_convert failed err:" << newFrame->nb_samples;
            return;
        }

        {
            std::unique_lock<std::mutex> uniquelk(m_mutexAudioBuf);
            m_cvAudioBufNotFull.wait(uniquelk, [newFrame, this] {
                return av_audio_fifo_space(m_audioFifoBuffer) >= newFrame->nb_samples;
            });
        }

        if (av_audio_fifo_write(m_audioFifoBuffer, (void **)newFrame->data, newFrame->nb_samples) < newFrame->nb_samples) {
            qDebug() << "av_audio_fifo_write failed";
            return;
        }

        m_cvAudioBufNotEmpty.notify_one();
    }
}

void AVRecordLive::InitVideoBuffer()
{
    m_videoOutFrameSize = av_image_get_buffer_size(m_outVideoEncodecCtx->pix_fmt,
                                                   m_videoWidth, m_videoHeight, 1);
    m_videoOutFrameBuf = (uint8_t *)av_malloc(m_videoOutFrameSize);
    m_videoOutFrame = av_frame_alloc();

    // 先让AVFrame指针指向buf, 后面再写入数据到buf
    av_image_fill_arrays(m_videoOutFrame->data, m_videoOutFrame->linesize, m_videoOutFrameBuf,
                         m_outVideoEncodecCtx->pix_fmt, m_videoWidth, m_videoHeight, 1);
    // 申请30帧缓存
    m_videoFifoBuffer = av_fifo_alloc_array(30, m_videoOutFrameSize);
}

void AVRecordLive::InitAudioBuffer()
{
    m_outnbSamplesPeerFrame = m_outAudioEnCodecCtx->frame_size;
    if (m_outnbSamplesPeerFrame == 0) {
        m_outnbSamplesPeerFrame = 1024;
    }

    // 申请30帧缓存
    m_audioFifoBuffer = av_audio_fifo_alloc(m_outAudioEnCodecCtx->sample_fmt,
                                            m_outAudioEnCodecCtx->channels, 30 * m_outnbSamplesPeerFrame);
}
