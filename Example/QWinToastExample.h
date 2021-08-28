#ifndef QWINTOASTEXAMPLE
#define QWINTOASTEXAMPLE

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include "QWinToast.h"

class QWinToastExample: public QWidget
{
    Q_OBJECT
public:
    QWinToastExample(QWidget *parent = 0);
    ~QWinToastExample();
private:
    QVBoxLayout *mainLayout;
private:
    void on_btn_clicked();
};

#endif //QWINTOASTEXAMPLE