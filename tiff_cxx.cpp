#include "tiff_cxx.h"

#include <optional>
#include <limits>

#ifdef _WIN32
#define tiff_memcpy_s(dest, dest_size, src, count) memcpy_s((dest), (dest_size), (src), (count))

#else
#define tiff_memcpy_s(dest, dest_size, src, count) memcpy((dest), (src), (count))

#endif

namespace tiff
{
	enum class ByteOrder : uint8_t
	{
		Unknown = 0,
		BigEndian = 1,
		LittleEndian = 2,
	};

	enum class FillOrder : uint8_t
	{
		Default = 1,
		Reverse = 2,
	};

	enum class DataType : uint16_t
	{
		Byte = 1,
		ASCII = 2,
		Short = 3,
		Long = 4,
		Rational = 5
	};

	enum class CompressionType : uint16_t
	{
		None = 1,
		CCITT = 2,
		PackBits = 32773,
	};

	enum class Orientation : uint8_t
	{
		Stantard = 1
	};

	enum class ExtraSamples : uint16_t
	{
		Unspecified = 0,
		AssociatedAlpha = 1,
		UnassociatedAlpha = 2,
	};

	enum class SampleLayout : int32_t
	{
		Interleaved = 0,
		Separate = 1,
		Chunky = Interleaved,
		Planar = Separate,
	};

	enum class PlanarConfiguration : uint16_t
	{
		Chunky = 1,
		Planar = 2,
	};

	enum class PhotometricInterpretation : uint32_t
	{
		WhiteIsZero = 0,
		BlackIsZero = 1,
		RGB = 2,
		Palette = 3,
		Transparency = 4,
		CMYK = 5,
		YCBCR = 6,
		CIELAB = 8,
	};

	// tiff tags: https://www.loc.gov/preservation/digital/formats/content/tiff_tags.shtml
	enum class Tags : uint16_t
	{
		ImageWidth = 256,
		ImageLength = 257,
		BitsPerSample = 258,
		Compression = 259,
		PhotometricInterpretation = 262,
		FillOrder = 266,
		ImageDescription = 270,
		StripOffsets = 273,
		Orientation = 274,
		SamplesPerPixel = 277,
		RowsPerStrip = 278,
		StripByteCounts = 279,
		XResolution = 282,
		YResolution = 283,
		PlanarConfig = 284,
		ResolutionUnit = 296,
		TileWidth = 322,
		TileLength = 323,
		TileOffsets = 324,
		TileByteCounts = 325,
		ExtraSamples = 338,
		SampleFormat = 339,
	};

	namespace util
	{
		static ByteOrder get_byte_order()
		{
			union { long l; char c[4]; } test{};
			test.l = 1;
			if (test.c[3] && !test.c[2] && !test.c[1] && !test.c[0])
			{
				return ByteOrder::BigEndian;
			}
			if (!test.c[3] && !test.c[2] && !test.c[1] && test.c[0])
			{
				return ByteOrder::LittleEndian;
			}
			return ByteOrder::Unknown;
		}

		static std::optional<Vec2ul> do_ranges_overlap(const Vec2ul& r1, const Vec2ul& r2)
		{
			unsigned long total_range = std::max(r1.y, r2.y) - std::min(r1.x, r2.x);
			unsigned long sum_of_ranges = (r1.y - r1.x) + (r2.y - r2.x);
			if (sum_of_ranges > total_range)
			{
				Vec2ul result{ std::max(r1.x, r2.x), std::min(r1.y, r2.y) };
				return result;
			}
			return {};
		}

		template<typename value_t>
		static value_t byte_swap(value_t n) { return n; }

		template<>
		static uint64_t byte_swap<uint64_t>(uint64_t n)
		{
			return ((n & 0x00000000000000FFULL) << 56)
				+ ((n & 0x000000000000FF00ULL) << 40)
				+ ((n & 0x0000000000FF0000ULL) << 24)
				+ ((n & 0x00000000FF000000ULL) << 8)
				+ ((n & 0x000000FF00000000ULL) >> 8)
				+ ((n & 0x0000FF0000000000ULL) >> 24)
				+ ((n & 0x00FF000000000000ULL) >> 40)
				+ ((n & 0xFF00000000000000ULL) >> 56);
		}

		template<>
		static uint32_t byte_swap<uint32_t>(uint32_t n)
		{
			return ((n & 0x000000FF) << 24)
				+ ((n & 0x0000FF00) << 8)
				+ ((n & 0x00FF0000) >> 8)
				+ ((n & 0xFF000000) >> 24);
		}

		template<>
		static uint16_t byte_swap<uint16_t>(uint16_t n)
		{
			return ((n >> 8) | (n << 8));
		}
	}

	namespace reader
	{
		struct ReaderFrame
		{
			uint32_t width = 0;
			uint32_t height = 0;
			CompressionType compression = CompressionType::None;

			uint16_t samples_per_pixel = 1;
			uint32_t bits_per_sample = 0;
			PlanarConfiguration planar_config = PlanarConfiguration::Chunky;
			SampleFormat sample_format = SampleFormat::Uint;

			uint32_t image_length = 0;
			Orientation orientation = Orientation::Stantard;
			FillOrder fill_order = FillOrder::Default;

			ResolutionUnit resolution_unit = ResolutionUnit::None;
			Vec2f resolution{ 1.0f, 1.0f };

			PhotometricInterpretation photometric_interpertation = PhotometricInterpretation::BlackIsZero;
			bool is_tiled = false;

			uint32_t rows_per_strip = 0;
			uint32_t strip_count = 0;
			std::vector<uint32_t> strip_offsets{};
			std::vector<uint32_t> strip_byte_counts{};

			std::string description{};
		};

		struct ReaderFile
		{
			uint32_t first_record_offset = 0;
			uint32_t next_ifd_offset = 0;

			ByteOrder system_byte_order = ByteOrder::Unknown;
			ByteOrder file_byte_order = ByteOrder::Unknown;

			uint64_t size = 0;

			ReaderFrame current_frame{};

			std::ifstream stream{};
		};

		// Image File Directory
		struct IFD
		{
			Tags tag = Tags::ImageWidth;
			DataType type = DataType::Byte;
			uint32_t count = 0;
			uint32_t value = 0;
			uint32_t value2 = 0;

			std::vector<uint32_t> pvalue{};
			std::vector<uint32_t> pvalue2{};
		};

		struct ReaderPrivate
		{
			std::filesystem::path tiff_path{};
			ReaderFile file{};
			std::string last_error{};
			bool good = false;

			template<typename value_t>
			value_t byte_swap_if_need(value_t n) const noexcept
			{
				if (file.system_byte_order != file.file_byte_order)
				{
					return util::byte_swap(n);
				}
				return n;
			}

			template<typename value_t>
			value_t read()
			{
				value_t result{};
				if (file.stream.good())
				{
					file.stream.read((char*)(&result), sizeof(result));
					result = byte_swap_if_need(result);
				}
				return result;
			}

			IFD read_ifd()
			{
				IFD d{};

				d.tag = Tags(read<uint16_t>());
				d.type = DataType(read<uint16_t>());
				d.count = read<uint32_t>();

				std::streampos pos = file.stream.tellg();
				bool pos_changed = false;

				switch (d.type)
				{
				case DataType::Byte:
				case DataType::ASCII:
				{
					if (d.count > 0)
					{
						if (d.count <= 4)
						{
							for (int i = 0; i < 4; ++i)
							{
								uint32_t v = read<uint8_t>();
								if (i < d.count) d.pvalue.emplace_back(v);
							}
						}
						else
						{
							pos_changed = true;
							uint32_t offset = read<uint32_t>();
							if (offset + static_cast<uint64_t>(d.count) * 1 <= file.size)
							{
								for (uint32_t i = 0; i < d.count; ++i)
								{
									d.pvalue.emplace_back(read<uint8_t>());
								}
							}
						}
					}
					break;
				}
				case DataType::Short:
				{
					if (d.count <= 2)
					{
						for (int i = 0; i < 2; ++i)
						{
							uint32_t v = read<uint16_t>();
							if (i < d.count) d.pvalue.emplace_back(v);
						}
					}
					else
					{
						pos_changed = true;
						uint32_t offset = read<uint32_t>();
						if (offset + static_cast<uint64_t>(d.count) * 2 <= file.size)
						{
							file.stream.clear();
							file.stream.seekg(offset, std::ios_base::beg);
							for (uint32_t i = 0; i < d.count; ++i)
							{
								d.pvalue.emplace_back(read<uint16_t>());
							}
						}
					}
					break;
				}
				case DataType::Long:
				{
					if (d.count <= 1)
					{
						d.pvalue.emplace_back(read<uint32_t>());
					}
					else
					{
						pos_changed = true;
						uint32_t offset = read<uint32_t>();
						if (offset + static_cast<uint64_t>(d.count) * 4 <= file.size)
						{
							file.stream.clear();
							file.stream.seekg(offset, std::ios_base::beg);
							for (uint32_t i = 0; i < d.count; ++i)
							{
								d.pvalue.emplace_back(read<uint32_t>());
							}
						}
					}
					break;
				}
				case DataType::Rational:
				{
					pos_changed = true;
					uint32_t offset = read<uint32_t>();
					if (offset + static_cast<uint64_t>(d.count) * 4 <= file.size)
					{
						file.stream.clear();
						file.stream.seekg(offset, std::ios_base::beg);
						for (uint32_t i = 0; i < d.count; ++i)
						{
							d.pvalue.emplace_back(read<uint32_t>());
							d.pvalue2.emplace_back(read<uint32_t>());
						}
					}

					break;
				}
				default:
				{
					d.value = read<uint32_t>();
					break;
				}
				}

				if (!d.pvalue.empty()) d.value = d.pvalue[0];
				if (!d.pvalue2.empty()) d.value2 = d.pvalue2[0];

				if (pos_changed)
				{
					file.stream.clear();
					file.stream.seekg(pos);
					file.stream.seekg(4, std::ios_base::cur);
				}

				return d;
			}

			Error read_next_frame()
			{
				Error err = Error::NoError;
				file.current_frame = ReaderFrame{};

				if (file.next_ifd_offset != 0 && static_cast<uint64_t>(file.next_ifd_offset) + 2 < file.size)
				{
					file.stream.clear();
					file.stream.seekg(file.next_ifd_offset, std::ios_base::beg);
					uint16_t ifd_count = read<uint16_t>();
					for (uint16_t i = 0; i < ifd_count; ++i)
					{
						IFD ifd = read_ifd();
						switch (ifd.tag)
						{
						case Tags::ImageWidth:
						{
							file.current_frame.width = ifd.value;
							break;
						}
						case Tags::ImageLength:
						{
							file.current_frame.image_length = ifd.value;
							break;
						}
						case Tags::BitsPerSample:
						{
							file.current_frame.bits_per_sample = ifd.value;
							if (ifd.count > 0 && !ifd.pvalue.empty())
							{
								file.current_frame.bits_per_sample = ifd.pvalue[0];
								bool ok = true;
								for (size_t j = 1; j < ifd.pvalue.size(); ++j)
								{
									if (ifd.pvalue.at(j) != ifd.pvalue[0])
									{
										ok = false; break;
									}
								}
								if (!ok)
								{
									err = Error::MultiSampleSizeNotSupport;
								}
							}
							break;
						}
						case Tags::Compression:
						{
							file.current_frame.compression = CompressionType(ifd.value);
							break;
						}
						case Tags::StripOffsets:
						{
							if (ifd.count > 0 && !ifd.pvalue.empty())
							{
								file.current_frame.strip_count = ifd.count;
								for (const auto& v : ifd.pvalue)
								{
									file.current_frame.strip_offsets.emplace_back(v);
								}
							}
							break;
						}
						case Tags::SamplesPerPixel:
						{
							file.current_frame.samples_per_pixel = ifd.value;
							break;
						}
						case Tags::RowsPerStrip:
						{
							file.current_frame.rows_per_strip = ifd.value;
							break;
						}
						case Tags::SampleFormat:
						{
							file.current_frame.sample_format = SampleFormat(ifd.value);
							break;
						}
						case Tags::ImageDescription:
						{
							if (ifd.count > 0 && !ifd.pvalue.empty())
							{
								file.current_frame.description = "";
								for (size_t j = 0; j < ifd.pvalue.size(); ++j)
								{
									file.current_frame.description += (char)ifd.pvalue[j];
								}
							}
							break;
						}
						case Tags::StripByteCounts:
						{
							if (ifd.count > 0 && !ifd.pvalue.empty())
							{
								file.current_frame.strip_count = ifd.count;
								for (const auto& v : ifd.pvalue)
								{
									file.current_frame.strip_byte_counts.emplace_back(v);
								}
							}
							break;
						}
						case Tags::PlanarConfig:
						{
							file.current_frame.planar_config = PlanarConfiguration(ifd.value);
							break;
						}
						case Tags::Orientation:
						{
							file.current_frame.orientation = Orientation(ifd.value);
							break;
						}
						case Tags::PhotometricInterpretation:
						{
							file.current_frame.photometric_interpertation = PhotometricInterpretation(ifd.value);
							break;
						}
						case Tags::FillOrder:
						{
							file.current_frame.fill_order = FillOrder(ifd.value);
							break;
						}
						case Tags::TileByteCounts:
						case Tags::TileLength:
						case Tags::TileOffsets:
						case Tags::TileWidth:
						{
							file.current_frame.is_tiled = true;
							break;
						}
						case Tags::XResolution:
						{
							file.current_frame.resolution.x = (float(ifd.value) / float(ifd.value2));
							break;
						}
						case Tags::YResolution:
						{
							file.current_frame.resolution.y = (float(ifd.value) / float(ifd.value2));
							break;
						}
						case Tags::ResolutionUnit:
						{
							file.current_frame.resolution_unit = ResolutionUnit(ifd.value);
							break;
						}
						default:
							break;
						}
					}
					file.current_frame.height = file.current_frame.image_length;
					file.stream.clear();
					typedef std::basic_istream<char, std::char_traits<char>>::off_type off_t;
					file.stream.seekg(static_cast<off_t>(file.next_ifd_offset) + 2
						+ 12 * static_cast<off_t>(ifd_count), std::ios_base::beg);
					file.next_ifd_offset = read<uint32_t>();

				}
				else
				{
					err = Error::NoMoreImagesInTiff;
				}

				good = (err == Error::NoError);
				return err;
			}

			std::vector<variant_t> get_sample_data_internal(uint16_t sample, Error& err)
			{
				std::vector<variant_t> result{};
				if (file.current_frame.compression != CompressionType::None)
				{
					err = Error::CompressionNotSupport;
					return result;
				}
				if (file.current_frame.is_tiled)
				{
					err = Error::TiledNotSupport;
					return result;
				}
				if (file.current_frame.orientation != Orientation::Stantard)
				{
					err = Error::OrientationNotSupport;
					return result;
				}
				if (file.current_frame.photometric_interpertation == PhotometricInterpretation::Palette)
				{
					err = Error::PhotometricInterpretationNotSupport;
					return result;
				}
				if (file.current_frame.width == 0 || file.current_frame.height == 0)
				{
					err = Error::InvalidImageSize;
					return result;
				}
				{
					const uint32_t& bps = file.current_frame.bits_per_sample;
					if (bps != 8 && bps != 16 && bps != 32 && bps != 64)
					{
						err = Error::InvalidBitPerSample;
						return result;
					}
				}
				const uint32_t sample_image_size_bytes = file.current_frame.width * file.current_frame.height
					* file.current_frame.bits_per_sample / 8;

				std::vector<uint8_t> buffer{};
				buffer.resize(sample_image_size_bytes);

				std::streampos pos = file.stream.tellg();
				if (file.current_frame.strip_count > 0 && !file.current_frame.strip_byte_counts.empty() && !file.current_frame.strip_offsets.empty())
				{
					if (file.current_frame.samples_per_pixel == 1 || file.current_frame.planar_config == PlanarConfiguration::Planar)
					{
						const uint32_t sample_start_bytes = sample * sample_image_size_bytes;
						const uint32_t sample_end_bytes = sample_start_bytes + sample_image_size_bytes;


						uint32_t strip = 0;
						uint32_t file_imageidx_bytes = 0;
						uint32_t output_imageidx_bytes = 0;
						for (strip = 0; strip < file.current_frame.strip_count; ++strip)
						{
							const uint32_t strip_size_bytes = file.current_frame.strip_byte_counts.at(strip);
							const uint32_t strip_offset_bytes = file.current_frame.strip_offsets.at(strip);
							auto bytes_to_read_result = util::do_ranges_overlap(
								Vec2ul{ sample_start_bytes, sample_end_bytes },
								Vec2ul{ file_imageidx_bytes, file_imageidx_bytes + strip_size_bytes }
							);
							if (bytes_to_read_result.has_value())
							{
								const auto& bytes_to_read_range = bytes_to_read_result.value();
								const uint32_t count_bytes_to_read = bytes_to_read_range.y - bytes_to_read_range.x;

								const auto& seek_offset = strip_offset_bytes + (bytes_to_read_range.x - file_imageidx_bytes);

								file.stream.clear();
								file.stream.seekg(seek_offset, std::ios_base::beg);

								file.stream.read((char*)(buffer.data()) + output_imageidx_bytes, count_bytes_to_read);
								auto read_bytes = file.stream.gcount();
								if (read_bytes != count_bytes_to_read)
								{
									err = Error::StripDataLost;
								}
								output_imageidx_bytes += count_bytes_to_read;
							}
							else if (file_imageidx_bytes > sample_end_bytes)
							{
								break; // have read all data
							}

							file_imageidx_bytes += strip_size_bytes;
						}
					}
				}
				else if (file.current_frame.samples_per_pixel > 1 && file.current_frame.planar_config == PlanarConfiguration::Chunky)
				{
					uint32_t strip = 0;
					uint32_t file_imageidx_bytes = 0;
					uint32_t output_imageidx_bytes = 0;
					std::vector<uint8_t> stripdata{};
					uint32_t last_stripsize_bytes = 0;
					for (strip = 0; strip < file.current_frame.strip_count; ++strip)
					{
						const uint32_t stripsize_bytes = file.current_frame.strip_byte_counts[strip];
						const uint32_t strip_offset_bytes = file.current_frame.strip_offsets[strip];
						if (stripsize_bytes > last_stripsize_bytes)
						{
							stripdata.clear();
							stripdata.resize(stripsize_bytes);
							last_stripsize_bytes = stripsize_bytes;
						}

						file.stream.clear();
						file.stream.seekg(strip_offset_bytes, std::ios::beg);
						file.stream.read((char*)stripdata.data(), stripsize_bytes);
						auto readbytes = file.stream.gcount();
						if (readbytes != stripsize_bytes)
						{
							err = Error::StripDataLost;
						}

						uint32_t strip_i = 0;
						for (strip_i = sample * file.current_frame.bits_per_sample / 8;
							strip_i < stripsize_bytes;
							strip_i += file.current_frame.bits_per_sample / 8 * file.current_frame.samples_per_pixel)
						{
							for (int i = 0; i < file.current_frame.bits_per_sample / 8; ++i)
							{
								tiff_memcpy_s(&buffer[output_imageidx_bytes], sample_image_size_bytes - output_imageidx_bytes,
									&stripdata[strip_i], file.current_frame.bits_per_sample / 8);

								output_imageidx_bytes += file.current_frame.bits_per_sample / 8;
							}

							file_imageidx_bytes += stripsize_bytes;
						}
					}

					if (file.system_byte_order != file.file_byte_order)
					{
						const auto& bps = file.current_frame.bits_per_sample;
						if (bps == 8)
						{
							// 1-byte data
						}
						else if (bps == 16)
						{
							for (uint32_t x = 0; x < file.current_frame.width * file.current_frame.height; ++x)
							{
								((uint16_t*)buffer.data())[x] = util::byte_swap(((uint16_t*)buffer.data())[x]);
							}
						}
						else if (bps == 32)
						{
							for (uint32_t x = 0; x < file.current_frame.width * file.current_frame.height; ++x)
							{
								((uint32_t*)buffer.data())[x] = util::byte_swap(((uint32_t*)buffer.data())[x]);
							}
						}
						else if (bps == 64)
						{
							for (uint32_t x = 0; x < file.current_frame.width * file.current_frame.height; ++x)
							{
								((uint64_t*)buffer.data())[x] = util::byte_swap(((uint64_t*)buffer.data())[x]);
							}
						}
					}
				}

				for (uint32_t i = 0; i < file.current_frame.width * file.current_frame.height; ++i)
				{
					variant_t t{};

					const auto& bps = file.current_frame.bits_per_sample;
					if (bps == 8)
					{
						t = ((uint8_t*)buffer.data())[i];
					}
					else if (bps == 16)
					{
						t = ((uint16_t*)buffer.data())[i];
					}
					else if (bps == 32)
					{
						t = ((uint32_t*)buffer.data())[i];
					}
					else if (bps == 64)
					{
						t = ((uint64_t*)buffer.data())[i];
					}

					result.emplace_back(t);
				}

				file.stream.clear();
				file.stream.seekg(pos);

				err = Error::NoError;
				return result;
			}

			Error open()
			{
				file.system_byte_order = util::get_byte_order();

				file.stream.open(tiff_path, std::ios_base::binary);
				if (!file.stream.good())
				{
					return Error::OpenFileFailed;
				}

				file.stream.ignore(std::numeric_limits<std::streamsize>::max());
				file.size = file.stream.gcount();
				file.stream.clear(); // reset eof bit
				file.stream.seekg(0, std::ios_base::beg);

				std::vector<uint8_t> tiffid{ 0, 0, 0 };
				file.stream.read((char*)tiffid.data(), 2);
				if (tiffid[0] == 'I' && tiffid[1] == 'I')
				{
					file.file_byte_order = ByteOrder::LittleEndian;
				}
				else if (tiffid[0] == 'M' && tiffid[1] == 'M')
				{
					file.file_byte_order = ByteOrder::BigEndian;
				}
				else
				{
					return Error::InvalidTiffByteOrder;
				}

				uint16_t magic = read<uint16_t>();
				if (magic != 42)
				{
					return Error::InvalidTiffMagicNumber;
				}

				file.first_record_offset = read<uint32_t>();
				file.next_ifd_offset = file.first_record_offset;

				return read_next_frame();
			}
		};
	}
}

tiff::reader::Reader::Reader(std::filesystem::path tiff_path) noexcept
{
	_p = std::make_shared<ReaderPrivate>();
	_p->tiff_path = tiff_path;
}

tiff::reader::Reader::~Reader() noexcept
{
}

tiff::Error tiff::reader::Reader::open() noexcept
{
	return _p->open();
}

bool tiff::reader::Reader::good() const noexcept
{
	return _p->good;
}

uint32_t tiff::reader::Reader::width() const noexcept
{
	return _p->file.current_frame.width;
}

uint32_t tiff::reader::Reader::height() const noexcept
{
	return _p->file.current_frame.height;
}

std::string tiff::reader::Reader::image_description() const noexcept
{
	return _p->file.current_frame.description;
}

uint32_t tiff::reader::Reader::count_frames() const noexcept
{
	if (_p->good)
	{
		uint32_t frames = 0;
		std::streampos pos = _p->file.stream.tellg();

		uint32_t next_offset = _p->file.first_record_offset;
		while (next_offset > 0)
		{
			_p->file.stream.clear();
			_p->file.stream.seekg(next_offset, std::ios_base::beg);
			uint16_t count = _p->read<uint16_t>() * 12;
			_p->file.stream.seekg(count, std::ios_base::cur);
			next_offset = _p->read<uint32_t>();
			frames += 1;
		}

		_p->file.stream.clear();
		_p->file.stream.seekg(pos);
		return frames;
	}
	return 0;
}

bool tiff::reader::Reader::has_next_frame() const noexcept
{
	if (_p->good
		&& _p->file.next_ifd_offset > 0
		&& _p->file.next_ifd_offset < _p->file.size)
	{
		return true;
	}
	return false;
}

tiff::Error tiff::reader::Reader::read_next_frame() const noexcept
{
	if (!has_next_frame())
	{
		return Error::NoMoreImagesInTiff;
	}
	return _p->read_next_frame();
}

tiff::Vec2f tiff::reader::Reader::resolution() const noexcept
{
	return _p->file.current_frame.resolution;
}

tiff::ResolutionUnit tiff::reader::Reader::resolution_unit() const noexcept
{
	return _p->file.current_frame.resolution_unit;
}

tiff::SampleFormat tiff::reader::Reader::sameple_format() const noexcept
{
	return _p->file.current_frame.sample_format;
}

uint16_t tiff::reader::Reader::bits_per_sample() const noexcept
{
	return _p->file.current_frame.bits_per_sample;
}

uint16_t tiff::reader::Reader::samples_per_pixel() const noexcept
{
	return _p->file.current_frame.samples_per_pixel;
}

std::vector<tiff::variant_t> tiff::reader::Reader::get_sample_data(uint16_t sample, tiff::Error& err)
{
	if (_p->good)
	{
		return _p->get_sample_data_internal(sample, err);
	}
	err = Error::ReaderIsNotGoodYet;
	return {};
}
