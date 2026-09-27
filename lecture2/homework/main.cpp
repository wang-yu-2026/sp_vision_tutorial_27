#include "io/camera.hpp"
#include "tasks/yolo.hpp"
#include"tasks/apriltag_detector.hpp"
#include "opencv2/opencv.hpp"
#include "tools/img_tools.hpp"

int main()
{
    // 初始化相机、yolo类
    Camera camera;
    auto_aim::YOLO yolo("../configs/yolo.yaml");
    auto_charge::AprilTagDetector apriltag("../configs/yolo.yaml");
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

            std::string color_str =auto_aim::COLORS[armor.color];
            std::string name_str =auto_aim::ARMOR_NAMES[armor.name];
            std::string label = color_str +" "+name_str;

            cv::Point text_pos=armor.points[0];
            text_pos.y-=10;
            if(text_pos.y<0){
                text_pos.y=10;
            }
            tools::draw_text(img,label,text_pos,{0,255,255},2.0,2);
        }

        auto tags =apriltag.detect(img);
        for(const auto& tag:tags){
            tools::draw_points(img,tag.corners,{0,255,0},2);
            cv::circle(img,tag.center,5,{0,0,255},-1);
            tools::draw_text(img, std::to_string(tag.id), tag.center, {0, 255, 255}, 2.0, 2);;
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