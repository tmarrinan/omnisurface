#pragma once
#include <iostream>
#include <cstdint>
#include <filesystem>
#include <thread>
#include "libwebsockets.h"

namespace uis {
	class UiServer {
	private:
		std::thread _net_thread;
		struct lws_context* _context;
		std::filesystem::path _http_dir;
		std::filesystem::path _img_select_root_dir;
		std::filesystem::path _img_select_current_dir;
		bool _running;

	public:
		UiServer(std::filesystem::path http_dir, uint16_t port);
		~UiServer();

		std::filesystem::path getHttpDirectory();
		void setImageSelectionRootDirectory(std::filesystem::path img_select_dir);
		std::filesystem::path getImageSelectionRootDirectory();
		std::filesystem::path getImageSelectionCurrentDirectory();
		void listen();
		void listenAsync();
		void shutdown();
	};
}
