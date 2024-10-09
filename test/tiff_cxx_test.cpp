#include "tiff_cxx.h"

#include <string>
#include <iostream>
#include <algorithm>

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

	std::cout << "\nwidth = " << reader.width() << ", "
		<< "height = " << reader.height() << "\n"
		<< "description = " << reader.image_description() << "\n"
		<< "count_frames = " << reader.count_frames() << "\n"
		<< "resolution = (" << reader.resolution().x
		<< ", " << reader.resolution().y << ")\n"
		<< "resolution unit = " << (int)reader.resolution_unit() << "\n"
		<< "bits_per_sample = " << reader.bits_per_sample() << "\n"
		<< "sample_per_pixel = " << reader.samples_per_pixel() << "\n"
		<< "sample_format = " << (int)reader.sameple_format() << "\n";

	std::cout << std::endl;

	auto err = tiff::Error::NoError;
	auto data = reader.get_sample_data(0, err);

	if (err != tiff::Error::NoError)
	{
		std::cerr << "get sample data failed: " << (int)err << std::endl;
		return 0;
	}
	
	std::cout << "first 20 data:\n";
	for (int i = 0; i < std::min(20, (int)data.size()); ++i)
	{
		auto& variant = data.at(i);
		if (auto value = std::get_if<uint8_t>(&variant))
		{
			std::cout << "\t[uint8_t] "
				<< *value
				<< " [int8_t] "
				<< tiff::util::cast_as<uint8_t, int8_t>(*value)
				<< "\n";
		}
		else if (auto value = std::get_if<uint16_t>(&variant))
		{
			std::cout << "\t[uint16_t] "
				<< *value
				<< " [int16_t] "
				<< tiff::util::cast_as<uint16_t, int16_t>(*value)
				<< "\n";
		}
		else if (auto value = std::get_if<uint32_t>(&variant))
		{
			std::cout << "\t[uint32_t] "
				<< *value
				<< " [int32_t] "
				<< tiff::util::cast_as<uint32_t, int32_t>(*value)
				<< " [float] "
				<< tiff::util::cast_as<uint32_t, float>(*value)
				<< "\n";
		}
		else if (auto value = std::get_if<uint64_t>(&variant))
		{
			std::cout << "\t[uint64_t] "
				<< *value
				<< " [int64_t] "
				<< tiff::util::cast_as<uint64_t, int64_t>(*value)
				<< " [double] "
				<< tiff::util::cast_as<uint64_t, double>(*value)
				<< "\n";
		}
	}

	return 0;
}