#include "widget.h"
#include <QApplication>
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QFile file(":/qss/LPR.qss");/*QSS文件所在的路径*/
    file.open(QIODevice::ReadOnly);
    if(file.isOpen())
    {
        QString strFile = file.readAll();
        a.setStyleSheet(strFile);
        file.close();/*记得关闭QSS文件*/
    }

    Widget w;
    w.resize(800,450);
    w.show();
    return a.exec();
}
