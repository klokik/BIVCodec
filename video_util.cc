#include <cassert>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

#include "Frame.hh"

using namespace cv;


void encode(const std::vector<std::string> &args)
{
  VideoCapture cap("/home/klokik/Movies/sintel_trailer-480p.mp4");
  assert(cap.isOpened());

  std::ofstream ofs;
  ofs.open("video.bfps", std::ios_base::out|std::ios_base::binary);

  bool first_frame = true;

  while (1)
  {
    Mat cap_mat;
    cap >> cap_mat;

    if(cap_mat.empty())
      break;

    cvtColor(cap_mat, cap_mat, CV_BGR2GRAY);
    resize(cap_mat, cap_mat, Size(0, 0), 0.1, 0.1);

    if (first_frame)
    {
      std::cout << "Source size: (" << cap_mat.cols << ";" << cap_mat.rows << ")" <<std::endl;
      first_frame = false;
    }

    BIVCodec::ImageMatrix mat_source(cap_mat.cols, cap_mat.rows, BIVCodec::ColorSpace::Grayscale, cap_mat.ptr(0));
    // mat_source = std::move(BIVCodec::matrixMap(mat_source, [](auto a) { return a/256; }));
    BIVCodec::ImageBSP bsp_source(mat_source, BIVCodec::ColorSpace::Grayscale);

    auto frame_chain = std::move(bsp_source.asFrameChain());

    for (auto frame : frame_chain)
    {
      auto data = frame.serialize();
      assert(data.size() == 8);

      ofs.write(reinterpret_cast<char*>(&data[0]), data.size());
    }
    std::cout << "|" << std::flush;
  }
  std::cout << std::endl;

  ofs.close();
}

void playback(const std::vector<std::string> &args)
{
  std::ifstream ifs;
  ifs.open("video.bfps", std::ios_base::in|std::ios_base::binary);

  BIVCodec::ImageBSP bsp_image(BIVCodec::ColorSpace::Grayscale);

  char data[8];

  while (!ifs.eof())
  {
    BIVCodec::Frame frame;

    ifs.read(&data[0], 8);
    frame.deserialize(reinterpret_cast<uint8_t*>(&data[0]));

    if (frame.header.type == BIVCodec::FrameHeader::HeaderType::Sync)
    {
      auto sync = std::static_pointer_cast<BIVCodec::FrameSyncData>(frame.data);
      bsp_image.applyFrameData(*sync);

      BIVCodec::ImageMatrix mat_image = std::move(bsp_image.asImageMatrix(std::min(sync->width*4, 512)));
      mat_image = std::move(BIVCodec::matrixMap(mat_image, [](auto a) { return a/256; }));

      Mat dec_mat(mat_image.height, mat_image.width, CV_32F, mat_image.data());
      imshow("BIVCodec", dec_mat);

      std::cout << "|" << std::flush;
      if (waitKey(5) == 27)
        break;
    }
    else
      bsp_image.applyFrameData(*std::static_pointer_cast<BIVCodec::FrameImageData>(frame.data));
  }
  std::cout << std::endl;

  ifs.close();
}

int main(int argc, const char **argv)
{
  if (argc == 1)
  {
    std::cout << "encode or playback?" << std::endl;
    return 0;
  }

  std::vector<std::string> args;
  for(int i = 1; i < argc; ++i)
    args.push_back(std::string(argv[i]));

  if (args[0] == "encode")
    encode(args);
  else if(args[0] == "playback")
    playback(args);

  return 0;
}