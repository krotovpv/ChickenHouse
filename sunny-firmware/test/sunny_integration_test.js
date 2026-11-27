const fs=require('fs'),assert=require('assert'),crypto=require('crypto');
const current=fs.readFileSync('src/web_pages.cpp','utf8').replace(/\r\n/g,'\n'),main=fs.readFileSync('src/main.cpp','utf8');const baseline=JSON.parse(fs.readFileSync('test/sunny_contract_baseline.json','utf8'));
const extract=(s,p)=>[...s.matchAll(p)].map(m=>m[1]);const missing=(a,b)=>a.filter(x=>!b.includes(x));
assert.deepStrictEqual(missing(baseline.elementIds,extract(current+fs.readFileSync('src/table_assets.h','utf8'),/id=['\"]([^'\"]+)['\"]/g)),[],'existing page IDs');
assert.deepStrictEqual(missing(baseline.routes,extract(main,/webServer\.on\("([^"]+)"/g)),[],'existing routes');
for(const u of ['/','/climate','/ventilation','/door','/feeding','/light','/alerts','/table','/settings','/update'])assert(current.includes(`href='${u}'`),'navigation '+u);
for(const name of ['climate','ventilation','door','feeding','light'])assert(current.includes('sunny-'+name),'overview '+name);
assert(current.includes('sunny-navigation'),'navigation');assert(current.includes('/sunny-logo.png'),'logo');assert(fs.existsSync('src/sunny_logo.h'),'logo bytes');assert(!current.includes('class="demo-bit"'),'no demo switches');
for(const [name,expected] of Object.entries(baseline.scriptHashes)){let i=current.indexOf('void '+name+'()'),j=current.indexOf('\nvoid ',i+5),x=current.slice(i,j<0?undefined:j),k=x.indexOf('<script>');assert(k>=0,'missing script '+name);let script=x.slice(k,x.indexOf('</script>',k)+9);assert.equal(crypto.createHash('sha256').update(script).digest('hex'),expected,'data script changed '+name)}
console.log('PASS: page IDs, routes and tracked data scripts preserved; sunny navigation and cards present');

