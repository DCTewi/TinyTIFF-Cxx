# TinyTiff-Cxx

Unofficial modern C++ port implementation of [TinyTIFF](https://github.com/jkriege2/TinyTIFF).

## Why and When

- Need to read tiff images with a super tiny single file library with C++.
- Want to read tiff images in a more modern C++ way and avoid manual `malloc`/`free`.

## Usage

### by source

C++17 is needed.

All you need to do is just put `tiff_cxx.h` and `tiff_cxx.cpp` in your project.

### by library (ok fine)

```shell
git clone https://github.com/DCTewi/TinyTIFF-Cxx.git
cd TinyTIFF-Cxx
cmake -S . -B build -DBUILD_TEST=ON
cmake --build build --config Release
```

## Examples

```cpp
std::string tiff_path{ "path/to/your/tiff.tif" };

tiff::reader::Reader reader{ tiff_path };
if (reader.open() != tiff::Error::NoError)
{
    std::cerr << "tiff open failed\n";
}
if (reader.good())
{
    std::cout << "\nwidth = " << reader.width() << ", height = " << reader.height() << "\n"

    auto err = tiff::Error::NoError;
    auto data = reader.get_sample_data(0, err);
    if (err == tiff::Error::NoError)
    {
        auto& variant = data.at(0);
        if (auto value = std::get_if<uint32_t>(&variant))
        {
            std::cout << "data[0] = " << tiff::util::cast_as<uint32_t, float>(*value) << "\n";
        }
    }
}
```

For more, see `test/tiff_cxx_test.cpp`.

## Todo

`tiff::writer`
