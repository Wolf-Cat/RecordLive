#include "MainWindow.h"
#include "ui_Mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->startBtn, &QPushButton::clicked, this, &MainWindow::OnBtnStartClicked);
    connect(ui->stopBtn, &QPushButton::clicked, this, &MainWindow::OnBtnStopClicked);

    ui->startBtn->setEnabled(true);
    ui->stopBtn->setEnabled(false);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::OnBtnStartClicked()
{
    ui->startBtn->setEnabled(false);
    ui->stopBtn->setEnabled(true);

    m_av.Init();
    m_av.Start();
}

void MainWindow::OnBtnStopClicked()
{
    ui->startBtn->setEnabled(true);
    ui->stopBtn->setEnabled(false);

    m_av.Stop();
}
