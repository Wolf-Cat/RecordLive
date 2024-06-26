#include "AvRecordLive.h"

AVRecordLive::AVRecordLive(QObject *parent) : QObject(parent)
{
    
}

void AVRecordLive::Init()
{
    if (m_isInited) {
        return;
    }

    m_isInited = true;
}

void AVRecordLive::Uinit()
{
    m_isInited = false;
}

void AVRecordLive::Start()
{
    
}

void AVRecordLive::Stop()
{
    
}
