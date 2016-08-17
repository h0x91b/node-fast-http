#ifndef NODE_HTTP_H
#define NODE_HTTP_H

#include <nan.h>
#include "../deps/http-parser/http_parser.h"

extern "C" {
	#include "../deps/sds/sds.h"
}

#define CHECK(status, msg) \
	if (status != 0) { \
		fprintf(stderr, "%s: %s\n", msg, uv_err_name(status)); \
		exit(1); \
	}

// #define ENABLELOG
#ifdef ENABLELOG
#define LOG(...) fprintf( stderr, __VA_ARGS__ );
#else
#define LOG(...) do{ } while ( false )
#endif


namespace node_http {
	//#define USEBUFFERPOOL
	class HttpServer;
	
	struct ClientContext {
		HttpServer *server;
		uv_tcp_t handle;
		bool keepAlive;
		http_parser parser;
		uv_write_t write_req;
		sds uri;
	};
	void alloc_cb(uv_handle_t * handle, size_t suggested_size, uv_buf_t* buf);
	
	class HttpServer : public node::ObjectWrap {
	public:
		static NAN_METHOD(Listen);
		static NAN_METHOD(New);
		static NAN_METHOD(End);
		static void Init(v8::Handle<v8::Object> exports);
	private:
		static void onConnect(uv_stream_t* server_handle, int status);
		static int onMessageComplete(http_parser *parser);
		static int onUrl(http_parser *parser, const char *at, size_t length);
		static void onRead(uv_stream_t* tcp, ssize_t nread, const uv_buf_t * buf);
		static void onClose(uv_handle_t* handle);
		static void freeClient(ClientContext *client);
		static void afterWrite(uv_write_t* req, int status);
		static Nan::Persistent<v8::Function> constructor;
		Nan::Persistent<v8::Function> jsHandler;
		uv_tcp_t server;
		unsigned short port;
		static http_parser_settings parser_settings;
	};

}

#endif