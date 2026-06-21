// L26 编译器桥接服务器
// 启动: node server.js
// 浏览器打开 http://localhost:8765 即可使用

const http = require('http');
const { execSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const os = require('os');

const PORT = 8765;
const COMPILER = path.join(__dirname, '..', 'Compiler', 'l26compiler.exe');
const INDEX_HTML = path.join(__dirname, 'index.html');

http.createServer((req, res) => {
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'POST, OPTIONS, GET');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

    if (req.method === 'OPTIONS') {
        res.writeHead(204);
        res.end();
        return;
    }

    // 首页：直接提供 index.html，前端就在本地
    if (req.method === 'GET' && (req.url === '/' || req.url === '')) {
        fs.readFile(INDEX_HTML, 'utf-8', (err, data) => {
            if (err) {
                res.writeHead(500);
                res.end('无法加载 index.html');
            } else {
                res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
                res.end(data);
            }
        });
        return;
    }

    // 编译接口：前端 POST 源码，后端返回 JSON
    if (req.method === 'POST' && req.url === '/compile') {
        let body = '';
        req.on('data', chunk => body += chunk);
        req.on('end', () => {
            let source;
            try {
                source = JSON.parse(body).source || '';
            } catch (_) {
                source = body;
            }

            const tmpFile = path.join(os.tmpdir(), 'l26_' + Date.now() + '.l26');
            try {
                fs.writeFileSync(tmpFile, source, 'utf-8');
                const result = execSync(
                    `"${COMPILER}" "${tmpFile}" --json`,
                    { encoding: 'utf-8', timeout: 10000 }
                );
                res.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8' });
                res.end(result);
            } catch (e) {
                res.writeHead(500, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({
                    tokens: [],
                    pcode: [],
                    errors: [{ line: 0, col: 0, msg: '编译器执行失败: ' + (e.stderr || e.message) }]
                }));
            } finally {
                try { fs.unlinkSync(tmpFile); } catch (_) { }
            }
        });
        return;
    }

    res.writeHead(404);
    res.end('Not found');
}).listen(PORT, () => {
    console.log('========================================');
    console.log('  L26 编译器已启动！');
    console.log('  浏览器打开: http://localhost:' + PORT);
    console.log('  按 Ctrl+C 停止');
    console.log('========================================');
});
