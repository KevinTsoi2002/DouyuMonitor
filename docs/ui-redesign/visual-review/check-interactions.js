async (page) => {
  const results=[];
  function check(condition,name){if(!condition)throw new Error(name);results.push({name,passed:true});}
  await page.setViewportSize({width:1366,height:768});
  await page.goto('http://127.0.0.1:8899/');
  const originalPreference=await page.evaluate(()=>localStorage.getItem('cstg-visual-review-preference'));
  for(const scheme of ['a','b','c']){
    await page.reload();
    await page.locator(`.scheme-choice[data-scheme="${scheme}"]`).click();
    check(await page.locator('body').getAttribute('data-scheme')===scheme,`${scheme}: scheme selection`);
    await page.locator('#search-input').fill('北屿');
    check(await page.locator('.member-list .member-row').count()===1,`${scheme}: name search`);
    await page.locator('#search-input').fill('10003');
    check(await page.locator('.member-list .member-row').count()===1,`${scheme}: room search`);
    await page.locator('#search-input').fill('不存在的示例');
    check(await page.getByText('没有找到匹配的内容',{exact:true}).isVisible(),`${scheme}: search empty state`);
    await page.locator('[data-action="clear"]').click();
    await page.locator('[data-filter="online"]').click();
    check(await page.locator('.member-list .member-row').count()===5,`${scheme}: online filter`);
    await page.locator('#team-filter').selectOption('一队');
    check(await page.locator('.member-list .member-row').count()===3,`${scheme}: combined team filter`);
    await page.locator('[data-filter="all"]').click();
    await page.locator('#team-filter').selectOption('all');
    await page.locator('[data-member-detail="2"]').click();
    check((await page.locator('.detail-panel h2').allTextContents()).includes('小满'),`${scheme}: member selection`);
    await page.locator('[data-join="9"]').click();
    check(await page.locator('dialog').isVisible()&&(await page.locator('#dialog-title').innerText())==='确认房间号',`${scheme}: unresolved room guard`);
    await page.keyboard.press('Escape');
    await page.locator('[data-join="7"]').click();
    check(await page.locator('#nav-room-count').innerText()==='5',`${scheme}: demo join feedback`);
    for(const state of ['loading','empty','error']){
      await page.locator('#view-state').selectOption(state);
      check(await page.locator('.state-box').isVisible(),`${scheme}: directory ${state}`);
    }
    await page.locator('[data-action="retry"]').click();
    check(await page.locator('.member-row').count()===10,`${scheme}: retry recovery`);
    await page.locator('.nav-main [data-page="ranking"]').click();
    await page.locator('#search-input').fill('小满');
    check(await page.locator('.rank-row').count()===1,`${scheme}: rank search`);
    await page.locator('#search-input').fill('');
    await page.locator('.rank-row [data-rank-detail="3"]').last().click();
    check((await page.locator('.rank-detail h2').allTextContents()).includes('白舟'),`${scheme}: rank detail selection`);
    check((await page.locator('.rank-row').last().innerText()).includes('—'),`${scheme}: missing score marker`);
    for(const state of ['loading','empty','error']){
      await page.locator('#view-state').selectOption(state);
      check(await page.locator('.state-box').isVisible(),`${scheme}: ranking ${state}`);
    }
    await page.locator('#view-state').selectOption('normal');
    for(const tab of ['常规赛','总决赛','足球赛','定级赛']){
      await page.locator('[data-tab="'+tab+'"]').click();
      check(await page.locator('[data-tab="'+tab+'"]').getAttribute('aria-pressed')==='true',`${scheme}: ${tab} switch`);
    }
    await page.locator('.nav-main [data-page="watch"]').click();
    check(await page.locator('.video-tile').count()===4,`${scheme}: four composition previews`);
    await page.locator('[data-action="danmaku"]').click();
    check(await page.locator('.danmaku-example').count()===0,`${scheme}: danmaku preview toggle`);
    await page.locator('.main-video [data-action="mute"]').click();
    check((await page.locator('.audio-label').innerText()).includes('全局静音'),`${scheme}: mute preview`);
    await page.locator('[data-action="layout"]').click();
    await page.locator('[data-layout-choice="等分网格"]').click();
    check(await page.locator('.video-grid').getAttribute('data-layout')==='等分网格',`${scheme}: layout preview`);
    await page.locator('.nav-item[data-page="settings"]').click();
    for(const tab of ['teams','updates','roles','general']){
      await page.locator(`[data-settings="${tab}"]`).click();
      check(await page.locator(`[data-settings="${tab}"]`).getAttribute('aria-pressed')==='true',`${scheme}: settings ${tab}`);
    }
    await page.locator('[data-action="close-preview"]').click();
    check(await page.getByRole('button',{name:'完全退出',exact:true}).isVisible(),`${scheme}: close dialog`);
    await page.keyboard.press('Escape');
    await page.locator('.nav-item[data-page="components"]').click();
    await page.locator('[data-action="teams"]').click();
    await page.locator('[data-action="import-preview"]').click();
    check(await page.locator('#dialog-title').innerText()==='确认导入预览',`${scheme}: import preview gate`);
    await page.keyboard.press('Escape');
    await page.locator('#record-choice').click();
    await page.locator('#preference-note').fill('自动验证用临时备注');
    await page.locator('[data-action="save-preference"]').click();
    await page.reload();
    check((await page.locator('#saved-preference').innerText()).includes(scheme.toUpperCase()),`${scheme}: preference survives reload`);
  }
  await page.evaluate(value=>{if(value===null)localStorage.removeItem('cstg-visual-review-preference');else localStorage.setItem('cstg-visual-review-preference',value);},originalPreference);
  await page.reload();
  await page.keyboard.press('/');
  check(await page.locator('#search-input').evaluate(e=>e===document.activeElement),'keyboard: slash search');
  await page.locator('#record-choice').click();
  const focus=[];
  for(let i=0;i<12;i++){await page.keyboard.press('Tab');focus.push(await page.evaluate(()=>Boolean(document.activeElement.closest('dialog'))));}
  check(focus.every(Boolean),'keyboard: dialog focus stays inside');
  await page.keyboard.press('Escape');
  check(!await page.locator('dialog').isVisible(),'keyboard: escape closes dialog');
  check(await page.locator('#record-choice').evaluate(e=>e===document.activeElement),'keyboard: dialog returns focus');
  return {date:new Date().toISOString(),checks:results.length,results,temporaryPreferenceRestored:true};
}
