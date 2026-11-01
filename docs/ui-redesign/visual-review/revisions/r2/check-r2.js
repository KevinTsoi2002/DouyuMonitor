// Isolated webpage review checks. Does not connect to native services.
async (page) => {
  const base='http://127.0.0.1:8899/';
  const sizes=[{width:1280,height:800},{width:1366,height:768},{width:1920,height:1080}];
  const assertions=[],matrix=[],baseline=[],geometry=[];
  const errors=[];
  page.on('pageerror',error=>errors.push(error.message));
  const check=(condition,name)=>{assertions.push({name,passed:!!condition});if(!condition)throw new Error(name);};
  const metrics=()=>{
    const rect=e=>{const r=e.getBoundingClientRect();return {x:r.x,y:r.y,width:r.width,height:r.height};};
    const area=r=>r.width*r.height;
    const imageRects=[...document.querySelectorAll('.video-scene')].map(e=>{
      const r=rect(e),width=Math.min(r.width,r.height*16/9),height=width*9/16;
      return {x:r.x+(r.width-width)/2,y:r.y+(r.height-height)/2,width,height};
    });
    const grid=document.querySelector('.video-grid'),stage=document.querySelector('.watch-stage'),dock=document.querySelector('.watch-dock');
    const overlap=(a,b)=>Math.max(0,Math.min(a.x+a.width,b.x+b.width)-Math.max(a.x,b.x))*Math.max(0,Math.min(a.y+a.height,b.y+b.height)-Math.max(a.y,b.y));
    return {
      viewport:{width:innerWidth,height:innerHeight},
      app:rect(document.querySelector('.app-window')),
      grid:grid?rect(grid):null,stage:stage?rect(stage):null,
      tiles:[...document.querySelectorAll('.video-tile')].map(rect),
      sceneArea:[...document.querySelectorAll('.video-scene')].reduce((s,e)=>s+area(rect(e)),0),
      imageArea:imageRects.reduce((s,r)=>s+area(r),0),imageRects,
      dock:dock?rect(dock):null,dockImageOverlap:dock?imageRects.reduce((s,r)=>s+overlap(r,rect(dock)),0):0,
      overflow:document.body.scrollWidth>innerWidth+1||document.querySelector('main').scrollWidth>document.querySelector('main').clientWidth+1,
      missingImages:[...document.images].filter(i=>!i.complete||!i.naturalWidth).map(i=>i.src),
      overflowingControls:[...document.querySelectorAll('.scheme-choice,.record-choice,main .button,.toolbar,.member-row,.rank-row')].filter(e=>e.getClientRects().length&&e.scrollWidth>e.clientWidth+2).map(e=>e.textContent.trim().slice(0,60))
    };
  };
  for(const size of sizes){
    await page.setViewportSize(size);
    await page.goto(base+'revisions/r1/');
    await page.locator('.scheme-choice[data-scheme="c"]').click();
    await page.locator('.app-nav [data-page="watch"]').click();
    await page.mouse.move(3,3);
    baseline.push({size,layout:'R1 C default bottom',...await page.evaluate(metrics)});
    await page.locator('[data-action="layout"]').click();
    await page.locator('[data-layout-choice="主画面 + 侧列"]').click();
    baseline.push({size,layout:'R1 C matched side',...await page.evaluate(metrics)});
  }
  await page.goto(base);
  const savedPreference=await page.evaluate(()=>localStorage.getItem('cstg-visual-review-r2-preference'));
  try{
    for(const size of sizes){
      await page.setViewportSize(size);
      for(const palette of ['c1','c2','c3']){
        await page.locator(`.scheme-choice[data-palette="${palette}"]`).click();
        for(const screen of ['directory','ranking','watch','settings','components']){
          await page.locator(`.app-nav [data-page="${screen}"]`).click();
          await page.mouse.move(3,3);
          const result=await page.evaluate(metrics);
          matrix.push({size,palette,screen,...result});
          check(!result.overflow,`${size.width}/${palette}/${screen}: no horizontal overflow`);
          check(!result.missingImages.length,`${size.width}/${palette}/${screen}: image resources loaded`);
          check(!result.overflowingControls.length,`${size.width}/${palette}/${screen}: controls fit`);
          if(['directory','ranking','watch'].includes(screen))await page.screenshot({path:`D:/DouyuMonitor/docs/ui-redesign/visual-review/assets/r2-${palette}-${screen}-${size.width}.png`,fullPage:true});
        }
        await page.locator('.app-nav [data-page="watch"]').click();
        await page.evaluate(()=>window.__r2VideoNodes=[...document.querySelectorAll('.video-tile,.video-scene')]);
        const before=await page.evaluate(metrics);
        for(const action of ['rooms','layout','audio']){
          await page.locator(`#watch-${action}`).click();
          check(await page.locator('dialog').getAttribute('data-presentation')==='drawer',`${size.width}/${palette}/${action}: drawer presentation`);
          check(await page.locator(`#watch-${action}`).getAttribute('aria-expanded')==='true',`${size.width}/${palette}/${action}: expanded state`);
          const after=await page.evaluate(metrics);
          const delta=Math.max(...before.tiles.flatMap((r,i)=>['x','y','width','height'].map(key=>Math.abs(r[key]-after.tiles[i][key]))));
          geometry.push({size,palette,action,delta});
          check(delta<=1,`${size.width}/${palette}/${action}: no video squeeze`);
          check(await page.evaluate(()=>window.__r2VideoNodes.every((n,i)=>n===[...document.querySelectorAll('.video-tile,.video-scene')][i])),`${size.width}/${palette}/${action}: video nodes preserved`);
          const panel=await page.locator('dialog').boundingBox();
          check(panel.x>=0&&panel.y>=0&&panel.x+panel.width<=size.width+1&&panel.y+panel.height<=size.height+1,`${size.width}/${palette}/${action}: drawer inside viewport`);
          if(action==='rooms'&&size.width===1366)await page.screenshot({path:`D:/DouyuMonitor/docs/ui-redesign/visual-review/assets/r2-${palette}-drawer.png`});
          await page.keyboard.press('Escape');
          check(await page.evaluate(id=>document.activeElement.id===id,`watch-${action}`),`${size.width}/${palette}/${action}: Escape restores trigger focus`);
        }
      }
    }
    await page.setViewportSize({width:1366,height:768});
    await page.locator('.scheme-choice[data-palette="c1"]').click();
    await page.locator('.app-nav [data-page="directory"]').click();
    await page.locator('#search-input').fill('北屿');
    check(await page.locator('.member-list .member-row').count()===1,'name search');
    await page.locator('#search-input').fill('不存在');
    check(await page.locator('.state-box').isVisible(),'search empty state');
    await page.locator('[data-action="clear"]').click();
    await page.locator('[data-filter="online"]').click();
    check(await page.locator('.member-list .member-row').count()===5,'live filter');
    await page.locator('[data-filter="all"]').click();
    await page.locator('[data-join="7"]').click();
    await page.locator('.app-nav [data-page="watch"]').click();
    await page.locator('#watch-rooms').click();
    check(await page.locator('dialog .watch-room').count()===5,'fifth room remains in drawer');
    await page.locator('dialog [data-action="add"]').click();
    check(await page.locator('dialog').getAttribute('data-presentation')==='modal','drawer to centered add dialog resets presentation');
    await page.keyboard.press('Escape');
    check(await page.evaluate(()=>document.activeElement.id==='watch-rooms'),'nested dialog returns to original room trigger');
    await page.locator('#watch-audio').click();
    await page.locator('[data-audio="muted"]').check();
    await page.locator('[data-audio="danmaku"]').uncheck();
    for(let i=0;i<12;i++){await page.keyboard.press('Tab');check(await page.evaluate(()=>document.querySelector('dialog').contains(document.activeElement)),`drawer Tab containment ${i}`);}
    await page.keyboard.press('Escape');
    check(await page.locator('.danmaku-example').isHidden(),'danmaku state updated');
    check(await page.locator('.main-video .video-heading small').innerText()==='主画面 · 全局静音','mute state updated');
    await page.locator('#watch-layout').click();
    await page.locator('[data-layout-choice="等分网格"]').click();
    await page.evaluate(()=>window.__r2VideoNodes=[...document.querySelectorAll('.video-tile,.video-scene')]);
    for(const palette of ['c2','c3','c1']){
      await page.locator(`.scheme-choice[data-palette="${palette}"]`).click();
      check(await page.locator('body').getAttribute('data-scheme')==='c','C structure retained '+palette);
      check(await page.locator('.video-grid').getAttribute('data-layout')==='等分网格','layout retained '+palette);
      check(await page.locator('.danmaku-example').isHidden(),'danmaku retained '+palette);
      check(await page.evaluate(()=>window.__r2VideoNodes.every((n,i)=>n===[...document.querySelectorAll('.video-tile,.video-scene')][i])),'palette preserves video nodes '+palette);
    }
    await page.locator('#watch-tools-toggle').click();
    check(await page.locator('#watch-dock-main').isHidden(),'collapse removes tools from view');
    await page.keyboard.press('Tab');
    check(!(await page.evaluate(()=>document.querySelector('#watch-dock-main').contains(document.activeElement))),'collapsed tools are not keyboard stops');
    await page.locator('#watch-tools-toggle').click();
    await page.locator('.video-tile').first().focus();
    check(await page.locator('.main-video .video-controls').isVisible(),'tile focus reveals controls');
    await page.keyboard.press('Tab');
    check(await page.evaluate(()=>document.activeElement.dataset.action==='playback'),'tile keyboard enters visible playback control');
    await page.locator('#watch-immersive').click();
    check(await page.locator('.app-nav').isHidden(),'immersive hides navigation');
    const immersive=await page.evaluate(metrics);
    await page.screenshot({path:'D:/DouyuMonitor/docs/ui-redesign/visual-review/assets/r2-immersive.png'});
    await page.locator('#watch-rooms').click();
    await page.keyboard.press('Escape');
    check(await page.locator('body').evaluate(el=>el.classList.contains('immersive')),'first Escape only closes drawer');
    await page.keyboard.press('Escape');
    check(!(await page.locator('body').evaluate(el=>el.classList.contains('immersive'))),'second Escape exits immersive');
    check(await page.evaluate(()=>document.activeElement.id==='watch-immersive'),'immersive exit focus restored');
    check(await page.evaluate(()=>window.__r2VideoNodes.every((n,i)=>n===[...document.querySelectorAll('.video-tile,.video-scene')][i])),'immersion preserves video nodes');
    await page.locator('#record-choice').click();
    await page.locator('#preference-note').fill('R2 test; restored after validation');
    await page.locator('[data-action="save-preference"]').click();
    const stored=await page.evaluate(()=>JSON.parse(localStorage.getItem('cstg-visual-review-r2-preference')));
    check(stored.revision==='r2'&&stored.components==='C'&&stored.scheme==='c1','versioned preference saved');
    await page.locator('.app-nav [data-page="ranking"]').click();
    check((await page.locator('.rank-row').allTextContents()).some(t=>t.includes('—')),'missing score remains dash');
    for(const state of ['loading','empty','error']){await page.locator('#view-state').selectOption(state);check(await page.locator('.state-box').isVisible(),'ranking '+state);}
    check(errors.length===0,'no runtime JavaScript errors');
    return {revision:'r2',date:new Date().toISOString(),assertions,matrix,baseline,geometry,immersive,errors};
  }finally{
    await page.evaluate(value=>{if(value===null)localStorage.removeItem('cstg-visual-review-r2-preference');else localStorage.setItem('cstg-visual-review-r2-preference',value);},savedPreference);
    await page.reload();
  }
}
