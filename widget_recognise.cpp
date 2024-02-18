#include "widget.h"
#include "ui_widget.h"
#include <vector>

//车牌定位
Mat Widget::lic_locate(Mat input)
{
    qWarning() << "";
    qWarning() << "车牌定位开始————————————————";

    //把图像转换到hsv色彩空间
    cv::Mat hsv;
    cv::cvtColor(input, hsv, cv::COLOR_BGR2HSV);

    // 分割HSV成三个单通道图像 (H, S, V)
    vector<Mat> hsv_channels;
    split(hsv, hsv_channels);
    Mat v_channel = hsv_channels[2];

    // 计算V通道的平均值
    Scalar avg_brightness = mean(v_channel);

    // 计算根据亮度调整蓝色阈值
    Vec3b lower_blue, upper_blue;
    if (avg_brightness[0] < 100) { // 亮度低
        lower_blue = Vec3b(100, 80, 40);
        upper_blue = Vec3b(130, 255, 255);
    } else if (avg_brightness[0] > 150) { // 亮度高
        lower_blue = Vec3b(100, 120, 60);
        upper_blue = Vec3b(130, 255, 255);
    } else { // 亮度中等
        lower_blue = Vec3b(100, 100, 50);
        upper_blue = Vec3b(130, 255, 255);
    }

    // 二值化只包含蓝色和黑色部分
    Mat mask;
    inRange(hsv, lower_blue, upper_blue, mask);

    //消除小块噪声
    dilate(mask, mask, getStructuringElement(MORPH_RECT, Size(3, 3)), Point(-1, -1), 2);
    erode(mask, mask, getStructuringElement(MORPH_RECT, Size(3, 3)), Point(-1, -1), 1);

//    //显示处理结果
//    Mat blue=Mat();
//    cv::bitwise_and(input, input, blue, mask);  //mask和原图与运算，获取蓝色部分
//    showImage(blue, ui->lb_main);

    //收集所有蓝色轮廓
    vector<vector<cv::Point>> contours;
    vector<Vec4i> hierarchy;
    findContours(mask, contours, hierarchy, RETR_LIST, CHAIN_APPROX_SIMPLE);

    // 载入车牌模板图像
    std::vector<String> plateFilenames;
    glob("Plate/*.jpg", plateFilenames);

    //遍历寻找车牌匹配度最高的轮廓区域
    double maxMatchVal = 0;  //车牌匹配度
    Mat maxMatch;  //车牌匹配度最高的区域图像
    float inputSize = input.cols*input.rows;  //输入图像的面积
    double minSize = 0.005*inputSize;  //最小面积

    for (auto& i: contours)
    {
        if (contourArea(i)<minSize)
            continue;  //轮廓面积过小，跳过

        RotatedRect rotated = minAreaRect(i);

        // 获取旋转角度和尺寸
        float angle = rotated.angle;
        Size2f size2f = rotated.size;

        // 修正宽度和高度，使宽度始终大于高度
        if (size2f.width < size2f.height)
        {
            swap(size2f.width, size2f.height);
            angle += 90.0;
        }

        // 使用角度的绝对值来决定是否需要旋转180度
        if (abs(angle) > 45)
            angle += 180.0;

        // 获取旋转矩阵
        Mat M = getRotationMatrix2D(rotated.center, angle, 1.0);

        // 进行仿射变换
        Mat revised;
        warpAffine(input, revised, M, input.size(), INTER_CUBIC);
        // 在进行仿射变换之后，确保revised图像的尺寸是有效的
        if (revised.cols <= 0 || revised.rows <= 0) {
            qWarning() << "Invalid size of the revised image after rotation.";
            continue;
        }

        // 裁剪旋转后的图像
        Rect roi = Rect(rotated.center.x - size2f.width / 2,
                        rotated.center.y - size2f.height / 2,
                        size2f.width,
                        size2f.height);


        // 保证ROI在图像范围内
        roi.x = max(roi.x, 0);
        roi.y = max(roi.y, 0);
        roi.width = min(roi.width, revised.cols - roi.x);
        roi.height = min(roi.height, revised.rows - roi.y);
        revised = revised(roi);
        // revised现在是旋转修正并裁剪后的图像
        qWarning() << "矩形宽高：" << revised.cols << revised.rows;

        float ratio = (float)revised.cols / (float)revised.rows;  //长宽比
        int size = revised.cols * revised.rows;
        if(ratio < 2.4 || ratio > 4.2
            || inputSize / size < 8)
            continue;  //长宽比或大小太离谱，跳过

        qWarning() << "矩形宽高：" << revised.cols << revised.rows;

        for (const auto& filename: plateFilenames)
        {
            Mat plateTemplate = imread(filename);
            // 如果载入的模板为空，则跳过
            if (plateTemplate.empty() || plateTemplate.cols == 0 || plateTemplate.rows == 0) {
                qWarning() << "Template image is empty or has invalid size.";
                continue;
            }

            // 确保revised图像有效
            if (revised.empty() || revised.cols == 0 || revised.rows == 0) {
                qWarning() << "Revised image is empty or has invalid size.";
                continue;
            }

            // 计算缩放比例
            double scaleWidth = (double)plateTemplate.cols / revised.cols;
            double scaleHeight = (double)plateTemplate.rows / revised.rows;
            double scale = min(scaleWidth, scaleHeight);

            // 确保缩放比例是正数
            if (scale <= 0) {
                qWarning() << "Scale is not positive. Skipping this template.";
                continue;
            }

            // 如果候选区域大于模板尺寸，缩小候选区域
            if (scale < 1) {
                cv::resize(revised, revised, Size(), scale, scale, INTER_AREA);
            }

            // 进行模板匹配
            Mat matchResult;
            matchTemplate(revised, plateTemplate, matchResult, TM_CCOEFF_NORMED);

            double minVal, maxVal;
            Point minLoc, maxLoc;
            minMaxLoc(matchResult, &minVal, &maxVal, &minLoc, &maxLoc);

            if (maxVal > maxMatchVal) {
                maxMatchVal = maxVal;
                maxMatch = revised.clone();
            }
        }

        QCoreApplication::processEvents();  //处理其他事件，防止卡死
    }

    if (maxMatch.empty())
    {
        qWarning() << "未找到有效矩形区域";
        return Mat();
    }

    //裁切
    int height = maxMatch.rows, width = maxMatch.cols;
    Mat output = maxMatch(Range(0.1*height, 0.9*height), Range(0, width));
    qWarning() << "最大矩形宽高：" << output.cols << output.rows;

    qWarning() << "车牌定位结束————————————————";
    return output;
}


//字符提取
Mat Widget::char_extract(Mat input)
{
    qWarning() << "";
    qWarning() << "字符提取开始————————————————";

    //形态学处理
    Mat gray=input.clone();
    cvtColor(input, gray, COLOR_BGR2GRAY);
    threshold(gray, gray, 0, 255, THRESH_BINARY | THRESH_OTSU);

    int up=0, down=gray.rows;//记录字符区的上下边界

    //抹除上侧水平边缘
    for (int i = 0; i < gray.rows * 0.5; i++)
    {
        int x = 0;
        for (int j = 0; j < gray.cols - 1; j++)
            if (gray.at<uchar>(i, j) == 255 && gray.at<uchar>(i, j + 1) == 0)
                x++;
        if (x < 7)
            up = i;
    }
    qWarning() << "上边缘：" << up;

    //抹除下侧水平边缘
    for (int i = gray.rows - 1; i > gray.rows - gray.rows * 0.50; i--)
    {
        int x = 0;
        for (int j = 0; j < gray.cols - 1; j++)
            if (gray.at<uchar>(i, j) == 255 && gray.at<uchar>(i, j + 1) == 0)
                x++;
        if (x < 7)
            down = i;
    }
    qWarning() << "下边缘：" << down;

    int left = 0, right = gray.cols - (gray.cols/20+1)+4;  //记录字符区的上下边界
    //抹除竖边缘
    //以左边缘为例，若最左边width/20列内有联通区域宽度不足阈值，抹除
    int width = gray.cols;
    int height = gray.rows;
    int* whitePerCol = new int[width];  //创建一个用于储存每列白色像素个数的数组
    memset(whitePerCol, 0, width * 4);  //初始化数组
    //遍历统计每一列白色像素值
    for (int col = 0; col < width; ++col)
    {
        for (int row = 0; row < height; ++row)
        {
            if (gray.at<uchar>(row, col) == 255)
                whitePerCol[col]++;
            QCoreApplication::processEvents();  //处理其他事件，防止卡死
        }
    }
    //抹除左侧竖直边缘
    int start = 0;  //记录进入字符区的索引
    int end = 0;  //记录离开字符区的索引
    bool inblock = false;  //是否遍历到了字符区内
    for (int i = 0; i < width/20+width/10; i++)
    {
        if (!inblock && whitePerCol[i] != 0)  //进入字符区
        {
            inblock = true;
            start = i;
        }
        else if (whitePerCol[i] == 0 && inblock)  //离开字符区
        {
            end = i;
            inblock = false;
            if (end - start < 10)  //抹除边缘
            {
                left = max(0,end-2);
                for (int k = 0; k < end; k++)
                    for (int l = 0; l < height; l++)
                        gray.at<uchar>(l, k) = 0;
            }
        }
    }
    qWarning() << "左边缘：" << left;

    //抹除右侧竖直边缘
    start = 0;  //记录进入字符区的索引
    end = 0;  //记录进入空白区域的索引
    inblock = false;  //是否遍历到了字符区内
    for (int i = gray.cols - 1; i > gray.cols - (width/20+1); i--)
    {
        if (!inblock && whitePerCol[i] != 0)  //进入字符区
        {
            inblock = true;
            start = i;
        }
        else if (whitePerCol[i] == 0 && inblock)  //离开字符区
        {
            end = i;
            inblock = false;
            if (start - end < 10)  //抹除边缘
            {
                right = min(end+4,width);
                for (int k = gray.cols - 1; k > end; k--)
                    for (int l = 0; l < height; l++)
                        gray.at<uchar>(l, k) = 0;
            }
        }
    }
    delete[] whitePerCol;
    qWarning() << "右边缘：" << right;

    gray=gray(Range(up, down), Range(left, right));  //切割空白边缘
    qWarning() << "切割后的宽高：" << gray.cols << gray.rows;

    //抹除小噪点, 可能会去除省份缩写汉字的点
    Mat labels,stats, centroids;
    int labelsNum = connectedComponentsWithStats(gray, labels, stats, centroids, 8, CV_32S);
    //遍历所有连通区域
    for (int i = 1; i < labelsNum; i++)
        if (stats.at<int>(i, CC_STAT_AREA) < 0.001*gray.cols*gray.rows)  //抹除面积过小的连通区域
            gray.setTo(0, labels == i);

    qWarning() << "字符提取结束————————————————";
    return gray;
}


//字符分割
vector<Mat> Widget::char_segment(Mat input)
{
    qWarning() << "";
    qWarning() << "字符分割开始————————————————";

    int width = input.cols;
    int height = input.rows;

    int* whitePerCol = new int[width];//创建一个用于储存每列白色像素个数的数组
    memset(whitePerCol, 0, width * 4);//初始化数组
    //遍历统计每一列白色像素值
    for (int col = 0; col < width; col++)
    {
        for (int row = 0; row < height; ++row)
        {
            if (input.at<uchar>(row, col) == 255)
                whitePerCol[col]++;
            QCoreApplication::processEvents();  //处理其他事件，防止卡死
        }
    }

    vector<Mat> reOrder;  //分割出来的字符,与车牌字符顺序相反
    int start = 0;  //记录进入字符区的索引
    int end = 0;  //记录离开字符区的索引
    bool inBlock = false;  //是否遍历到了字符区内
    int flag = 0;  //记录经过了几个字符
    int sum = 0;  //用于计算平均字符宽度
    for (int i = input.cols - 1; i > 0; i--)      //从右往左,以应对“川”字等
    {
        if (!inBlock && whitePerCol[i] != 0)  //进入字符区
        {
            inBlock = true;
            start = i;
        }
        else if (whitePerCol[i] == 0 && inBlock)  //离开字符区
        {
            end = i;
            inBlock = false;
            sum+=start-end;
            qWarning() << "字符左界：" << end << "字符右界：" << start;

            Mat roiImg;
            roiImg = input(Range(0, input.rows), Range(end-2, start+1));
            reOrder.push_back(roiImg);
            flag++;
        }
        if (!inBlock && flag == 7 - 1 && start-end<(sum/5)/2)  //如果分割到“·”，则剔除该字符
        {
            qWarning() << start-end;
            reOrder.pop_back();
            flag--;
        }
        if (flag == 7 - 1)  //如果分割到达最后一位（省份缩写），则把剩下的整个区域分为一个字符
        {
            Mat roiImg = input(Range(0,input.rows), Range(0, end - 3));
            reOrder.push_back(roiImg);
            break;
        }
    }
    delete[] whitePerCol;

    //为“1”填充添加一点边框
    for (size_t i = 0; i < reOrder.size(); i++)
    {
        if (reOrder[i].cols < 20)
            copyMakeBorder(reOrder[i], reOrder[i], 0, 0, 4, 4, BORDER_REPLICATE);
    }

    vector<Mat> output;
    size_t n = reOrder.size();
    for (size_t i = 0; i < n; i++)
    {
        output.push_back(reOrder.back());
        reOrder.pop_back();
        imwrite("output/char_segment"+to_string(i)+".jpg", output[i]);
    }

    qWarning() << "字符分割结束————————————————";
    return output;
}


//省份缩写匹配
string Widget::match_province(Mat input)
{
    qWarning() << "";
    qWarning() << "省份缩写匹配开始————————————————";

    QString Qprovince[] = { "京", "津", "冀", "晋", "蒙", "辽", "吉", "黑",
                            "沪", "苏", "浙", "皖", "闽", "赣", "鲁", "豫",
                            "鄂", "湘", "粤", "桂", "琼", "川", "贵", "云",
                            "藏", "陕", "甘", "青", "宁", "新", "渝" };

    std::string province[31];

    for(int i=0;i<31;i++)  //要使用QString转换后的string，否则编码会出错
        province[i] = Qprovince[i].toLocal8Bit().toStdString();

    vector<float> max_value;
    vector<string> lic_char_name;
    for (auto i: province)
    {
        string pattern="Province/"+i+"/*.jpg";
        std::vector<cv::String> fn;
        std::vector<cv::Mat> img_list;
        cv::glob(pattern, fn);

        int count = fn.size(); //number of png files in images folder
        for (int j = 0; j < count;j++)
        {
            /* Mat t_img = imread(fn[j]);
                cvtColor(t_img, t_img, COLOR_BGR2GRAY);
                threshold(t_img, t_img, 100, 255, THRESH_BINARY);*/
            img_list.push_back(imread(fn[j]));//上面是浅拷贝
            QCoreApplication::processEvents();  //处理其他事件，防止卡死
        }

        //模板匹配
        vector<float> result;
        for (auto& j : img_list)
        {
            Size size=j.size();
            Mat r;

            cvtColor(j, j, COLOR_BGR2GRAY);
            threshold(j, j, 100, 255, THRESH_BINARY);
            Mat input_resized;
            cv::resize(input, input_resized, size);//尺寸一致

            matchTemplate(input_resized, j, r, TM_CCOEFF_NORMED);
            // 找到最佳匹配
            double minVal, maxVal;
            Point minLoc, maxLoc;
            minMaxLoc(r, &minVal, &maxVal, &minLoc, &maxLoc);
            //cout << max_val << endl;
            result.push_back(maxVal);
            QCoreApplication::processEvents();  //处理其他事件，防止卡死
        }

        auto max_temp=max_element(result.begin(), result.end());//获取此文件夹内最大匹配值
        // cout << "当前文件夹最大值"<< * max_temp << endl;
        max_value.push_back(*max_temp);
        lic_char_name.push_back(i);//存储文件夹名
    }

    qWarning() << "各个省份缩写字符匹配率：";
    for (size_t i = 0 ; i < max_value.size(); i++)
    {
        qWarning() << QString::fromLocal8Bit(province[i].data()) << max_value[i];
    }
    auto it = std::max_element(std::begin(max_value), std::end(max_value));
    size_t ind = std::distance(std::begin(max_value), it);
//    int ind = distance(max_value.begin(), max_element(max_value.begin(), max_value.end()));
    qWarning() << "最大匹配率" << *max_element(max_value.begin(), max_value.end())
               << "该字符为" << QString::fromLocal8Bit(province[ind].data());
    qWarning() << "省份缩写匹配结束————————————————";
    return province[ind];
}


//数字和字母匹配
string Widget::match_NumLet(Mat input)
{
    qWarning() << "";
    qWarning() << "数字和字母匹配开始————————————————";
    string NumLet[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
                        "A", "B", "C", "D", "E", "F", "G", "H",
                        "J", "K", "L", "M", "N", "P", "Q", "R",
                        "S", "T", "U", "V", "W", "X", "Y", "Z" };

    QString qlic_char[7];
    vector<float> max_value;
    vector<string> NumLet_name;
    for (auto i: NumLet)
    {
        //cv::String pattern = "province\\"+i+"\\*";
        string pattern="NumLet/"+i+"/*.jpg";
        std::vector<cv::String> fn;
        std::vector<cv::Mat> img_list;
        cv::glob(pattern, fn);

        int count = fn.size(); //当前文件夹中图片个数
        for (int j = 0; j < count;j++)
        {
            /* Mat t_img = imread(fn[j]);
                cvtColor(t_img, t_img, COLOR_BGR2GRAY);
                threshold(t_img, t_img, 100, 255, THRESH_BINARY);*/
            img_list.push_back(imread(fn[j]));//上面是浅拷贝，不能正确处理
            QCoreApplication::processEvents();  //处理其他事件，防止卡死
        }

        //模板匹配
        vector<float> result;
        for (auto& j: img_list)
        {

            Size size=j.size();
            Mat r;

            cvtColor(j, j, COLOR_BGR2GRAY);
            threshold(j, j, 100, 255, THRESH_BINARY);
            Mat input_resized;
            cv::resize(input, input_resized, size);//尺寸一致

            matchTemplate(input_resized, j, r, TM_CCOEFF_NORMED);
            // 找到最佳匹配
            double minVal, maxVal;
            Point minLoc, maxLoc;
            minMaxLoc(r, &minVal, &maxVal, &minLoc, &maxLoc);
            //cout << max_val << endl;
            result.push_back(maxVal);
            QCoreApplication::processEvents();  //处理其他事件，防止卡死
        }

        auto max_temp=max_element(result.begin(), result.end());//获取此文件夹内最大匹配值
        // cout << "当前文件夹最大值"<< * max_temp << endl;
        max_value.push_back(*max_temp);
        NumLet_name.push_back(i);//存储文件夹名
    }
    for (size_t i = 0 ; i < max_value.size(); i++)
    {
        qWarning()  << QString::fromLocal8Bit(NumLet[i].data()) << max_value[i];
    }
    auto it = std::max_element(std::begin(max_value), std::end(max_value));
    size_t ind = std::distance(std::begin(max_value), it);
    //int ind = distance(max_value.begin(), max_element(max_value.begin(), max_value.end()));
    qWarning() << "最大匹配率" << *max_element(max_value.begin(), max_value.end())
               << "该字符为" << QString::fromLocal8Bit(NumLet[ind].data());
    qWarning() << "数字和字母匹配结束————————————————";
    return NumLet[ind];
}
