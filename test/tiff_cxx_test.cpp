#include "tiff_cxx.h"

#include <string>
#include <iostream>

int main()
{
	std::string tiff_path{};
	std::cout << "tiff_path:" << std::endl;
	std::getline(std::cin, tiff_path);

	if (tiff_path.size()
		&& tiff_path.at(0) == '"'
		&& tiff_path.at(tiff_path.size() - 1) == '"')
	{
		tiff_path = tiff_path.substr(1, tiff_path.size() - 2);
	}

	std::cout << "open " << tiff_path << std::endl;

	tiff::reader::Reader reader{ tiff_path };
	if (reader.open() != tiff::Error::NoError)
	{
		std::cerr << "tiff open failed\n";

		return 0;
	}

	if (!reader.good())
	{
		std::cerr << "tiff reader is not good\n";

		return 0;
	}

	std::cout << "width=" << reader.width() << ", "
		<< "height=" << reader.height() << "\n"
		<< "description=" << reader.image_description() << "\n"
		<< "count_frames=" << reader.count_frames() << "\n"
		<< "resolution=(" << reader.resolution().x
		<< ", " << reader.resolution().y << ")\n"
		<< "bits/sample=" << reader.bits_per_sample() << "\n"
		<< "sample/pixel=" << reader.samples_per_pixel() << "\n";

	std::cout << std::endl;

	return 0;
}