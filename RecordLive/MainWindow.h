#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "AvRecordLive.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void OnBtnStartClicked();
    void OnBtnStopClicked();

private:
    Ui::MainWindow *ui;

    AVRecordLive m_av;
};

#endif // MAINWINDOW_H
