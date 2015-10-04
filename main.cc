#include <iostream>

#include "Frame.hh"


int main(int argc, const char **argv)
{
	BIVCodec::ImageMatrix src_image(128,128,BIVCodec::ColorSpace::Grayscale);

	BIVCodec::ImageBSP bsp_image(src_image,BIVCodec::ColorSpace::Grayscale);

	std::cout << "Frames: " << bsp_image.frames << std::endl;
}