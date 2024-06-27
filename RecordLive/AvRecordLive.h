#ifndef AVRECORDLIVE_H
#define AVRECORDLIVE_H

#include <QObject>

extern "C" {

#include "libavformat/avformat.h"
}

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
    int OpenVideoDevice();
    int OpenAudioDevice();

    void MuxerProcessThread();   // 音频和视频复用线程
    void VideoRecordThread();    // 视频流录制线程
    void AudioRecordThread();    // 音频流录制线程

private:
    bool m_isInited = false;

    QString m_filePath;
    int m_videoWidth = 0;
    int m_videoHeight = 0;
    int m_fps = 0;
    int m_audioBitrate = 0;
    int m_offsetx = 0;    // 抓取视频左上角点的X
    int m_offsety = 0;        // 抓取视频左上角点的Y

    AVFormatContext *m_pVideoFmtCtx = NULL;
    AVFormatContext *m_pAudioFmtCtx = NULL;
    AVFormatContext *m_pOutFmtCtx = NULL;     // 输出文件

    AVCodecContext *m_videoDecodecCtx = NULL;
    AVCodecContext *m_audioDecodecCtx = NULL;

    int m_videoIndex = -1;
    int m_audioIndex = -1;
};

#endif // AVRECORDLIVE_H
