#include <string>
#include <map>

#include "uiserver.h"

static bool isSafeReqPath(std::filesystem::path root_dir, std::filesystem::path req_path)
{
	std::filesystem::path full_path = std::filesystem::weakly_canonical(req_path);
	std::filesystem::path canonical_base = std::filesystem::canonical(root_dir);

	std::pair<std::filesystem::path::const_iterator, std::filesystem::path::const_iterator> mismatch_result =
		std::mismatch(canonical_base.begin(), canonical_base.end(), full_path.begin(), full_path.end());

	return (mismatch_result.first == canonical_base.end());
}

static int callbackHttp(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len)
{
	struct lws_context* context = lws_get_context(wsi);
	uis::UiServer* server = static_cast<uis::UiServer*>(lws_context_user(context));

    switch (reason) {
	case LWS_CALLBACK_HTTP:
	{
		std::filesystem::path web_root = server->getHttpDirectory();
		std::string uri_path = std::string(static_cast<const char*>(in));
		std::filesystem::path req_path;
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
		break;
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
	    { "http", callbackHttp, 0, 0 },
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
