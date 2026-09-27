#include <iostream>
#include <opencv2/opencv.hpp>

// ======================= 作业 =======================
// 1. 读取 ../assets/demo.jpg
// 2. 使用 cvtColor 把图像转为灰度图（颜色空间：BGR2GRAY）
// 3. 使用 imwrite 把灰度图保存为 gray.jpg
// 4. 在灰度图上用 circle 画一个圆，标记你要"瞄准"的位置
// 5. 显示灰度图，按任意键退出
// ====================================================

int main()
{
    // TODO: 在这里完成你的代码
    cv::Mat img = cv::imread("../assets/demo.jpg");

    if(img.empty()){
        std::cout<<"读取图片失败"<<std::endl;
        return -1;
    }
    std::cout<<"读取照片成功"<<std::endl;
    cv::Mat gray;
    cv::cvtColor(img,gray,cv::COLOR_BGR2GRAY);
    std::cout<<"转灰度成功"<<std::endl;
    cv::imwrite("gray.jpg",gray);
    std::cout<<"已保存gray.jpg"<<std::endl;
    cv::circle(gray,cv::Point(300,200),40,cv::Scalar(255),3);
    cv::imshow("homework",gray);
    std::cout <<"按任意键退出"<<std::endl;
    cv::waitKey(0);
    return 0;
}
