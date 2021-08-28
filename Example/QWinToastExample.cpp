#include "QWinToastExample.h"
#include <QMessageBox>

QWinToastExample::QWinToastExample(QWidget *parent) : QWidget(parent)
{
    mainLayout = new QVBoxLayout(this);
    QPushButton *btn1 = new QPushButton("Btn");
    mainLayout->addWidget(btn1);

    connect(btn1, &QPushButton::clicked, this, &QWinToastExample::on_btn_clicked);
}

QWinToastExample::~QWinToastExample()
{
}

void QWinToastExample::on_btn_clicked()
{
    QWinToast* toast = QWinToast::instance();
	toast->setAppName("A-Soul");
    toast->setAppUserModelID(
        QWinToast::configureAUMI("skykey", "qwintoast", "qwintoastexample", "20210828"));
    if (!toast->initialize()) {
        qDebug() << "Error, your system in not compatible!";
    }

    QWinToastTemplate templ = QWinToastTemplate(QWinToastTemplate::ImageAndText04);
    templ.setImagePath("D:/Diana.png");
    templ.setTextField("遇到困难，摆大烂", QWinToastTemplate::FirstLine);
    templ.setTextField("关注嘉然，顿顿解馋", QWinToastTemplate::SecondLine);
    templ.setTextField("嘉心糖屁用没有", QWinToastTemplate::ThirdLine);
    templ.setExpiration(1 * 1000);
    templ.setAudioPath(QWinToastTemplate::AudioSystemFile::Mail);
    templ.setAudioOption(QWinToastTemplate::AudioOption::Default);
    templ.addAction("Yes");
    templ.addAction("No");


    if (toast->showToast(templ) < 0) {
        QMessageBox::warning(this, "Error", "Could not launch your toast notification!");
    }

    connect(toast, static_cast<void(QWinToast::*)(void)>(&QWinToast::toastActivated), [this]()
        {
            QMessageBox::information(this, "ToastActivated!", "ToastActivated!");
        });

}
