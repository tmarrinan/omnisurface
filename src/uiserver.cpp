#include "uiserver.h"

static int callbackHttp(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len)
{
	struct lws_context* context = lws_get_context(wsi);
	uis::UiServer* server = static_cast<uis::UiServer*>(lws_context_user(context));

    switch (reason) {
	case LWS_CALLBACK_HTTP:
	{
		std::string uri_path = std::string(static_cast<const char*>(in));
		std::filesystem::path req_path;
		if (uri_path.empty() || uri_path == "/")
		{
			req_path = server->getHttpDirectory() / "index.html";
		}
		else if (uri_path[0] == '/')
		{
			req_path = server->getHttpDirectory() / uri_path.substr(1);
		}
		req_path.make_preferred();
		printf("UiServer> HTTP Request (%p) [%s]\n", server, req_path.string().c_str());
		// TODO: check if path is safe (i.e. within the HTTP dir)
		
		lws_serve_http_file(wsi, req_path.string().c_str(), "text/html", NULL, 0);
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
