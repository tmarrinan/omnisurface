#pragma once
#include <iostream>
#include <cstdint>
#include <string>
#include <filesystem>
#include <thread>
#include "libwebsockets.h"

namespace uis {
	class UiServer {
	private:
		std::thread _net_thread;
		struct lws_context* _context;
		std::filesystem::path _http_dir;
		bool _running;

	public:
		UiServer(std::filesystem::path http_dir, uint16_t port);
		~UiServer();

		std::filesystem::path getHttpDirectory();
		void listen();
		void listenAsync();
		void shutdown();
	};
}
