const fs = require('fs');

let content = fs.readFileSync('targets/wasm/js/demo.js', 'utf8');
content = content.replace(/wetElement: null,\n/g, '');
content = content.replace(/previewGraph\.wetElement = null; \/\/ Removed\n/g, '');

fs.writeFileSync('targets/wasm/js/demo.js', content);
console.log("wetElement removed");
