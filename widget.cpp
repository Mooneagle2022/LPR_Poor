#include "widget.h"
#include "ui_widget.h"
#include <QFile>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QTimer>

Widget::Widget(QWidget *parent):
    QWidget(parent), ui(new Ui::Widget)
{
    ui->setupUi(this);
}

Widget::~Widget()
{
    delete ui;
}

void Widget::mouseDoubleClickEvent(QMouseEvent *event)  //鼠标双击事件
{
    if(!img.empty()||onWork==true)
        return;
    if (event->button() == Qt::LeftButton)
    {
        QString Qpath = QFileDialog::getOpenFileName(
            nullptr, "Open File", QDir::homePath(),
            "All Files (*);;Text Files (*.txt)");
        if (!Qpath.isEmpty())
        {
            String path = String((const char *)Qpath.toLocal8Bit());
            Mat mat = imread(path, 1);//一定要使用绝对路径，其他可以回报错
            if (!mat.empty())
            {
                src=mat.clone();
                img=src.clone();
                showImage(img, ui->lb_main);
                ui->lb_output->setText("点击右下方蓝色按钮进行识别");
            }
            else
            {
                QMessageBox::warning(this,"文件错误","不支持该文件类型");
                qWarning() << "Unsupported File type";
            }
        }
    }
}

void Widget::dragEnterEvent(QDragEnterEvent *event)  //将文件拖拽进来的事件
{
    if(event->mimeData()->hasUrls()&&onWork==false)
    {
        event->acceptProposedAction();
    }
    else
        event->ignore();//不接受鼠标事件
}

void Widget::dropEvent(QDropEvent *event)  //拖拽释放事件
{
    if(event->mimeData()->hasUrls()&&onWork==false)
    {
        const QMimeData *qm = event->mimeData();//获取MIMEData
        String path = String((const char *)qm->urls()[0].toLocalFile().toLocal8Bit());
        src = imread(path, 1);//一定要使用绝对路径，其他容易报错
        if (!src.empty())
        {
            img=src.clone();
            showImage(img, ui->lb_main);
            ui->lb_output->setText("点击右下方蓝色按钮进行识别");
        }
        else
        {
            event->ignore();//不接受鼠标事件
            QMessageBox::warning(this,"图片打开错误","无法解析该图片格式");
            qWarning() << "图片打开错误";
        }
    }
    else
    {
        event->ignore();//不接受鼠标事件
        QMessageBox::warning(this,"文件路径错误","找不到文件所在路径");
        qWarning() << "文件路径错误";
    }
}

void Widget::resizeEvent(QResizeEvent *event)  //窗口大小改变事件
{
    if(!img.empty())
        showImage(img,ui->lb_main);
}

void Widget::on_btn1_recognize_clicked()  //点击识别按钮
{
    if(img.empty()||onWork==true)
        return;
    onWork=true;
    ui->lb_output->setText("正在识别中，请耐心等候");
    string str_lic=recognize(src);
    if(str_lic!="")
        ui->lb_output->setText(QString::fromLocal8Bit(str_lic.data()));
    else
        ui->lb_output->setText("识别失败");
    img=src.clone();
    showImage(img,ui->lb_main);
    onWork=false;
}

void Widget::on_btn2_clear_clicked()  //点击清空按钮
{
    if(!img.empty()&&onWork==false)
    {
        ui->lb_main->clear();
        ui->lb_main->setText("拖 动 图 片 到 此 处\n或 双 击 此 处");
        ui->lb_output->clear();
        ui->lb_output->setText("请打开待识别图片");
        src=Mat();
        img=src.clone();
    }
}

string Widget::recognize(Mat input)  //车牌识别主函数
{
    img=lic_locate(input);
    if(img.empty())
    {
        qWarning() << "车牌定位失败";
        return "";
    }
    showImage(img,ui->lb_main);

    img=char_extract(img);
    showImage(img,ui->lb_main);

    vector<Mat> licChar=char_segment(img);
    if(licChar.size()<7)
    {
        qWarning() << "字符分割失败";
        return "";
    }

    string str_lic="";
    Mat mat = licChar[0].clone();
    str_lic+=match_province(mat);
    for (size_t i = 1; i < licChar.size(); i++)
        str_lic+=match_NumLet(licChar[i]);
    return str_lic;
}
