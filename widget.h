#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QLabel>
#include <QDebug>
#include <QMessageBox>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
using namespace cv;
using namespace std;

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

private:
    Ui::Widget *ui;

public:
    string recognize(Mat input);//车牌识别主函数
    Mat lic_locate(Mat input);//车牌定位
    Mat char_extract(Mat input);//字符提取
    vector<Mat> char_segment(Mat input);//字符分割
    string match_province(Mat input);//省份缩写匹配
    string match_NumLet(Mat input);//数字和字母匹配

private slots:
    void on_btn1_recognize_clicked();
    void on_btn2_clear_clicked();

private:
    Mat src,img;
    bool onWork=false;//记录是否在工作中，如果是，则不响应按钮操作
    void showImage(cv::Mat mat, QLabel *label);//图片显示
    Mat adjust(cv::Mat input, const QLabel *label);//图片大小随窗口调整并居中
    QImage MatToQImage(const cv::Mat &mat);
    Mat QImageToMat(const QImage &image);

private:
    virtual void resizeEvent(QResizeEvent *event) override;

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
};
#endif // WIDGET_H
