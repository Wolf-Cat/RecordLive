#ifndef AVRECORDLIVE_H
#define AVRECORDLIVE_H

#include <QObject>

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
    bool m_isInited = false;

    QString m_filePath;
    int m_videoWidth = 0;
    int m_videoHeight = 0;
    int m_fps = 0;
    int m_offsetx = 0;    // 抓取视频左上角点的X
    int m_offsety = 0;        // 抓取视频左上角点的Y
};

#endif // AVRECORDLIVE_H
