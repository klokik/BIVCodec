#include <iostream>
#include <fstream>

#include "Frame.hh"

// #include "lena_gray.hh"
#include "lena_color.hh"


int main(int argc, const char **argv)
{
  std::vector<uint8_t> source;

  for (int i = 0; i < lena_image.width*lena_image.height*lena_image.bytes_per_pixel; i += lena_image.bytes_per_pixel)
    source.push_back(0.333*(
      lena_image.pixel_data[i]+
      lena_image.pixel_data[i+1]+
      lena_image.pixel_data[i+2]));

  BIVCodec::ImageMatrix src_image(lena_image.width,lena_image.height,BIVCodec::ColorSpace::Grayscale,&source[0]);

  BIVCodec::ImageBSP bsp_image(src_image,BIVCodec::ColorSpace::Grayscale);

  std::cout << "Frames: " << bsp_image.frames << std::endl;

  BIVCodec::ImageBSP bsp_from_chain(BIVCodec::ColorSpace::Grayscale);
  auto frame_chain = std::move(bsp_image.asFrameChain());
  bsp_from_chain.applyFrameChain(frame_chain);

  BIVCodec::ImageMatrix dec_image(std::move(bsp_from_chain.asImageMatrix(512)));

  std::cout << "Width:\t" << dec_image.width << std::endl
            << "Height:\t" << dec_image.height << std::endl
            << "Chain length: " << frame_chain.size() << std::endl
            << "Frames from chain: " << bsp_from_chain.frames << std::endl;

  std::ofstream ofs;
  ofs.open("image.data", std::ios_base::binary|std::ios_base::out);

  for (int i = 0; i < dec_image.width*dec_image.height; ++i)
    ofs << static_cast<char>(dec_image.getFragment(i));

  ofs.close();
}