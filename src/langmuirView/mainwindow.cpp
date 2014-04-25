#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "langmuirviewer.h"

#include <QAction>
#include <QDebug>
#include <QStyle>
#include <QIcon>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_viewer = ui->qglwidget;
    init();
}

void MainWindow::init()
{
    setIcon(ui->actionExit, "application-exit", QStyle::SP_DialogCloseButton);
    setIcon(ui->actionStart, "media-playback-start", QStyle::SP_MediaPlay);
    setIcon(ui->actionStop, "media-playback-stop", QStyle::SP_MediaStop);
    setIcon(ui->actionOpen, "document-open", QStyle::SP_DialogOpenButton);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionOpen_triggered()
{
    qDebug() << "open clicked";
}

void MainWindow::on_actionStart_triggered()
{
    ui->actionStart->setEnabled(false);
    ui->actionStop->setDisabled(false);
}

void MainWindow::on_actionStop_triggered()
{
    ui->actionStart->setEnabled(true);
    ui->actionStop->setDisabled(true);
}

void MainWindow::setIcon(QAction *action, QString themeIcon, QStyle::StandardPixmap standardPixmap)
{
    if (action == NULL) {
        return;
    }
    if (QIcon::hasThemeIcon(themeIcon)) {
        action->setIcon(QIcon::fromTheme(themeIcon));
    }
    else {
        action->setIcon(this->style()->standardIcon(standardPixmap));
    }
}
