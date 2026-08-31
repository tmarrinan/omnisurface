#pragma once
#include <iostream>
#include <cstdint>

namespace img {
	uint8_t* loadImageFromFile(const char* filename, int* width, int* height, int* channels, int out_channels);
	void freeImage(uint8_t* image);
	uint8_t* cropImage(uint8_t* image, int width, int height, int channels, int x, int y, int crop_w, int crop_h);
	uint8_t* resizeImage(uint8_t* image, int width, int height, int channels, int new_w, int new_h);
	void saveImageJpeg(const char* filename, uint8_t* image, int width, int height, int channels, int quality);
	void saveImagePng(const char* filename, uint8_t* image, int width, int height, int channels);
}