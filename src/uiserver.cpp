#include <algorithm>
#include <string>
#include <vector>

#include "imgloader.h"
#include "uiserver.h"


struct PerSessionHttpData {
	char path[256];
};

static bool isSafeReqPath(std::filesystem::path root_dir, std::filesystem::path req_path)
{
	std::filesystem::path full_path = std::filesystem::weakly_canonical(req_path);
	std::filesystem::path canonical_base = std::filesystem::canonical(root_dir);

	std::pair<std::filesystem::path::const_iterator, std::filesystem::path::const_iterator> mismatch_result =
		std::mismatch(canonical_base.begin(), canonical_base.end(), full_path.begin(), full_path.end());

	return (mismatch_result.first == canonical_base.end());
}

static bool compareCharIgnoreCase(unsigned char ch1, unsigned char ch2)
{
	return std::tolower(ch1) < std::tolower(ch2);
}

static bool compareStringIgnoreCase(const std::string& a, const std::string& b)
{
	return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), compareCharIgnoreCase);
}

static int callbackHttp(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len)
{
	struct lws_context* context = lws_get_context(wsi);
	uis::UiServer* server = static_cast<uis::UiServer*>(lws_context_user(context));
	PerSessionHttpData* session_data = static_cast<PerSessionHttpData*>(user);

    switch (reason) {
	case LWS_CALLBACK_HTTP:
	{
		std::filesystem::path web_root = server->getHttpDirectory();
		std::string uri_path = std::string(static_cast<const char*>(in));
		strncpy(session_data->path, uri_path.c_str(), 255);
		session_data->path[255] = '\0';
		std::filesystem::path req_path;
		if (uri_path.length() >= 12 && uri_path.substr(0, 11) == "/thumbnail/")
		{
			std::string thumbnail_path = uri_path.substr(11);
			std::filesystem::path cache_path = web_root / "cache" / thumbnail_path;
			if (!std::filesystem::is_regular_file(cache_path))
			{
				int w, h, ch;
				std::filesystem::path image_path = server->getImageSelectionRootDirectory() / thumbnail_path;
				if (std::filesystem::is_regular_file(image_path))
				{
					uint8_t* image = img::loadImageFromFile(image_path.string().c_str(), &w, &h, &ch, 4);

					bool is_stereo = (w == h);
					int size = w / 4;
					int cx = 0.375 * w;
					int cy = (is_stereo) ? 0.125 * h : 0.25 * h;
					uint8_t* crop = img::cropImage(image, w, h, 4, cx, cy, size, size);
					img::freeImage(image);

					uint8_t* resize = img::resizeImage(crop, size, size, 4, 256, 256);
					img::freeImage(crop);

					std::filesystem::create_directories(cache_path.parent_path());

					if (image_path.extension() == ".png")
					{
						img::saveImagePng(cache_path.string().c_str(), resize, 256, 256, 4);
					}
					else if (image_path.extension() == ".jpg" || image_path.extension() == ".jpeg")
					{
						img::saveImageJpeg(cache_path.string().c_str(), resize, 256, 256, 4, 90);
					}
					img::freeImage(resize);
				}
				else
				{
					req_path = web_root / "error.html";
					const char* headers = "HTTP/1.1 404 Not Found\r\n";
					int headers_len = static_cast<int>(strlen(headers));
					lws_serve_http_file(wsi, req_path.string().c_str(), "text/html", headers, headers_len);
				}
			}

			printf("UiServer> Thumbnail Request; file='%s'\n", cache_path.string().c_str());
			const char* mime_type = lws_get_mimetype(cache_path.string().c_str(), NULL);
			if (!mime_type) mime_type = "application/octet-stream";
			lws_serve_http_file(wsi, cache_path.string().c_str(), mime_type, NULL, 0);
		}
		else if (uri_path == "/json/current-media")
		{
			lws_callback_on_writable(wsi);
		}
		else
		{
			if (uri_path.empty() || uri_path == "/")
			{
				req_path = web_root / "index.html";
			}
			else if (uri_path[0] == '/')
			{
				req_path = web_root / uri_path.substr(1);
			}
			req_path.make_preferred();
			printf("UiServer> HTTP Request; file='%s'\n", req_path.string().c_str());
			if (isSafeReqPath(web_root, req_path) && std::filesystem::is_regular_file(req_path))
			{
				const char* mime_type = lws_get_mimetype(req_path.string().c_str(), NULL);
				if (!mime_type) mime_type = "application/octet-stream";
				lws_serve_http_file(wsi, req_path.string().c_str(), mime_type, NULL, 0);
			}
			else
			{
				req_path = web_root / "error.html";
				const char* headers = "HTTP/1.1 404 Not Found\r\n";
				int headers_len = static_cast<int>(strlen(headers));
				lws_serve_http_file(wsi, req_path.string().c_str(), "text/html", headers, headers_len);
			}
		}
		break;
	}
	case LWS_CALLBACK_HTTP_WRITEABLE:
	{
		std::filesystem::path web_root = server->getHttpDirectory();
		std::string uri_path = std::string(session_data->path);
		
		if (uri_path == "/json/current-media")
		{
			std::filesystem::path media_dir = server->getImageSelectionCurrentDirectory();
			std::vector<std::string> files;
			std::vector<std::string> directories;
			for (std::filesystem::directory_iterator it(media_dir); it != std::filesystem::directory_iterator(); it++)
			{
				const std::filesystem::directory_entry& entry = *it;
				std::string name = entry.path().filename().string();

				if (entry.is_regular_file())
				{
					files.push_back(name);
				}
				else if (entry.is_directory())
				{
					directories.push_back(name);
				}
			}

			std::sort(files.begin(), files.end(), compareStringIgnoreCase);
			std::sort(directories.begin(), directories.end(), compareStringIgnoreCase);

			std::string response_body = "{\"folders\":[";
			for (int i = 0; i < directories.size(); i++)
			{
				response_body += "\"" + directories[i] + "\"";
				if (i < directories.size() - 1) response_body += ",";
			}
			response_body += "],\"files\":[";
			for (int i = 0; i < files.size(); i++)
			{
				response_body += "\"" + files[i] + "\"";
				if (i < files.size() - 1) response_body += ",";
			}
			response_body += "]}";

			uint8_t header_buf[LWS_PRE + 512];
			uint8_t* p = &header_buf[LWS_PRE];
			uint8_t* end = p + sizeof(header_buf) - LWS_PRE;

			if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK, "application/json", response_body.size(), &p, end))
			{
				return 1;
			}
			if (lws_finalize_write_http_header(wsi, &header_buf[LWS_PRE], &p, end))
			{
				return 1;
			}
			
			std::vector<uint8_t> send_buf(LWS_PRE + response_body.size());
			std::memcpy(&send_buf[LWS_PRE], response_body.data(), response_body.size());
			
			int bytes_written = lws_write(wsi, &send_buf[LWS_PRE], response_body.size(), LWS_WRITE_HTTP_FINAL);
			if (bytes_written < 0)
			{
				return -1;
			}
			if (lws_http_transaction_completed(wsi))
			{
				return -1;
			}

			return 1;
		}
	}
    default:
        break;
    }
    return 0;
}

uis::UiServer::UiServer(std::filesystem::path http_dir, uint16_t port) :
	_context {nullptr}, _running {false}, _http_dir {http_dir}
{
	const struct lws_protocols protocols[] = {
	    { "http", callbackHttp, sizeof(PerSessionHttpData), 0},
	    { NULL, NULL, 0, 0 } /* terminator */
	};

	struct lws_context_creation_info info;
	memset(&info, 0, sizeof(info));
	info.port = port;
	info.protocols = protocols;
	info.user = this;

	_context = lws_create_context(&info);
	if (!_context)
	{
		fprintf(stderr, "libwebsockets init failed\n");
		return;
	}

	printf("UiServer> addr %p\n", this);
}

uis::UiServer::~UiServer()
{
	shutdown();
}

std::filesystem::path uis::UiServer::getHttpDirectory()
{
	return _http_dir;
}

void uis::UiServer::setImageSelectionRootDirectory(std::filesystem::path img_select_dir)
{
	_img_select_root_dir = img_select_dir;
	_img_select_current_dir = img_select_dir;
}

std::filesystem::path uis::UiServer::getImageSelectionRootDirectory()
{
	return _img_select_root_dir;
}

std::filesystem::path uis::UiServer::getImageSelectionCurrentDirectory()
{
	return _img_select_current_dir;
}

void uis::UiServer::listen()
{
	_running = true;
	while (_running) {
		lws_service(_context, 1000);
	}
}

void uis::UiServer::listenAsync()
{
	_net_thread = std::thread(&uis::UiServer::listen, this);
}

void uis::UiServer::shutdown()
{
	_running = false;
	lws_cancel_service(_context);
	if (_net_thread.joinable())
	{
		_net_thread.join();
	}
	lws_context_destroy(_context);
}
