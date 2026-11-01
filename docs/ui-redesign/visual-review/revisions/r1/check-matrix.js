// Run with Playwright MCP browser_run_code_unsafe filename; not production code.
async (page) => {
  const results=[];
  const sizes=[{width:1280,height:800},{width:1366,height:768},{width:1920,height:1080}];
  await page.goto('http://127.0.0.1:8899/');
  for(const size of sizes){
    await page.setViewportSize(size);
    for(const scheme of ['a','b','c']){
      await page.emulateMedia({colorScheme:scheme==='b'?'light':'dark',reducedMotion:'reduce'});
      await page.locator(`.scheme-choice[data-scheme="${scheme}"]`).click();
      for(const key of ['directory','ranking','watch','settings','components']){
        await page.locator(`.app-nav .nav-item[data-page="${key}"]`).click();
        const metrics=await page.evaluate(()=>{
          const main=document.querySelector('main');
          const images=[...document.images].filter(i=>!i.complete||i.naturalWidth===0).map(i=>i.getAttribute('src'));
          const overflow=[...document.querySelectorAll('main .button,main .member-row,main .rank-row,.app-nav,.toolbar,.page-header,.review-bar')].filter(e=>{
            const r=e.getBoundingClientRect();return r.width>0&&(r.left< -1||r.right>innerWidth+1||e.scrollWidth>e.clientWidth+2);
          }).map(e=>({tag:e.className,text:e.innerText.slice(0,60),width:e.clientWidth,scroll:e.scrollWidth}));
          const controls=[...document.querySelectorAll('.video-controls')].filter(e=>getComputedStyle(e).display!=='none').map(e=>({bottom:e.getBoundingClientRect().bottom,mainBottom:main.getBoundingClientRect().bottom}));
          return {bodyWidth:document.body.scrollWidth,viewport:innerWidth,mainWidth:main.clientWidth,mainScrollWidth:main.scrollWidth,images,overflow,controls,watchHeight:main.scrollHeight-main.clientHeight};
        });
        if(['directory','ranking','watch'].includes(key))await page.screenshot({path:`D:/DouyuMonitor/docs/ui-redesign/visual-review/assets/qa-${scheme}-${key}-${size.width}.png`,fullPage:true});
        results.push({size,scheme,page:key,...metrics});
      }
    }
  }
  return {date:new Date().toISOString(),cases:results.length,results};
}
