const fs = require('fs');
const vm = require('vm');
const assert = require('assert');
const source = fs.readFileSync('src/web_pages.cpp', 'utf8');
const start = source.indexOf('  let uiUpdateRequired = false;');
const end = source.indexOf('</script>)rawliteral', start);
assert(start > 0 && end > start);
const script = source.slice(start, end);

function scenario(response) {
  const nodes = [];
  const banner = {dataset: {}, style: {}, textContent: ''};
  const element = () => ({style: {}, children: [], setAttribute() {}, appendChild(x) { this.children.push(x); }, addEventListener() {}, focus() {}, contains() { return false; }});
  const document = {body: {appendChild: x => nodes.push(x)}, hidden: false, createElement: element, getElementById: () => banner, addEventListener() {}};
  const context = vm.createContext({document, window: {addEventListener() {}}, location: {reload() {}}, uiBuildAtLoad: 'a'.repeat(64), setInterval() {}, fetch: () => response()});
  vm.runInContext(script, context);
  return {nodes, banner};
}
const tick = () => new Promise(resolve => setImmediate(resolve));
const reply = build => Promise.resolve({ok: true, headers: {get: () => build}, json: () => Promise.resolve({master_status: 'fresh'})});
(async () => {
  let test = scenario(() => reply('a'.repeat(64))); await tick(); assert.equal(test.nodes.length, 0); assert.equal(test.banner.style.display, 'none');
  test = scenario(() => reply('b'.repeat(64))); await tick(); assert.equal(test.nodes.length, 1, 'changed build must block old page');
  test = scenario(() => reply(null)); await tick(); assert.equal(test.nodes.length, 1, 'missing build after a successful response must block old page');
  test = scenario(() => Promise.reject(Error('offline'))); await tick(); assert.equal(test.nodes.length, 0, 'network failure must not be presented as a firmware change');
  console.log('PASS: version match, changed/missing build and network failure');
})().catch(error => { console.error(error); process.exitCode = 1; });
