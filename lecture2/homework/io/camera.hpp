#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <opencv2/opencv.hpp>

class Camera
{
    public:
        Camera();
        ~Camera();
        bool read(cv::Mat& img);
    private:
        void* handle_;
};

#endif
