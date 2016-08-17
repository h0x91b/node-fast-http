const addon = require('./build/Release/node_http');

//console.log(addon.hello()); // 'world'

//addon.listen(8080);
//addon.listen(8081);

const cluster = require('cluster');

if (cluster.isMaster) {
	console.log('master');
	for(var i=0;i<2;i++)
		cluster.fork();
} else {
	console.log('worker', process.pid);
	process.title = 'child-'+process.pid;
	console.log('Hello');
	var cServer = new addon.HttpServer();
	console.log(cServer.listen(8080, handler));
	var cServer2 = new addon.HttpServer();
	console.log(cServer2.listen(8081, handler));


	var i = 0;
	function handler(clientContext, uri) {
		console.log('handler(%s)', uri, process.pid);
		// setTimeout(()=>{
		// 	// cServer.end(clientContext, 'Hello '+(i++));
		// 	onEnd();
		// }, 1);
		cServer.end(clientContext, 'Hello '+(i++));
	}
}