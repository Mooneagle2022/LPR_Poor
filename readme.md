环境配置    
Qt6.2.4+OpenCV4.5.5 
请将LPR_Poor.pro的下面OpenCV路径改为你自己的OpenCV路径  
INCLUDEPATH += D:\OpenCV\OpenCV-MinGW-Build-OpenCV-4.5.5-x64\include    
LIBS += D:\OpenCV\OpenCV-MinGW-Build-OpenCV-4.5.5-x64\x64\mingw\lib\libopencv_*.a   

补充说明    
编译构建后，将#匹配模板资源文件夹中的所以文件放到构建目录   
build-LPR_Poor-Desktop_Qt_6_2_4_MinGW_64_bit-Debug  
它们是车牌定位和字符识别的模板匹配图片，如果缺失会闪退  
车牌模板较少，可以自行添加  