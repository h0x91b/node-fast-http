{
	"targets": [
		{
			"target_name": "node_http",
			"sources": [
				"src/node-http.cc",
				"src/buf-pool.cc",
				"deps/sds/sds.c",
				"deps/http-parser/http_parser.c",
			],
			"include_dirs" : [
				"<!(node -e \"require('nan')\")"
			]
		}
	]
}