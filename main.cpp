#include "chatwidget.h"
#include "loginwidget.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    LoginWidget lw;
    lw.show();
    return a.exec();
}
