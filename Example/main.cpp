#include "QWinToastExample.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QWinToastExample widget;
    widget.show();

    return app.exec();
}
