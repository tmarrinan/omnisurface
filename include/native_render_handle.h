#pragma once
#include <cstdint>

class NativeRenderHandle {
private:
	struct NativeData;

	NativeData* _native_data;
	uint8_t _luid[8];

public:
	NativeRenderHandle();
	~NativeRenderHandle();

	GLFWwindow* createFullscreenWindow(const char* title);
	void swapBuffers();
};
