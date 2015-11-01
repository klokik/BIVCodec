#include <cassert>

#include <chrono>
#include <iostream>
#include <queue>
#include <tuple>

#include <opencv2/opencv.hpp>

#include "Frame.hh"

using namespace cv;


int main(int argc, const char **argv)
{
  VideoCapture cap(1);//"/home/klokik/Movies/sintel_trailer-480p.mp4");
  assert(cap.isOpened());

  const int enc_width = 64;
  const int enc_height = 64;

  BIVCodec::ImageMatrix mat_black(enc_width, enc_height, BIVCodec::ColorSpace::Grayscale);
  BIVCodec::ImageBSP bsp_prev(mat_black, BIVCodec::ColorSpace::Grayscale);
  std::vector<BIVCodec::Frame> prev_chain = std::move(bsp_prev.asFrameChain());

  BIVCodec::ImageBSP bsp_image(BIVCodec::ColorSpace::Grayscale);

  bool first_frame = true;

  while(1)
  {
    Mat cam_source;
    cap >> cam_source;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    cvtColor(cam_source, cam_source, CV_BGR2GRAY);
    resize(cam_source, cam_source, Size(enc_width, enc_height));

    BIVCodec::ImageMatrix mat_source(cam_source.cols, cam_source.rows, BIVCodec::ColorSpace::Grayscale, cam_source.ptr(0));
    mat_source = std::move(BIVCodec::matrixMap(mat_source, [](auto a) { return a/256; }));
    BIVCodec::ImageBSP bsp_source(mat_source, BIVCodec::ColorSpace::Grayscale);

    auto frame_chain = std::move(bsp_source.asFrameChain());

    std::priority_queue<std::tuple<float, BIVCodec::Frame>> priority_chain;

    assert(prev_chain.size() == frame_chain.size());
    for (int i = 1; i < frame_chain.size(); ++i)
    {

      auto a = frame_chain[i].imageData();
      auto b = prev_chain[i].imageData();

      float cost = std::abs(a->value_l-b->value_l)+std::abs(a->value_r-b->value_r);
      cost /= a->location.layer+1;

      priority_chain.push(std::make_tuple(cost, frame_chain[i]));
    }

    frame_chain.clear();
    while (!priority_chain.empty())
    {
      frame_chain.push_back(std::get<1>(priority_chain.top()));
      priority_chain.pop();
    }


    if (!first_frame)
    {
      int new_size = frame_chain.size()*1.0;
      frame_chain.erase(frame_chain.begin()+new_size, frame_chain.end());
    }
    else
      first_frame = false;

    bsp_image.applyFrameChain(frame_chain);
    prev_chain = std::move(bsp_image.asFrameChain());

    BIVCodec::ImageMatrix mat_image = std::move(bsp_image.asImageMatrix(512));

    Mat dec_mat(mat_image.height, mat_image.width, CV_32F, mat_image.data());

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::stringstream sstr;
    sstr << "IO latency "
         << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
         << "ms.";

    putText(dec_mat, sstr.str(), Point(100, 50), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255), 2);
    imshow("BIVCodec", dec_mat);

    if (waitKey(5) == 27)
      break;
  }

  return 0;
}