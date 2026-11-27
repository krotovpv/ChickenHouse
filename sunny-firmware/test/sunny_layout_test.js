const fs=require('fs'),assert=require('assert');const s=fs.readFileSync('src/web_pages.cpp','utf8');const start=s.indexOf('void handleIndex()'),end=s.indexOf('\nvoid handleApiData()',start),home=s.slice(start,end);
assert(home.includes("<body class='sunny-home'>"),'Главной странице нужен отдельный адаптивный макет');
assert(home.includes("class='container sunny-layout-grid'"),'Карточки должны размещаться в общей сетке');
assert(home.includes("class='card sunny-alert-summary'"),'Нужна карточка перехода к авариям');
assert(s.includes('grid-template-columns:repeat(2,minmax(0,1fr))'),'На широком экране нужны две колонки');
assert(s.includes('grid-template-columns:1fr'),'На телефоне нужна одна колонка');
assert(s.includes('max(0px,calc((100vw - 1320px)/2))'),'Навигация должна быть привязана к центрированной области');
assert(s.includes('body:not(.sunny-home){box-sizing:border-box;'), 'Desktop side padding must not overflow the viewport');
console.log('PASS: responsive overview structure is present');
