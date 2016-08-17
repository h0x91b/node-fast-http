#include <node.h>
#include <nan.h>
#include "node-http.h"
#include "buf-pool.h"

extern "C" {
	#include "../deps/sds/sds.h"
}

namespace node_http {

using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;
using v8::Handle;
using v8::Function;
using v8::FunctionTemplate;
using v8::External;

Nan::Persistent<Function> HttpServer::constructor;
struct http_parser_settings HttpServer::parser_settings;

void HttpServer::Init(Handle<Object> exports) {
	LOG("HttpServer::Init\n");
	
	Nan::HandleScope scope;
	Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
	tpl->SetClassName(Nan::New<String>("HttpServer").ToLocalChecked());
	tpl->InstanceTemplate()->SetInternalFieldCount(1);
	// Prototype
	Nan::SetPrototypeTemplate(tpl, "listen", Nan::New<FunctionTemplate>(Listen));
	Nan::SetPrototypeTemplate(tpl, "end", Nan::New<FunctionTemplate>(End));
	
	constructor.Reset(tpl->GetFunction());
	exports->Set(Nan::New("HttpServer").ToLocalChecked(), tpl->GetFunction());
}

NAN_METHOD(HttpServer::Listen) {
	LOG("HttpServer::Listen, libuv version %s\n", uv_version_string());
	
	Nan::HandleScope scope;
	HttpServer* self = ObjectWrap::Unwrap<HttpServer>(info.This());

	int r;
	r = uv_tcp_init(uv_default_loop(), &self->server);
	CHECK(r, "tcp_init");
	
	struct sockaddr_in bind_addr;
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_port = htons((unsigned short)info[0]->ToUint32()->Value());
	bind_addr.sin_addr.s_addr = INADDR_ANY;

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	int on = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)); //SO_REUSEADDR
	
	bind(fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr));
	
	// struct sockaddr_in address;
	// r = uv_ip4_addr("0.0.0.0", info[0]->ToUint32()->Value(), &address);
	// CHECK(r, "ip4_addr");
	// r = uv_tcp_bind(&self->server, (const struct sockaddr*)&address, 0);
	// CHECK(r, "tcp_bind");
	//
	// self->server.io_watcher.fd = fd;
	
	uv_tcp_open(&self->server, fd);
	//self->server = 
	
	//LOG("Server %p, port: %d\n", &self->server, info[0]->ToUint32()->Value());

	self->server.data = self;
	self->port = (unsigned short)info[0]->ToUint32()->Value();
	self->jsHandler.Reset(Local<Function>::Cast(info[1]));
		
	// struct sockaddr_in address;
	// r = uv_ip4_addr("0.0.0.0", info[0]->ToUint32()->Value(), &address);
	// CHECK(r, "ip4_addr");
	//
	// r = uv_tcp_bind(&self->server, (const struct sockaddr*)&address, 0);
	// CHECK(r, "tcp_bind");

//must be before bind
	// printf("%d\n", self->server.io_watcher.fd);
	// int on = 1;
	// setsockopt(self->server.io_watcher.fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)); //SO_REUSEADDR
	
	r = uv_listen((uv_stream_t*)&self->server, 1, onConnect);
	CHECK(r, "uv_listen");
	
	parser_settings.on_message_complete = onMessageComplete;
	parser_settings.on_url = onUrl;
}

void HttpServer::onConnect(uv_stream_t* server_handle, int status) {
	LOG("%s\n", __PRETTY_FUNCTION__);
	CHECK(status, "onConnect");
	ClientContext *client = (ClientContext*)calloc(sizeof(ClientContext), sizeof(ClientContext));
	client->keepAlive = TRUE;
	client->server = (HttpServer*)server_handle->data;
	uv_tcp_init(uv_default_loop(), &client->handle);
	http_parser_init(&client->parser, HTTP_REQUEST);

	client->parser.data = client;
	client->handle.data = client;

	int r = uv_accept(server_handle, (uv_stream_t*)&client->handle);
	CHECK(r, "accept");

	r = uv_tcp_nodelay(&client->handle, 1);
	CHECK(r, "uv_tcp_nodelay");

	uv_read_start((uv_stream_t*)&client->handle, alloc_cb, onRead);
}

void HttpServer::onRead(uv_stream_t* tcp, ssize_t nread, const uv_buf_t * buf) {
	LOG("%s\n", __PRETTY_FUNCTION__);
	ClientContext* client = (ClientContext*) tcp->data;
	
	if(nread < 0) {
		LOG("nread -1 %zd %s %s\n", nread, uv_err_name(nread) ,uv_strerror(nread));
		uv_close((uv_handle_t*)&client->handle, onClose);
#ifdef USEBUFFERPOOL
		freeBuf(buf->base);
#else
		free(buf->base);
#endif
		return;
	}
	
	LOG("onRead %.*s\n", (int)nread, buf->base);
	http_parser_execute(&client->parser, &parser_settings, buf->base, nread);
#ifdef USEBUFFERPOOL
	freeBuf(buf->base);
#else
	free(buf->base);
#endif
}

void HttpServer::freeClient(ClientContext *client) {
	// if(client->parser) free(client->parser);
	if(client->uri) sdsfree(client->uri);
	free(client);
}

void HttpServer::onClose(uv_handle_t* handle) {
	LOG("onClose %p\n", handle);
	ClientContext* client = (ClientContext*) handle->data;
	freeClient(client);
}

int HttpServer::onUrl(http_parser *parser, const char *at, size_t length) {
	LOG("%s\n", __PRETTY_FUNCTION__);
	ClientContext *client = (ClientContext*)parser->data;
	if(client->uri) sdsfree(client->uri);
	client->uri = sdsnewlen(at, length);
	return 0;
}

NAN_METHOD(HttpServer::End) {
	LOG("%s\n", __PRETTY_FUNCTION__);
	Nan::HandleScope scope;
	static sds resp = sdsnew("");
	resp[0] = '\0';
	sdsupdatelen(resp);
	static sds respBody = sdsnew("");
	respBody[0] = '\0';
	sdsupdatelen(respBody);
	
	ClientContext *client = reinterpret_cast<ClientContext*>(info[0].As<External>()->Value());
	Nan::Utf8String jsRespBody(info[1]);
	respBody = sdscatlen(respBody, *jsRespBody, jsRespBody.length());
	
	resp = sdscatprintf(
		resp, 
		"HTTP/1.1 200 OK\r\n"
		"Server: node-http-fast\r\n"
		"Content-Type: text\\plain\r\n"
		"Connection: Keep-Alive\r\n"
		"Content-Length: %zu\r\n\r\n%s",
		sdslen(respBody),
		respBody
	);
	
	uv_buf_t resbuf;
	resbuf.base = resp;
	resbuf.len = sdslen(resp);
	client->write_req.data = client;
	
	int r = uv_write(&client->write_req,
		(uv_stream_t*)&client->handle,
		&resbuf,
		1,
		afterWrite);
	CHECK(r, "write buff");
}

int HttpServer::onMessageComplete(http_parser *parser) {
	LOG("%s\n", __PRETTY_FUNCTION__);
	Nan::HandleScope scope;
	ClientContext *client = (ClientContext*)parser->data;
	Local<External> extContext = External::New(v8::Isolate::GetCurrent(), parser->data);
	Local<Value> argv[] = {
		extContext,
		Nan::New(client->uri).ToLocalChecked()
	};
	Nan::New(client->server->jsHandler)->Call(Nan::GetCurrentContext()->Global(), 2, argv);
	return 0;
}

void HttpServer::afterWrite(uv_write_t* req, int status) {
	CHECK(status, "write");
	ClientContext* client = (ClientContext*)req->data;
	if (!client->keepAlive && !uv_is_closing((uv_handle_t*)req->handle))
	{
		LOG("afterWrite\n");
		uv_close((uv_handle_t*)req->handle, onClose);
	} else if(uv_is_closing((uv_handle_t*)req->handle)) {
		LOG("uv_is_closing!\n");
	}
}

NAN_METHOD(HttpServer::New) {
	Nan::HandleScope scope;
	HttpServer *server = new HttpServer();
	server->Wrap(info.This());
	info.GetReturnValue().Set(info.This());
}

void alloc_cb(uv_handle_t * handle, size_t suggested_size, uv_buf_t* buf) {
	// static char *ptr = (char*)malloc(suggested_size);
	// *buf = uv_buf_init(ptr, suggested_size);
#ifdef USEBUFFERPOOL
	*buf = uv_buf_init((char*)allocBuf(suggested_size), suggested_size);
#else
	*buf = uv_buf_init((char*) malloc(suggested_size), suggested_size);
#endif
}

void init(Local<Object> exports) {
	HttpServer::Init(exports);
}

NODE_MODULE(node_http, init)

} // namespace node_http