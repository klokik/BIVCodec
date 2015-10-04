#include <iostream>

#include "Frame.hh"


int main(int argc, const char **argv)
{
  BIVCodec::ImageMatrix src_image(128,128,BIVCodec::ColorSpace::Grayscale);

  BIVCodec::ImageBSP bsp_image(src_image,BIVCodec::ColorSpace::Grayscale);

  std::cout << "Frames: " << bsp_image.frames << std::endl;

  BIVCodec::ImageMatrix dec_image(std::move(bsp_image.asImageMatrix(128)));

  std::cout << "Width:\t" << dec_image.width << std::endl
            << "Height:\t" << dec_image.height << std::endl;
}