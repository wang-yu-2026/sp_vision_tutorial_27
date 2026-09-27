#include "io/camera.hpp"
#include "tasks/yolo.hpp"
#include "opencv2/opencv.hpp"
#include "tools/img_tools.hpp"

int main()
{
    // 初始化相机、yolo类
    Camera camera;
    auto_aim::YOLO yolo("../configs/yolo.yaml");
    // while (1) {
        // 调用相机读取图像
    while(1){
        cv::Mat img;
        if(!camera.read(img)){
            std::cerr<<"读取相机失败"<<std::endl;
            break;
        }
        // 调用yolo识别装甲板
        auto armors = yolo.detect(img);
        for(const auto& armor:armors){
            tools::draw_points(img,armor.points,{0,255,0},2);
        }
        // 显示图像
        cv::resize(img,img,cv::Size(640,480));
        cv::imshow("img",img);
        if(cv::waitKey(1)=='q'){
            break;
        }
    }

    return 0;
}