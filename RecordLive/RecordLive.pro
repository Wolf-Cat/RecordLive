#-------------------------------------------------
#
# Project created by QtCreator 2024-06-26T11:29:28
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = RecordLive
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

LIBS +=  -lquartz  -lole32 -lstrmiids -lVfw32 -loleaut32

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
        main.cpp \
        MainWindow.cpp \
    AvRecordLive.cpp \
    Utils.cpp

HEADERS += \
        MainWindow.h \
    AvRecordLive.h \
    Utils.h

FORMS += \
        Mainwindow.ui


win32 {

INCLUDEPATH += ../include/ffmpeg4.2.1 \

LIBS += -L$$PWD/../lib/ffmpeg4.2.1   \
        -lavutil  \
        -lavcodec \
        -lavdevice \
        -lavfilter \
        -lavformat \
        -lpostproc  \
        -lswresample \
        -lswscale   \
}
