async (page) => {
  const results=[];
  const check=(value,name)=>{if(!value)throw new Error(name);results.push({name,passed:true});};
  await page.setViewportSize({width:1366,height:768});
  await page.goto('http://127.0.0.1:8899/');
  const input=page.locator('#search-input');
  await input.focus();
  const ime=await input.evaluate(e=>{
    e.dispatchEvent(new CompositionEvent('compositionstart',{bubbles:true,data:''}));
    e.value='北';e.dispatchEvent(new InputEvent('input',{bubbles:true,data:'北',inputType:'insertCompositionText',isComposing:true}));
    const same=document.getElementById('search-input')===e&&e.isConnected;
    e.value='北屿';e.dispatchEvent(new CompositionEvent('compositionend',{bubbles:true,data:'北屿'}));
    return same;
  });
  check(ime,'IME: composing node preserved');
  check(await page.locator('.member-row').count()===1,'IME: compositionend filters results');
  await page.locator('#search-input').fill('');
  await page.locator('[data-join="7"]').click();
  await page.locator('.nav-main [data-page="watch"]').click();
  check((await page.locator('.watch-aside').innerText()).includes('迟夏'),'fifth joined room appears in room list');
  check((await page.locator('.watch-tools').innerText()).includes('4'),'composition preview limit disclosed');
  await page.locator('[data-action="audio"]').click();
  await page.getByRole('checkbox',{name:'全局静音',exact:true}).check();
  await page.getByRole('checkbox',{name:'显示弹幕',exact:true}).uncheck();
  await page.keyboard.press('Escape');
  check((await page.locator('.audio-label').innerText()).includes('全局静音'),'audio dialog updates mute state');
  check(await page.locator('.danmaku-example').count()===0,'audio dialog updates danmaku state');
  await page.locator('.nav-main [data-page="ranking"]').click();
  await page.locator('[data-tab="常规赛"]').click();
  await page.locator('#search-input').fill('NO_MATCH_TEST');
  check(await page.locator('.event-row').count()===0&&await page.locator('.state-box').isVisible(),'event search empty state');
  await page.locator('[data-tab="定级赛"]').click();
  await page.locator('#view-state').selectOption('error');
  check((await page.locator('.state-box').innerText()).includes('暂时无法取得资料'),'placement error state');
  await page.locator('.nav-main [data-page="directory"]').click();
  await page.locator('[data-action="add"]').click();
  await page.locator('[data-action="search-room"]').click();
  check((await page.locator('dialog [role="status"]').innerText()).includes('输入'),'empty search feedback inside modal');
  await page.keyboard.press('Escape');
  await page.setViewportSize({width:390,height:844});
  await page.locator('[data-member-detail="1"]').click();
  await page.locator('dialog [data-action="watch"]').click();
  check(!await page.locator('dialog').isVisible()&&await page.locator('.watch-page').isVisible(),'modal navigation closes modal');
  for(const size of [{width:390,height:844},{width:960,height:600}]){
    await page.setViewportSize(size);
    for(const scheme of ['a','b','c']){
      await page.locator(`.scheme-choice[data-scheme="${scheme}"]`).click();
      for(const key of ['directory','ranking','watch','settings','components']){
        await page.locator(`.app-nav .nav-item[data-page="${key}"]`).click();
        check(await page.evaluate(()=>document.body.scrollWidth<=innerWidth+1),`${size.width}/${scheme}/${key}: body no horizontal overflow`);
      }
      await page.locator('.nav-main [data-page="ranking"]').click();
      await page.screenshot({path:`D:/DouyuMonitor/docs/ui-redesign/visual-review/assets/qa-${scheme}-ranking-${size.width}.png`,fullPage:true});
    }
  }
  return {date:new Date().toISOString(),checks:results.length,results,note:'Synthetic IME events check node lifecycle; physical Windows IME not exercised.'};
}
