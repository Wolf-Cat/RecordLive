#ifndef AVRECORDLIVE_H
#define AVRECORDLIVE_H

#include <QObject>
#include <mutex>
#include <condition_variable>

extern "C" {

#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/audio_fifo.h"

}

enum RecordState {
    UNKONOW = -1,
    START,
    PAUSED,
    STOP
};

class AVRecordLive : public QObject
{
    Q_OBJECT

public:
    explicit AVRecordLive(QObject *parent = nullptr);
    void Init();
    void Uinit();

signals:

public slots:
    void Start();
    void Stop();

private:
    int OpenVideoDevice();   // 打开摄像头
    int OpenAudioDevice();   // 打开麦克风
    int OpenOutput();        // 初始化输出流

    bool CheckSampleFmt(const AVCodec *codec, enum AVSampleFormat sampleFmt);
    void MuxerProcessThread();   // 音频和视频复用线程
    void VideoRecordThread();    // 视频流录制线程
    void AudioRecordThread();    // 音频流录制线程

    void InitVideoBuffer();
    void InitAudioBuffer();

private:
    bool m_isInited = false;
    RecordState m_state;

    QString m_filePath;
    int m_videoWidth = 0;
    int m_videoHeight = 0;
    int m_fps = 0;
    int m_audioBitrate = 0;
    int m_offsetx = 0;    // 抓取视频左上角点的X
    int m_offsety = 0;        // 抓取视频左上角点的Y

// 输入流相关
    AVFormatContext *m_pVideoFmtCtx = NULL;
    AVFormatContext *m_pAudioFmtCtx = NULL;

    AVCodecContext *m_videoDecodecCtx = NULL;
    AVCodecContext *m_audioDecodecCtx = NULL;

    SwsContext *m_videoSwsCtx = NULL;   // 视频颜色空间转换，缩放
    SwrContext *m_audioSwrCtx = NULL;   // 音频重采样

    int m_videoIndex = -1;    // 桌面输入视频流索引
    int m_audioIndex = -1;    // 麦克风输入音频流索引

    AVFifoBuffer *m_videoFifoBuffer = NULL;   // 视频共享队列
    std::condition_variable m_cvVideoBufNotFull;
    std::condition_variable m_cvVideoBufNotEmpty;
    std::mutex m_mutexVideoBuf;

    AVAudioFifo *m_audioFifoBuffer = NULL;    // 音频共享队列
    std::condition_variable m_cvAudioBufNotFull;
    std::condition_variable m_cvAudioBufNotEmpty;
    std::mutex m_mutexAudioBuf;

// 输出流相关
    AVFormatContext *m_pOutFmtCtx = NULL;     // 输出文件
    int m_outVideoIndex= -1;    // 输出文件的上下文中视频流索引
    int m_outAudioIndex = -1;   // 输出文件的上下文中音频流索引

    AVCodecContext *m_outVideoEncodecCtx = NULL;   // 视频输出流编码器上下文
    AVCodecContext *m_outAudioEnCodecCtx = NULL;   // 音频输出流编码器上下文

    // 输出的视频方面
    AVFrame *m_videoOutFrame = NULL;
    uint8_t *m_videoOutFrameBuf = NULL;
    int m_videoOutFrameSize = 0;
    int64_t m_videoCurPts = 0;

    // 输出的音频方面
    AVAudioFifo *m_outAudioFifoBuffer = NULL;
    int m_outnbSamplesPeerFrame = 0;
    int64_t m_audioCurPts = 0;
};

#endif // AVRECORDLIVE_H
