#pragma once

#include <memory>
#include <vector>
#include <fstream>
#include <variant>
#include <filesystem>

namespace tiff
{
	enum class Error : uint32_t
	{
		NoError = 0,

		FormatNotSupport,
		CompressionNotSupport,
		TiledNotSupport,
		OrientationNotSupport,
		PhotometricInterpretationNotSupport,
		MultiSampleSizeNotSupport,

		InvalidImageSize,
		InvalidBitPerSample,
		InvalidTiffByteOrder,
		InvalidTiffMagicNumber,

		NoMoreImagesInTiff,

		StripDataLost,
		OpenFileFailed,
		ReaderIsNotGoodYet,
	};

	enum class ResolutionUnit : uint16_t
	{
		None = 1,
		Inch = 2,
		CentiMeter = 3
	};

	enum class SampleFormat : uint16_t
	{
		Uint = 1,
		Int = 2,
		Float = 3,
		Undefined = 4,
		IEEEFp = Float,
		Void = Undefined,
	};

	template<typename value_t>
	struct Vec2
	{
		value_t x = 0;
		value_t y = 0;
	};
	typedef Vec2<float> Vec2f;
	typedef Vec2<unsigned long> Vec2ul;

	using variant_t = std::variant<uint8_t, uint16_t, uint32_t, uint64_t>;

	namespace util
	{
		template <typename from_t, typename cast_t>
		cast_t cast_as(from_t from)
		{
			cast_t result{};
			static_assert(sizeof from_t == sizeof cast_t);
			std::memcpy(&result, &from, sizeof from_t);
			return result;
		}
	}

	namespace reader
	{
		class ReaderPrivate;
		class Reader
		{
		public:
			Reader(std::filesystem::path tiff_path) noexcept;
			~Reader() noexcept;

		public:
			bool good() const noexcept;
			Error open() noexcept;

			uint32_t width() const noexcept;
			uint32_t height() const noexcept;

			std::string image_description() const noexcept;

			bool has_next_frame() const noexcept;
			Error read_next_frame() const noexcept;
			uint32_t count_frames() const noexcept;

			Vec2f resolution() const noexcept;
			ResolutionUnit resolution_unit() const noexcept;

			uint16_t bits_per_sample() const noexcept;
			uint16_t samples_per_pixel() const noexcept;
			SampleFormat sameple_format() const noexcept;

			std::vector<variant_t> get_sample_data(uint16_t sample, Error& err);

		private:
			std::shared_ptr<ReaderPrivate> _p = nullptr;
		};
	}

	namespace writer
	{
		// TODO
	}
}

