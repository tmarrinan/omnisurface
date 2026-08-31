#include <cstring>
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_image_resize2.h"
#include "imgloader.h"

uint8_t* img::loadImageFromFile(const char* filename, int* width, int* height, int* channels, int out_channels)
{
	return stbi_load(filename, width, height, channels, out_channels);
}

void img::freeImage(uint8_t* image)
{
	stbi_image_free(image);
}

uint8_t* img::cropImage(uint8_t* image, int width, int height, int channels, int x, int y, int crop_w, int crop_h)
{
	uint8_t* out = static_cast<uint8_t*>(stbi__malloc(crop_w * crop_h * channels));
	for (int i = 0; i < crop_h; i++)
	{
		uint8_t* src_row = image + (((y + i) * width + x) * channels);
		uint8_t* dst_row = out + (i * crop_w * channels);
		memcpy(dst_row, src_row, crop_w * channels);
	}
	return out;
}

uint8_t* img::resizeImage(uint8_t* image, int width, int height, int channels, int new_w, int new_h)
{
	uint8_t* out = static_cast<uint8_t*>(stbi__malloc(new_w * new_h * channels));
	stbir_resize_uint8_linear(image, width, height, 0, out, new_w, new_h, 0, static_cast<stbir_pixel_layout>(channels));
	return out;
}

void img::saveImageJpeg(const char* filename, uint8_t* image, int width, int height, int channels, int quality)
{
	stbi_write_jpg(filename, width, height, channels, image, quality);
}

void img::saveImagePng(const char* filename, uint8_t* image, int width, int height, int channels)
{
	stbi_write_png(filename, width, height, channels, image, width * channels);
}
