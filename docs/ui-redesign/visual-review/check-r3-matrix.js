// Review-only browser checks; no native services or external data.
async (page) => {
  const results=[],errors=[];
  page.on('pageerror',e=>errors.push(e.message));
  await page.goto('http://127.0.0.1:8899/');
  const theme=await page.evaluate(()=>localStorage.getItem('cstg-review-theme'));
  try{
    for(const size of [{width:1280,height:800},{width:1366,height:768},{width:1920,height:1080}]){
      await page.setViewportSize(size);
      for(const palette of ['c1','c3']){
        await page.locator(`.scheme-choice[data-palette="${palette}"]`).click();
        for(const screen of ['directory','ranking','watch','settings','components']){
          await page.locator(`.app-nav .nav-item[data-page="${screen}"]`).click();
          await page.mouse.move(2,2);
          const metrics=await page.evaluate(()=>{
            const area=r=>r.width*r.height;
            const rect=e=>{const r=e.getBoundingClientRect();return {x:r.x,y:r.y,width:r.width,height:r.height};};
            const stage=document.querySelector('.watch-stage'),tool=document.querySelector('#watch-tools-trigger');
            const videoRects=[...document.querySelectorAll('.video-scene')].map(rect);
            const toolRect=rect(tool),stageRect=stage?rect(stage):null;
            return {
              overflow:document.body.scrollWidth>innerWidth+1||document.querySelector('main').scrollWidth>document.querySelector('main').clientWidth+1,
              missing:[...document.images].filter(i=>!i.complete||!i.naturalWidth).map(i=>i.getAttribute('src')),
              overflowingControls:[...document.querySelectorAll('.scheme-choice,.record-choice,main .button,.member-row,.rank-row')].filter(e=>e.getClientRects().length&&e.scrollWidth>e.clientWidth+2).map(e=>e.textContent.trim().slice(0,40)),
              obsoleteOverlays:document.querySelectorAll('.scene-caption,.watch-dock,.watch-watermark').length,
              titlebarHeight:document.querySelector('.titlebar').getBoundingClientRect().height,
              toolRect,stageRect,sceneArea:videoRects.reduce((s,r)=>s+area(r),0),
              contained16x9Area:videoRects.reduce((s,r)=>{const w=Math.min(r.width,r.height*16/9);return s+w*w*9/16;},0),
              toolOutsideVideo:!stage||toolRect.y+toolRect.height<=stageRect.y,
              edgeHidden:!stage||getComputedStyle(document.querySelector('.edge-panel')).visibility==='hidden',
              modes:[...document.querySelectorAll('.scheme-choice')].map(b=>b.dataset.palette)
            };
          });
          const result={size,palette,screen,...metrics};results.push(result);
          if(metrics.overflow||metrics.missing.length||metrics.overflowingControls.length||metrics.obsoleteOverlays||metrics.titlebarHeight!==36||!metrics.toolOutsideVideo||!metrics.edgeHidden||metrics.modes.join(',')!=='c1,c3')throw new Error(JSON.stringify(result));
          if(['directory','ranking','watch'].includes(screen))await page.screenshot({path:`D:/DouyuMonitor/docs/ui-redesign/visual-review/assets/r3-${palette}-${screen}-${size.width}.png`});
        }
      }
    }
    if(errors.length)throw new Error(errors.join(';'));
    return {revision:'r3',date:new Date().toISOString(),cases:results.length,results,errors};
  }finally{
    await page.evaluate(value=>{if(value===null)localStorage.removeItem('cstg-review-theme');else localStorage.setItem('cstg-review-theme',value);},theme);
    await page.reload();
  }
}
