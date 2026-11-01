async (page) => {
  const base='http://127.0.0.1:8899/';
  const originalViewport=page.viewportSize();
  await page.goto(base);
  const originalTheme=await page.evaluate(()=>localStorage.getItem('cstg-review-theme'));
  try{
    await page.setViewportSize({width:1366,height:768});
    await page.locator('.scheme-choice[data-palette="c1"]').click();
    await page.mouse.move(2,2);
    await page.screenshot({path:'D:/DouyuMonitor/docs/ui-redesign/visual-review/assets/r3-c1-watch-1366.png'});
    await page.locator('#watch-tools-trigger').click();
    await page.screenshot({path:'D:/DouyuMonitor/docs/ui-redesign/visual-review/assets/r3-c1-tools-drawer.png'});
    await page.locator('dialog [data-action="immersive"]').click();
    await page.mouse.move(400,500);
    await page.screenshot({path:'D:/DouyuMonitor/docs/ui-redesign/visual-review/assets/r3-immersive-hidden-tools.png'});
    await page.mouse.move(1300,3);
    await page.locator('.edge-panel').waitFor({state:'visible'});
    await page.screenshot({path:'D:/DouyuMonitor/docs/ui-redesign/visual-review/assets/r3-immersive-edge-tools.png'});
    await page.keyboard.press('Escape');
    await page.locator('.scheme-choice[data-palette="c3"]').click();
    await page.mouse.move(2,2);
    await page.screenshot({path:'D:/DouyuMonitor/docs/ui-redesign/visual-review/assets/r3-c3-watch-1366.png'});
    return {revision:'r3',title:await page.title(),nightMode:await page.locator('body').getAttribute('data-palette'),screenshots:5};
  }finally{
    await page.evaluate(value=>{if(value===null)localStorage.removeItem('cstg-review-theme');else localStorage.setItem('cstg-review-theme',value);},originalTheme);
    await page.setViewportSize(originalViewport||{width:1366,height:768});
    await page.reload();
    await page.evaluate(value=>{if(value===null)localStorage.removeItem('cstg-review-theme');else localStorage.setItem('cstg-review-theme',value);},originalTheme);
  }
}
