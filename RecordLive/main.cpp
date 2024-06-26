#include "MainWindow.h"
#include <QApplication>
#include <QDebug>

extern "C"
{

#include "libavutil/avutil.h"

}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    qDebug() << "FFmepg的版本：" << av_version_info();

    return a.exec();
}
