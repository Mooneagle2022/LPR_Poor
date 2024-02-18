#include "widget.h"

void Widget::showImage(cv::Mat mat, QLabel *label)
{
    mat = adjust(mat, label);
    QImage image = MatToQImage(mat);
    label->setPixmap(QPixmap::fromImage(image));
}

Mat Widget::adjust(cv::Mat input, const QLabel *label)
{
    cv::Mat mat;
    double w0=input.cols, h0=input.rows,
        w1=label->width(), h1=label->height();
    double ratio0=w0/h0;
    double ratio1=w1/h1;
    if(ratio0>ratio1)
    {
        cv::resize(input, mat,
                   Size(w1,static_cast<int>(w1*(h0/w0))),0,0,INTER_AREA);
    }
    else
    {
        cv::resize(input, mat,
                   Size(static_cast<int>(h1*(w0/h0)),h1),0,0,INTER_AREA);
    }
    return mat;
}

QImage Widget::MatToQImage(const cv::Mat& mat)
{
    switch (mat.type())
    {
    case CV_8UC1:
    {
        QImage image(mat.cols, mat.rows, QImage::Format_Indexed8);
        image.setColorCount(256);
        for(int i = 0; i < 256; i++)
        {
            image.setColor(i, qRgb(i, i, i));
        }
        uchar *pSrc = mat.data;
        for(int row = 0; row < mat.rows; row ++)
        {
            uchar *pDest = image.scanLine(row);
            memcpy(pDest, pSrc, mat.cols);
            pSrc += mat.step;
        }
        return image;
        break;
    }
    case CV_8UC3:
    {
        const uchar *pSrc = (const uchar*)mat.data;
        QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
        return image.rgbSwapped();
        break;
    }
    case CV_8UC4:
    {
        const uchar *pSrc = (const uchar*)mat.data;
        QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
        return image.copy();
        break;
    }
    default:
        QMessageBox::warning(this,"MatToQImage错误","不支持该mat类型");
        qWarning() << "MatToQImage: Unsupported matrix type";
        return QImage();
    }
    return QImage();
}

cv::Mat Widget::QImageToMat(const QImage &image)
{
    cv::Mat mat;
    switch(image.format())
    {
    case QImage::Format_Grayscale8: //灰度图，每个像素点1个字节（8位）
    case QImage::Format_Indexed8: //Mat构造：行数，列数，存储结构，数据，step每行多少字节
        mat = cv::Mat(image.height(), image.width(),
                      CV_8UC1, (void*)image.constBits(), image.bytesPerLine());
        break;
    case QImage::Format_ARGB32:
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32_Premultiplied:
        mat = cv::Mat(image.height(), image.width(),
                      CV_8UC4, (void*)image.constBits(), image.bytesPerLine());
        break;
    case QImage::Format_RGB888: //RR,GG,BB字节顺序存储
        mat = cv::Mat(image.height(), image.width(),
                      CV_8UC3, (void*)image.constBits(), image.bytesPerLine());
        cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR); //opencv需要转为BGR的字节顺序
        break;
    default:
        QMessageBox::warning(this,"QImageToMat错误","不支持该QImage类型");
        qWarning() << "QImageToMat: Unsupported QImage type";
        return Mat();
    }
    return mat;
}
