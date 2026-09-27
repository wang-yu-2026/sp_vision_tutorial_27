#include"camera.hpp"
#include"hikrobot/include/MvCameraControl.h"
#include<iostream>
#include<unordered_map>

cv::Mat transfer(MV_FRAME_OUT& raw)
{
    MV_CC_PIXEL_CONVERT_PARAM cvt_param;
    cv::Mat img(cv::Size(raw.stFrameInfo.nWidth, raw.stFrameInfo.nHeight), CV_8U, raw.pBufAddr);

    cvt_param.nWidth = raw.stFrameInfo.nWidth;
    cvt_param.nHeight = raw.stFrameInfo.nHeight;

    cvt_param.pSrcData = raw.pBufAddr;
    cvt_param.nSrcDataLen = raw.stFrameInfo.nFrameLen;
    cvt_param.enSrcPixelType = raw.stFrameInfo.enPixelType;

    cvt_param.pDstBuffer = img.data;
    cvt_param.nDstBufferSize = img.total() * img.elemSize();
    cvt_param.enDstPixelType = PixelType_Gvsp_BGR8_Packed;

    auto pixel_type = raw.stFrameInfo.enPixelType;
    const static std::unordered_map<MvGvspPixelType, cv::ColorConversionCodes> type_map = {
      {PixelType_Gvsp_BayerGR8, cv::COLOR_BayerGR2RGB},
      {PixelType_Gvsp_BayerRG8, cv::COLOR_BayerRG2RGB},
      {PixelType_Gvsp_BayerGB8, cv::COLOR_BayerGB2RGB},
      {PixelType_Gvsp_BayerBG8, cv::COLOR_BayerBG2RGB}};
    cv::cvtColor(img, img, type_map.at(pixel_type));
    
    return img;
}

Camera::Camera()
{
    MV_CC_DEVICE_INFO_LIST device_list;
    int ret =MV_CC_EnumDevices(MV_USB_DEVICE,&device_list);
    if(ret!=MV_OK||device_list.nDeviceNum==0){
        std::cerr<<"未找到相机设备"<<std::endl;
        handle_=nullptr;
        return;
    }

    ret = MV_CC_CreateHandle(&handle_,device_list.pDeviceInfo[0]);
    if(ret!=MV_OK){
        std::cerr<<"创建句柄失败"<<std::endl;
        handle_=nullptr;
        return;
    }

    ret = MV_CC_OpenDevice(handle_);
    if(ret!=MV_OK){
        std::cerr<<"打开相机失败"<<std::endl;
        handle_=nullptr;
        return;
    }

    MV_CC_SetEnumValue(handle_, "BalanceWhiteAuto", MV_BALANCEWHITE_AUTO_CONTINUOUS);
    MV_CC_SetEnumValue(handle_, "ExposureAuto", MV_EXPOSURE_AUTO_MODE_OFF);
    MV_CC_SetEnumValue(handle_, "GainAuto", MV_GAIN_MODE_OFF);
    MV_CC_SetFloatValue(handle_, "ExposureTime", 4000);
    MV_CC_SetFloatValue(handle_, "Gain", 20);
    MV_CC_SetFrameRate(handle_, 60);
}

Camera::~Camera(){
    if(handle_==nullptr){
        return;
    }
    MV_CC_StopGrabbing(handle_);
    MV_CC_CloseDevice(handle_);
    MV_CC_DestroyHandle(handle_);
}

bool Camera::read(cv::Mat& img){
    if(handle_==nullptr){
        return false;
    }

    int ret=MV_CC_StartGrabbing(handle_);
    if(ret!=MV_OK){
        std::cerr<<"开始采集失败"<<std::endl;
        return false;
    }

    MV_FRAME_OUT raw;
    unsigned int nMsec=100;
    ret = MV_CC_GetImageBuffer(handle_,&raw,nMsec);
    if(ret!=MV_OK){
        std::cerr<<"获取图片失败"<<std::endl;
        MV_CC_StopGrabbing(handle_);
        return false;
    }

    img=transfer(raw).clone();

    MV_CC_FreeImageBuffer(handle_,&raw);
    MV_CC_StopGrabbing(handle_);
    return true;
}