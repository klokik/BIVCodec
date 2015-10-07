#include <cassert>

#include <algorithm>
#include <iostream>

#include <opencv2/opencv.hpp>

#include "Frame.hh"

using namespace cv;

int main(int argc, const char **argv)
{
  VideoCapture cap("/home/klokik/Movies/sintel_trailer-480p.mp4");
  assert(cap.isOpened());

  while(1)
  {
    Mat cam_source;
    cap >> cam_source;

    cvtColor(cam_source, cam_source, CV_BGR2GRAY);
    resize(cam_source, cam_source, Size(64, 64));

    BIVCodec::ImageMatrix mat_source(cam_source.cols, cam_source.rows, BIVCodec::ColorSpace::Grayscale, cam_source.ptr(0));
    mat_source = std::move(BIVCodec::matrixMap(mat_source, [](auto a) { return a/256; }));
    BIVCodec::ImageBSP bsp_source(mat_source, BIVCodec::ColorSpace::Grayscale);

    // auto frame_chain = std::move(bsp_source.asFrameChain());
    // BIVCodec::ImageBSP bsp_image(BIVCodec::ColorSpace::Grayscale);
    // int new_size = frame_chain.size()*0.05;
    // frame_chain.erase(frame_chain.begin()+new_size, frame_chain.end());
    // bsp_image.applyFrameChain(frame_chain);
    auto &bsp_image = bsp_source;

    BIVCodec::ImageMatrix mat_image = std::move(bsp_image.asImageMatrix(512));
    // BIVCodec::ImageMatrix mat_image = std::move(mat_source);

    Mat dec_mat(mat_image.height, mat_image.width, CV_32F, mat_image.data());

    imshow("BIVCodec", dec_mat);

    // resize(cam_source, cam_source, Size(512, 512), 0, 0, INTER_NEAREST);
    // imshow("BIVCodec", cam_source);

    if (waitKey(30) == 27)
      break;
  }

  return 0;
}