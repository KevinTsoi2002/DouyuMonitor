'use strict';

// This isolated review never contacts Douyu, the native service, or a remote API.
const schemes = {
  c1: {name:'石墨 · 青绿', description:'C1：中性石墨暗底，青绿标示选中与主操作。C 组件与按需展开的观看工具。', color:'石墨 / 青绿', detail:'背景接近中性深灰，青绿用于操作和选中状态。适合比较克制底色与画面之间的关系。'},
  c2: {name:'深林 · 嫩绿', description:'C2：暖绿暗底，嫩绿标示选中与主操作。组件与布局和 C1 完全一致。', color:'深林 / 嫩绿', detail:'背景带暖绿倾向，强调色偏嫩绿，层次更柔和。组件、间距与视频占比和其他配色保持一致。'},
  c3: {name:'雾白 · 苔绿', description:'C3：雾白浅底，苔绿标示选中与主操作。视频画布保持深色。', color:'雾白 / 苔绿', detail:'阅读区域采用浅色底面与深绿文字强调，视频区域保持深色。用于比较明亮环境下的阅读感受。'}
};
const pages = {directory:'主播导航',ranking:'团播榜单',watch:'直播工作区',settings:'设置',components:'组件与状态'};
const members = [
  {id:1,name:'北屿',room:'示例 10001',team:'一队',role:'队长',live:'online',title:'一起看今晚的团播',score:15.6,grade:'S',tone:1},
  {id:2,name:'小满',room:'示例 10002',team:'一队',role:'队员',live:'online',title:'今天也要认真发挥',score:15.2,grade:'S',tone:2},
  {id:3,name:'白舟',room:'示例 10003',team:'一队',role:'队员',live:'offline',title:'暂无直播',score:14.8,grade:'A',tone:3},
  {id:4,name:'南星',room:'示例 10004',team:'一队',role:'队员',live:'online',title:'新的一局，准备出发',score:14.3,grade:'A',tone:4},
  {id:5,name:'阿岚',room:'示例 10005',team:'二队',role:'队长',live:'online',title:'队伍集合，一起开场',score:13.9,grade:'A',tone:3},
  {id:6,name:'听雨',room:'示例 10006',team:'二队',role:'队员',live:'offline',title:'暂无直播',score:13.4,grade:'B',tone:1},
  {id:7,name:'迟夏',room:'示例 10007',team:'二队',role:'队员',live:'online',title:'晚间练习室',score:12.8,grade:'B',tone:2},
  {id:8,name:'长风',room:'示例 10008',team:'二队',role:'队员',live:'offline',title:'暂无直播',score:12.3,grade:'B',tone:4},
  {id:9,name:'未眠',room:'待确认',team:'未分队',role:'成员',live:'unknown',title:'尚未确认房间号',score:null,grade:'—',tone:1},
  {id:10,name:'暮川',room:'示例 10010',team:'未分队',role:'成员',live:'unknown',title:'直播状态待查询',score:null,grade:'—',tone:3}
];
const state = {scheme:'c1',page:'watch',query:'',filter:'all',team:'all',viewState:'normal',selected:1,rankTab:'野榜',rankSelected:1,settingsTab:'general',joined:new Set([1,2,4,5]),muted:false,danmaku:true,layout:'主画面 + 侧列',immersive:false,toolsCollapsed:false,preference:null,toastTimer:null};
const preferenceKey='cstg-visual-review-r2-preference';
const paths = {
  search:'<circle cx="10.5" cy="10.5" r="6.5"/><path d="m16 16 4.5 4.5"/>',
  screens:'<rect x="3" y="4" width="18" height="13" rx="2"/><path d="M8 21h8M12 17v4M14 4v13M14 10h7"/>',
  settings:'<path d="m9 3-.8 2.4-2.5.5-2 3.4 1.5 2-.1 2.5-1.4 2 2 3.5 2.5.4L9 22h5l.9-2.3 2.5-.4 2-3.5-1.4-2-.1-2.5 1.5-2-2-3.4-2.5-.5L14 3Z"/><circle cx="11.5" cy="12.5" r="3"/>',
  grid:'<rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/><rect x="14" y="14" width="7" height="7" rx="1"/>',
  plus:'<path d="M12 5v14M5 12h14"/>',
  arrow:'<path d="M5 12h14m-5-5 5 5-5 5"/>',
  refresh:'<path d="M20 8a8 8 0 1 0 .1 8M20 3v5h-5"/>',
  sound:'<path d="M4 9v6h4l5 4V5L8 9H4m12-1a6 6 0 0 1 0 8m3-11a10 10 0 0 1 0 14"/>',
  mute:'<path d="M4 9v6h4l5 4V5L8 9H4m12 0 5 6m0-6-5 6"/>',
  chat:'<path d="M20 15a2 2 0 0 1-2 2H9l-5 4V5a2 2 0 0 1 2-2h12a2 2 0 0 1 2 2Z"/><path d="M8 8h8M8 12h5"/>',
  chevron:'<path d="m8 5 7 7-7 7"/>',
  archive:'<rect x="3" y="3" width="18" height="5" rx="1"/><path d="M5 8v13h14V8M9 12h6"/>',
  info:'<circle cx="12" cy="12" r="9"/><path d="M12 11v6M12 7v.1"/>',
  check:'<path d="m5 12 4 4L20 5"/>',
  close:'<path d="m6 6 12 12M6 18 18 6"/>',
  empty:'<path d="m3 8 4-5h10l4 5v12H3Z"/><path d="M3 12h5l2 3h4l2-3h5"/>',
  warning:'<path d="m12 3 10 18H2Z"/><path d="M12 9v5M12 17v.2"/>',
  pause:'<path d="M8 5v14M16 5v14"/>',
  expand:'<path d="M8 3H3v5m13-5h5v5M3 16v5h5m13-5v5h-5"/>',
  star:'<path d="m12 3 2.8 5.7 6.2.9-4.5 4.4 1.1 6.2-5.6-3-5.6 3 1.1-6.2L3 9.6l6.2-.9Z"/>',
  more:'<circle cx="5" cy="12" r="1"/><circle cx="12" cy="12" r="1"/><circle cx="19" cy="12" r="1"/>'
};
function icon(name){return `<svg viewBox="0 0 24 24" aria-hidden="true">${paths[name]||paths.info}</svg>`;}
function escapeText(value){return String(value).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));}
function avatar(member){return `<span class="member-avatar" data-tone="${member.tone}" aria-hidden="true">${member.name[0]}</span>`;}
function mappingUnknown(member){return member.room==='待确认'?'房间待确认':'状态未知';}
function liveStatus(member){const labels={online:'直播中',offline:'未开播',unknown:mappingUnknown(member)};return `<span class="status ${member.live}"><i class="dot"></i>${labels[member.live]}</span>`;}
function button(text,action,style='',symbol=''){return `<button class="button ${style}" data-action="${action}">${symbol?icon(symbol):''}${text}</button>`;}
function pageHeader(eyebrow,title,description,actions=''){return `<header class="page-header"><div class="page-heading"><div class="eyebrow">${eyebrow}</div><h1>${title}</h1><p>${description}</p></div><div class="page-actions">${actions}</div></header>`;}
function stateSelect(){return `<div class="state-control"><label for="view-state">状态演示</label><select id="view-state" class="select" aria-label="状态演示">${[['normal','正常'],['loading','加载中'],['empty','空数据'],['error','请求失败']].map(([v,l])=>`<option value="${v}" ${state.viewState===v?'selected':''}>${l}</option>`).join('')}</select></div>`;}
function searchField(label){return `<label class="search">${icon('search')}<input id="search-input" type="search" aria-label="${label}" placeholder="${label}" value="${escapeText(state.query)}" autocomplete="off"><kbd>/</kbd></label>`;}
function emptyState(kind){
  if(kind==='loading')return `<div class="state-box" role="status" aria-live="polite"><h2>正在获取${state.page==='ranking'?'档案':'主播'}资料</h2><p>加载状态演示，可通过“状态演示”切回正常。</p>${[1,2,3].map(()=>'<div class="skeleton-row" aria-hidden="true"><div class="skeleton-avatar"></div><div class="skeleton-lines"><div class="skeleton"></div><div class="skeleton"></div></div></div>').join('')}</div>`;
  const error=kind==='error';return `<div class="state-box" role="status">${icon(error?'warning':'empty')}<h2>${error?'暂时无法取得资料':'没有找到匹配的内容'}</h2><p>${error?'当前是请求失败的演示，不显示原始服务诊断。你可以重试或稍后再来。':'试试其他名称，或清除当前筛选。空数据不会被示例分数替代。'}</p>${button(error?'重试':'清除筛选',error?'retry':'clear','',error?'refresh':'close')}</div>`;
}
function joinButton(member){if(state.joined.has(member.id))return '<span class="button join-button joined">已加入</span>';return `<button class="button join-button" data-join="${member.id}">${member.room==='待确认'?'确认房间':'加入'}${icon('plus')}</button>`;}
function memberRow(m){return `<article class="member-row ${m.id===state.selected?'selected':''}" data-member="${m.id}">${avatar(m)}<div class="member-info"><div class="member-name"><button class="name-button" data-member-detail="${m.id}">${m.name}</button><span class="role">${m.role}</span></div><p>${m.room} · ${m.title}</p></div><span class="member-team">${m.team}</span>${liveStatus(m)}${joinButton(m)}</article>`;}
function memberDetail(m){return `<aside class="detail-panel"><div class="section-bar"><h2>主播资料</h2><span>占位示例</span></div><div class="detail-identity">${avatar(m)}<div><h2>${m.name}</h2>${liveStatus(m)}</div></div><dl class="detail-grid"><div><dt>所属队伍</dt><dd>${m.team}</dd></div><div><dt>角色</dt><dd>${m.role}</dd></div><div><dt>房间</dt><dd>${m.room}</dd></div><div><dt>工作区</dt><dd>${state.joined.has(m.id)?'已加入':'尚未加入'}</dd></div></dl>${state.joined.has(m.id)?button('前往直播工作区','watch','primary','screens'):`<button class="button primary" data-join="${m.id}">${icon('plus')}${m.room==='待确认'?'确认房间号':'加入工作区'}</button>`}<div class="detail-divider"></div><div class="section-bar"><h2>S1 赛季档案</h2><span>历史成绩</span></div><p class="note">${m.score===null?'尚无对应成绩。缺失值保留为空。':'查看综合评分与九项维度。赛季成绩不代表当前直播状态。'}</p>${button('查看榜单详情','rank-detail','','arrow')}<div class="detail-divider"></div><p class="note">姓名、房间和状态为占位示例。<br>正式界面使用当前公会与房间模型。</p></aside>`;}
function renderDirectory(){
  const filtered=members.filter(m=>(state.filter==='all'||(state.filter==='online'&&m.live==='online')||(state.filter==='joined'&&state.joined.has(m.id)))&&(state.team==='all'||m.team===state.team)&&(m.name.includes(state.query)||m.room.includes(state.query)));
  const rows=filtered.map(memberRow).join('');
  const list=state.viewState!=='normal'?emptyState(state.viewState):!filtered.length?emptyState('empty'):`<div class="member-list">${rows}</div><div class="list-foot"><span>${filtered.length} 位示例主播 · 姓名和房间均为占位</span><span>点击姓名查看资料</span></div>`;
  return pageHeader('CSTG / DIRECTORY','找到同伴，一起看直播','按队伍找到主播，将想看的房间放进同一个工作区。',button('管理队伍','teams','','settings')+button('添加房间','add','primary','plus'))+`<div class="content-wrap"><div class="toolbar">${searchField('搜索主播或房间')}<div class="segmented" role="group" aria-label="主播状态筛选">${[['all','全部'],['online','直播中'],['joined','已加入']].map(([v,l])=>`<button data-filter="${v}" aria-pressed="${state.filter===v}">${l}</button>`).join('')}</div><select id="team-filter" class="select" aria-label="队伍筛选">${[['all','全部队伍'],['一队','一队'],['二队','二队'],['未分队','未分队']].map(([v,l])=>`<option value="${v}" ${state.team===v?'selected':''}>${l}</option>`).join('')}</select><div class="toolbar-end">${stateSelect()}</div></div><div class="directory-layout"><section class="directory-results" aria-label="主播名单"><div class="section-bar"><h2>公会成员 <span>身份 · 房间 · 工作区</span></h2><span>状态为演示</span></div>${list}</section>${memberDetail(members.find(m=>m.id===state.selected))}</div></div>`;
}
function rankDetail(m){const names=['力量','体力','智力','技巧','工投','财力','心态','颜值','直播能力'];return `<aside class="detail-panel rank-detail"><div class="section-bar"><h2>选中对象</h2><span>占位示例</span></div><div class="detail-identity">${avatar(m)}<div><h2>${m.name}</h2><span class="note">${m.team} · 评级 ${m.grade}</span></div></div><span class="note">综合评分</span><div class="score-total">${m.score===null?'—':m.score.toFixed(2)} <small>示例值</small></div><div class="dimensions">${names.map((name,i)=>{const value=m.score===null?null:Math.max(2,Math.min(19,m.score+[.2,-1,.8,-.5,1.1,-.8,.5,.1,-.4][i]));return `<div class="dimension"><span>${name}</span><div class="dimension-meter" aria-hidden="true"><span style="--value:${value===null?0:value/20*100}%"></span></div><span>${value===null?'—':value.toFixed(1)}</span></div>`;}).join('')}</div><p class="note">维度名称来自当前档案结构；数值仅用于排版比较，条形以 20 为演示尺度。</p><div class="detail-divider"></div>${button('查看主播资料','directory-detail','','arrow')}</aside>`;}
function rankTable(){const list=members.filter(m=>m.name.includes(state.query)||m.team.includes(state.query)||m.room.includes(state.query));if(state.viewState!=='normal')return emptyState(state.viewState);if(!list.length)return emptyState('empty');return `<div class="rank-table" role="table" aria-label="占位赛季榜单"><div class="rank-head" role="row">${['名次','主播 / 房间','队伍','评级','综合评分','详情'].map(n=>`<span role="columnheader">${n}</span>`).join('')}</div>${list.map(m=>`<div class="rank-row ${m.id===state.rankSelected?'selected':''}" role="row"><span class="rank-num" role="cell">${String(m.id).padStart(2,'0')}</span><div class="rank-identity" role="cell">${avatar(m)}<div><button class="name-button" data-rank-detail="${m.id}">${m.name}</button><p>${m.room}</p></div></div><span role="cell">${m.team}</span><span role="cell">${m.grade}</span><div class="rank-score" role="cell">${m.score===null?'—':m.score.toFixed(2)}<small>示例</small><div class="score-track" aria-hidden="true"><span style="--value:${m.score===null?0:m.score/20*100}%"></span></div></div><button class="button subtle" data-rank-detail="${m.id}" aria-label="查看${m.name}的赛季详情">查看</button></div>`).join('')}</div>`;}
function events(){
  if(state.viewState!=='normal')return emptyState(state.viewState);
  const tab=state.rankTab;
  const eventNumbers=[1,2,3,4].filter(i=>`${tab} 场次 ${i} 第 ${String(i).padStart(2,'0')} 场`.includes(state.query.trim()));
  if(!eventNumbers.length)return emptyState('empty');
  return `<div class="event-list">${eventNumbers.map(i=>`<article class="event-row"><div class="event-index">第 ${String(i).padStart(2,'0')} 场<br>日期占位</div><div class="event-info"><h3>${tab} · 场次 ${i}</h3><p>赛程标题与结果在正式界面读取内置档案；此处为结构示例。</p></div><button class="button" data-event="${i}">查看详情 ${icon('chevron')}</button></article>`).join('')}</div><div class="archive-overview"><h3>${tab==='足球赛'?'赛程与足球专项榜单':'赛季总览'}</h3><p>${tab==='足球赛'?'比赛树、进球、助攻及 MVP 榜单会沿用现有档案数据；当前只演示页面组织。':'保留赛事详情、日期筛选、分页及总览阅读。这里没有虚构胜率、趋势或当前赛事状态。'}</p></div>`;
}
function renderRanking(){return pageHeader('CSTG / SEASON ARCHIVE','团播榜单','看清每个名次，也看见每位主播。',button('档案说明','archive-info','','info'))+`<div class="content-wrap"><div class="archive-banner">${icon('archive')}<span><strong>S1 历史档案</strong> · 与实时直播状态分开呈现</span><span>原档案采集 2026.10.27 · 下方为占位示例</span></div><div class="tabbar" role="group" aria-label="赛季档案类型">${['野榜','常规赛','总决赛','足球赛','定级赛'].map(t=>`<button data-tab="${t}" aria-pressed="${state.rankTab===t}">${t}</button>`).join('')}</div><div class="toolbar">${searchField(state.rankTab==='野榜'?'搜索主播、队伍或房间':'搜索赛事标题或场次')}<span class="note">${state.rankTab==='野榜'?'原始名次顺序 · 不支持表头排序':'历史档案 · 非实时数据'}</span><div class="toolbar-end">${stateSelect()}</div></div>${state.rankTab==='野榜'?`<div class="rank-layout"><section class="rank-main" aria-label="排名列表">${rankTable()}<p class="rank-footnote">姓名、评级和分数均为占位 · 缺失值显示“—” · 未添加投票数或趋势</p></section>${rankDetail(members.find(m=>m.id===state.rankSelected))}</div>`:state.rankTab==='定级赛'?`<div class="archive-overview"><h2>定级赛</h2><p>六场成绩、总评及参与者详情使用独立档案结构。以下为缺失成绩状态示例。</p></div>${emptyState(state.viewState==='normal'?'empty':state.viewState)}`:events()}</div>`;}
function scene(index){const colors=[['oklch(26% .04 192)','oklch(39% .055 183)','oklch(17% .03 222)'],['oklch(30% .04 278)','oklch(45% .045 300)','oklch(18% .03 267)'],['oklch(31% .035 73)','oklch(45% .04 87)','oklch(18% .025 61)'],['oklch(27% .04 230)','oklch(42% .05 213)','oklch(17% .03 242)']][index%4];return `<svg viewBox="0 0 800 450" preserveAspectRatio="xMidYMid meet" aria-hidden="true"><rect width="800" height="500" fill="${colors[0]}"/><path d="M0 0h800v280L620 195l-70 90-160-130-140 90L0 200Z" fill="${colors[1]}" opacity=".35"/><path d="M0 360 210 240l150 70 230-145L800 290v210H0Z" fill="${colors[2]}"/><path d="m-100 470 500-220 500 220M400 250v250M250 315l-80 185m380-185 80 185" stroke="${colors[1]}" stroke-width="1.5" opacity=".55"/><path d="M0 370h800M0 420h800M0 480h800" stroke="${colors[1]}" stroke-width="1.2" opacity=".45"/><rect x="355" y="170" width="90" height="58" rx="4" fill="${colors[2]}" stroke="${colors[1]}" stroke-width="1.5"/><path d="m390 185 22 14-22 14Z" fill="${colors[1]}"/></svg>`;}
function videoTile(m,i){return `<section class="video-tile ${i===0?'main-video':''}" tabindex="0" aria-label="${m.name}，画面示例。聚焦显示播放控件。"><div class="video-heading"><i class="dot"></i><b>${m.name}</b><small>${i===0?'主画面 · '+(state.muted?'全局静音':'声音焦点'):'画面 '+(i+1)}</small></div><div class="video-scene">${scene(i)}${i===0?`<div class="danmaku-example" ${state.danmaku?'':'hidden'}>一起看这一场 · 弹幕位置示例</div>`:''}<div class="scene-caption"><span>CSTG / WATCH TOGETHER</span><b>${m.title}</b><small>视频构图占位 · 非真实直播画面</small></div></div><div class="video-controls"><button data-action="playback" aria-label="暂停播放示例">${icon('pause')}</button><button data-action="mute" aria-label="${state.muted?'取消全局静音':'全局静音'}" aria-pressed="${state.muted}">${icon(state.muted?'mute':'sound')}</button><div class="video-volume" aria-hidden="true"></div><button class="control-right" data-action="quality">画质 · 自动</button><button data-action="room-menu" aria-label="房间更多操作">${icon('more')}</button><button data-action="fullscreen" aria-label="放大评审画面">${icon('expand')}</button></div></section>`;}
function drawerButton(label,action,symbol){return `<button id="watch-${action}" class="button" data-action="${action}" aria-haspopup="dialog" aria-controls="review-dialog" aria-expanded="false">${icon(symbol)}${label}</button>`;}
function renderWatch(){
  const allRooms=members.filter(m=>state.joined.has(m.id));
  const rooms=allRooms.slice(0,4);
  return `<section class="watch-page" aria-label="直播工作区：晚间一起看"><div class="watch-stage"><div class="video-grid" data-layout="${escapeText(state.layout)}">${rooms.map(videoTile).join('')}</div><span class="watch-watermark">四路构图示例 · 非直播</span><div class="watch-dock" role="group" aria-label="观看工具" data-collapsed="${state.toolsCollapsed}"><div id="watch-dock-main" class="dock-main">${drawerButton('房间 '+allRooms.length,'rooms','screens')}${drawerButton('布局','layout','grid')}${drawerButton('声音与弹幕','audio','sound')}${button('添加','add','','plus')}${button('预设','presets','','archive')}<button id="watch-immersive" class="button" data-action="immersive" aria-pressed="${state.immersive}">${icon('expand')}<span>${state.immersive?'退出沉浸':'沉浸'}</span></button></div><button id="watch-tools-toggle" class="button dock-toggle" data-action="toggle-tools" aria-expanded="${!state.toolsCollapsed}" aria-controls="watch-dock-main">${icon(state.toolsCollapsed?'settings':'close')}<span>${state.toolsCollapsed?'观看工具':'收起'}</span></button></div><button id="exit-immersive" class="button immersion-exit" data-action="immersive" ${state.immersive?'':'hidden'}>${icon('expand')}退出沉浸 <kbd>Esc</kbd></button></div></section>`;
}
function roomsPanel(){const rooms=members.filter(m=>state.joined.has(m.id));return `<div class="drawer-section-label">已加入房间 · ${rooms.length}</div>${rooms.map((m,i)=>`<div class="watch-room">${avatar(m)}<div><strong>${m.name}</strong><small>${state.muted?'全局静音':i===0?'声音焦点':'静音'} · ${m.team}${i>=4?' · 未展示构图':''}</small></div>${liveStatus(m)}</div>`).join('')}<p class="note" style="margin-top:18px">仅前 4 路展示构图，其余房间保留在名单。所有房间及状态均为演示，未建立播放连接。</p>`;}
function syncWatch(){
  document.querySelectorAll('.video-controls [data-action="mute"]').forEach(el=>{el.innerHTML=icon(state.muted?'mute':'sound');el.setAttribute('aria-label',state.muted?'取消全局静音':'全局静音');el.setAttribute('aria-pressed',String(state.muted));});
  const heading=document.querySelector('.main-video .video-heading small');if(heading)heading.textContent='主画面 · '+(state.muted?'全局静音':'声音焦点');
  const danmaku=document.querySelector('.danmaku-example');if(danmaku)danmaku.hidden=!state.danmaku;
  const grid=document.querySelector('.video-grid');if(grid)grid.dataset.layout=state.layout;
}
function setImmersive(enabled){
  state.immersive=enabled;document.body.classList.toggle('immersive',enabled);
  const button=document.getElementById('watch-immersive');if(button){button.setAttribute('aria-pressed',String(enabled));button.querySelector('span').textContent=enabled?'退出沉浸':'沉浸';}
  const exit=document.getElementById('exit-immersive');if(exit)exit.hidden=!enabled;
  if(enabled)exit?.focus({preventScroll:true});else if(button?.getClientRects().length)button.focus({preventScroll:true});else document.getElementById('watch-tools-toggle')?.focus({preventScroll:true});
  updateDrawerPosition();
}
function toggle(label,checked=true,audioKey=''){return `<label class="toggle"><input type="checkbox" aria-label="${label}" ${audioKey?`data-audio="${audioKey}"`:''} ${checked?'checked':''}><span></span></label>`;}
function settingRow(title,desc,control){return `<div class="setting-row"><div><h3>${title}</h3><p>${desc}</p></div>${control}</div>`;}
function settingsBody(){switch(state.settingsTab){case 'teams':return `<div class="settings-card"><h2>队伍管理</h2><p>组织主播队伍与成员关系。导入必须先预览，再确认应用。</p>${settingRow('本地队伍','队伍与房间分组、工作区预设分别管理。',button('管理队伍','teams'))}</div>`;case 'updates':return `<div class="settings-card"><h2>更新</h2><p>当前源码版本 0.2.20；网页不执行更新请求。</p>${settingRow('软件更新','真实更新结果由原生客户端提供。',button('检查更新','check-update'))}</div>`;case 'roles':return `<div class="settings-card"><h2>活动角色</h2><p>为已确认房间的成员分配活动角色。修改后显式保存。</p>${settingRow('示例活动','名称仅用于评审。','<input class="select" aria-label="活动名称" value="S1 团播">')}${settingRow('北屿','示例成员 · 已确认房间','<select class="select" aria-label="北屿活动角色"><option>队长</option><option>团长</option><option>队员</option></select>')}${settingRow('角色配置','演示保存反馈。',button('保存配置','save-settings','primary'))}</div>`;default:return `<div class="settings-card"><h2>窗口与后台</h2><p>让窗口的关闭方式符合你的观看习惯。</p>${settingRow('点击关闭按钮时','后台托管可继续音频、状态检查和通知。','<select class="select" id="close-behavior" aria-label="关闭按钮行为"><option>每次询问</option><option>进入后台</option><option>完全退出</option></select>')}${settingRow('关闭确认','查看后台与完全退出的区别。',button('预览关闭对话框','close-preview'))}</div><div class="settings-card"><h2>通知与工作区</h2><p>保留现有独立设置入口，本页展示一种新的归集方式。</p>${settingRow('直播通知','开播、下播、播放异常和恢复。',button('通知设置','notifications'))}${settingRow('工作区预设','保存当前房间和布局组合。',button('管理预设','presets'))}</div>`;}}
function renderSettings(){return pageHeader('CSTG / PREFERENCES','设置','保留熟悉的功能，让设置更容易找到。',`<span class="inline-label">操作均为本地演示</span>`)+`<div class="content-wrap"><div class="settings-layout"><nav class="settings-sections" aria-label="设置分类">${[['general','窗口与通知'],['teams','队伍管理'],['updates','软件更新'],['roles','活动角色']].map(([id,label])=>`<button data-settings="${id}" aria-pressed="${state.settingsTab===id}">${label}</button>`).join('')}</nav><section>${settingsBody()}</section></div></div>`;}
function renderComponents(){return pageHeader('CSTG / UI STATES','每一种状态，都有回应','比较同一套按钮、输入、反馈与异常状态在三种方案下的表现。',button('预览添加房间','add','','plus'))+`<div class="content-wrap"><div class="control-grid"><section class="component-block"><h2>01 / 操作与反馈</h2><div class="component-row">${button('加入工作区','demo-action','primary','plus')}${button('查看详情','demo-action')}${button('移除','remove-preview','danger')}<button class="button" disabled>已加入</button></div><p class="note">主操作只有一个明确强调；破坏性操作保持独立。</p><div class="state-preview">${icon('info')}<div>工作区设置尚未保存<p>本次会话继续生效，可保留输入后重试。</p>${button('重试保存','save-settings','subtle')}</div></div></section><section class="component-block"><h2>02 / 输入与筛选</h2><label class="search">${icon('search')}<input aria-label="组件搜索示例" placeholder="搜索主播或房间"></label><div class="component-row" style="margin-top:15px"><select class="select" aria-label="组件队伍选择"><option>全部队伍</option><option>一队</option><option>二队</option></select><span class="note">弹幕</span>${toggle('组件弹幕开关')}<span class="status online"><i class="dot"></i>直播中</span><span class="status unknown"><i class="dot"></i>待确认</span></div><p class="note">Tab 聚焦、Space 切换；状态同时使用文字与颜色。</p></section><section class="component-block"><h2>03 / 加载与缺失</h2><div class="skeleton-row" aria-label="加载骨架示例"><div class="skeleton-avatar"></div><div class="skeleton-lines"><div class="skeleton"></div><div class="skeleton"></div></div></div><div class="state-preview">${icon('empty')}<div>还没有收藏的房间<p>缺失成绩显示“—”，未知直播状态保留“待确认”。</p></div></div></section><section class="component-block"><h2>04 / 异常与恢复</h2><div class="state-preview error-preview">${icon('warning')}<div>暂时无法加载房间资料<p>不展示原始URL或异常堆栈。重试后保留当前选择。</p>${button('重试','demo-retry','subtle','refresh')}</div></div><div class="component-row" style="margin-top:15px">${button('关闭确认','close-preview')}${button('通知设置','notifications')}${button('队伍导入预览','teams')}</div></section></div></div>`;}
function focusKey(element){
  return JSON.stringify([element.tagName,element.id||'',Object.entries(element.dataset||{}),element.getAttribute('aria-label')||'']);
}
function captureFocus(container){
  const active=document.activeElement;
  if(!active||!container.contains(active))return null;
  const key=focusKey(active);
  const matches=Array.from(container.querySelectorAll('button,input,select,textarea,a[href],[tabindex]')).filter(el=>focusKey(el)===key);
  return {key,index:matches.indexOf(active),start:active.selectionStart,end:active.selectionEnd};
}
function restoreFocus(container,snapshot){
  if(!snapshot)return;
  const matches=Array.from(container.querySelectorAll('button,input,select,textarea,a[href],[tabindex]')).filter(el=>focusKey(el)===snapshot.key);
  const target=matches[snapshot.index]||matches[0]||container;
  target.focus({preventScroll:true});
  if(typeof snapshot.start==='number'&&target.setSelectionRange){try{target.setSelectionRange(snapshot.start,snapshot.end);}catch{/* Search fields may not expose selection ranges. */}}
}
function renderPage(){
  const content=document.getElementById('page-content');
  const focus=captureFocus(content);
  content.innerHTML=({directory:renderDirectory,ranking:renderRanking,watch:renderWatch,settings:renderSettings,components:renderComponents}[state.page])();
  document.querySelectorAll('[data-page]').forEach(el=>{el.classList.toggle('active',el.dataset.page===state.page);if(el.dataset.page===state.page)el.setAttribute('aria-current','page');else el.removeAttribute('aria-current');});
  document.getElementById('status-context').textContent=`${pages[state.page]} / 候选 ${state.scheme.toUpperCase()}`;
  document.getElementById('nav-room-count').textContent=state.joined.size;
  document.querySelector('.workspace-shortcut small').textContent=state.joined.size+' 个演示房间';
  if(!dialog.open)restoreFocus(content,focus);
}
function setPage(page){if(!pages[page])return;if(dialog.open)dialog.close();if(state.immersive)setImmersive(false);state.page=page;state.query='';state.viewState='normal';renderPage();const content=document.getElementById('page-content');content.scrollTop=0;content.focus({preventScroll:true});}
function setScheme(scheme){if(!schemes[scheme])return;state.scheme=scheme;document.body.dataset.palette=scheme;document.querySelectorAll('.scheme-choice').forEach(el=>el.setAttribute('aria-pressed',String(el.dataset.palette===scheme)));document.getElementById('record-choice').innerHTML=`记录偏好 ${scheme.toUpperCase()} <span aria-hidden="true">↗</span>`;document.getElementById('scheme-description').textContent=schemes[scheme].description;document.getElementById('status-context').textContent=`${pages[state.page]} / 候选 ${scheme.toUpperCase()}`;}
function toast(message){
  const outer=document.getElementById('toast');
  clearTimeout(state.toastTimer);
  const box=dialog.open?document.getElementById('dialog-feedback'):outer;
  if(dialog.open)outer.hidden=true;
  box.textContent=message;box.hidden=false;
  if(!dialog.open)state.toastTimer=setTimeout(()=>box.hidden=true,4500);
}
const dialog=document.getElementById('review-dialog');
let dialogReturnFocus=null;
let dialogReturnSnapshot=null;
let searchComposing=false;
const focusableSelector='button:not([disabled]),input:not([disabled]):not([type=hidden]),select:not([disabled]),textarea:not([disabled]),a[href],[tabindex]:not([tabindex="-1"])';
function dialogFocusables(){return Array.from(dialog.querySelectorAll(focusableSelector)).filter(el=>el.getClientRects().length&&getComputedStyle(el).visibility!=='hidden');}
function updateDrawerPosition(){
  const stage=document.querySelector('.watch-stage');if(!stage)return;
  const rect=stage.getBoundingClientRect();
  dialog.style.setProperty('--drawer-top',Math.max(8,rect.top+4)+'px');
  dialog.style.setProperty('--drawer-bottom',Math.max(8,window.innerHeight-rect.bottom+4)+'px');
  dialog.style.setProperty('--drawer-right',Math.max(8,window.innerWidth-rect.right+4)+'px');
}
function openDialog(title,subtitle,body,actions='',presentation='modal'){
  if(!dialog.open){dialogReturnFocus=document.activeElement;dialogReturnSnapshot=captureFocus(document.getElementById('page-content'));}
  dialog.dataset.presentation=presentation;
  if(dialogReturnFocus?.matches('[aria-haspopup="dialog"]'))dialogReturnFocus.setAttribute('aria-expanded','true');
  updateDrawerPosition();
  document.getElementById('dialog-content').innerHTML=`<header class="dialog-header"><div><h2 id="dialog-title">${title}</h2><p>${subtitle}</p></div><button class="icon-button" data-action="close-dialog" aria-label="关闭对话框">${icon('close')}</button></header><div class="dialog-body">${body}<p id="dialog-feedback" class="note dialog-feedback" role="status" aria-live="polite" hidden></p></div><footer class="dialog-actions">${actions||button('关闭','close-dialog')}</footer>`;
  document.getElementById('toast').hidden=true;
  if(!dialog.open)dialog.showModal();
  const controls=dialogFocusables();
  const initial=controls.find(el=>el.matches('input,textarea,select'))||controls[0];
  if(initial)initial.focus({preventScroll:true});
}
dialog.addEventListener('close',()=>{
  if(dialog.open)return;
  document.querySelectorAll('[aria-controls="review-dialog"]').forEach(el=>el.setAttribute('aria-expanded','false'));
  if(document.activeElement===document.body||dialog.contains(document.activeElement)){
    if(dialogReturnFocus?.isConnected)dialogReturnFocus.focus({preventScroll:true});
    else restoreFocus(document.getElementById('page-content'),dialogReturnSnapshot);
  }
});
function savePreference(){const note=document.getElementById('preference-note').value;const preference={revision:'r2',scheme:state.scheme,components:'C',layout:state.layout,note,date:new Date().toISOString()};try{localStorage.setItem(preferenceKey,JSON.stringify(preference));state.preference=preference;updatePreferenceLabel();dialog.close();toast(`已记录偏好 ${state.scheme.toUpperCase()}。请在聊天中明确确认后再继续设计。`);}catch{toast('浏览器未允许保存。请在聊天中告诉我偏好的方案。');}}
function updatePreferenceLabel(){document.getElementById('saved-preference').textContent=state.preference?`本地偏好 ${state.preference.scheme.toUpperCase()} · 等待聊天确认`:'尚未记录偏好';}
function openPreference(){const s=schemes[state.scheme];openDialog('记录你的视觉偏好','R2 · C 组件。只保存到当前浏览器。',`<div class="preference-summary"><b>${state.scheme.toUpperCase()}</b><div><strong>${s.name}</strong><p>${s.color} · ${state.layout}</p></div></div><label for="preference-note">想保留或调整的地方（选填）</label><textarea id="preference-note" placeholder="例如：选 C2，房间抽屉保留，希望默认收起观看工具。">${escapeText(state.preference?.note||'')}</textarea><p style="margin-top:13px">这里记录的是偏好，不是正式实施授权。请回到聊天明确选择方案；确认前不创建正式 Figma 设计或修改生产 QML。</p>`,button('暂不记录','close-dialog')+button('保存偏好','save-preference','primary'));}
function detailModal(m,type){openDialog(`${m.name} · ${type==='rank'?'赛季详情':'主播资料'}`,'此处为占位数据；仅用于评审布局。',type==='rank'?rankDetail(m):memberDetail(m));}
function act(action){
  switch(action){
    case 'close-dialog':dialog.close();break;
    case 'rooms':openDialog('房间','覆盖展开，关上后继续观看。',roomsPanel(),button('添加房间','add','primary','plus')+button('收起','close-dialog'),'drawer');break;
    case 'immersive':setImmersive(!state.immersive);break;
    case 'toggle-tools':{state.toolsCollapsed=!state.toolsCollapsed;const dock=document.querySelector('.watch-dock');dock.dataset.collapsed=String(state.toolsCollapsed);const trigger=document.getElementById('watch-tools-toggle');trigger.setAttribute('aria-expanded',String(!state.toolsCollapsed));trigger.innerHTML=icon(state.toolsCollapsed?'settings':'close')+`<span>${state.toolsCollapsed?'观看工具':'收起'}</span>`;break;}
    case 'save-preference':savePreference();break;
    case 'watch':setPage('watch');break;
    case 'clear':state.query='';state.filter='all';state.team='all';state.viewState='normal';renderPage();break;
    case 'retry':state.viewState='normal';renderPage();toast('已恢复正常示例；未请求真实服务。');break;
    case 'rank-detail':state.rankSelected=state.selected;state.rankTab='野榜';setPage('ranking');break;
    case 'directory-detail':state.selected=state.rankSelected;setPage('directory');break;
    case 'add':openDialog('添加房间','房间号、直播间链接或主播名。评审中不发送查询。','<label for="add-query">查找房间</label><input id="add-query" type="text" placeholder="输入示例主播名，如 北屿"><p style="margin-top:12px">输入任意非空内容，可查看示例候选。</p>',button('取消','close-dialog')+button('搜索示例','search-room','primary','search'));break;
    case 'search-room':{const value=document.getElementById('add-query').value.trim();if(!value){toast('请先输入主播名或房间号。');document.getElementById('add-query').focus();break;}openDialog('选择房间','以下为固定占位结果，与输入无真实查询关系。',members.slice(6,8).map(memberRow).join(''));break;}
    case 'archive-info':openDialog('关于 S1 历史档案','静态档案与实时房间状态来自不同数据源。','<p>当前源码优先读取内置 S1 档案，原始采集日期为 2026-10-27。野榜使用排名、姓名、房间、队伍、评级、综合评分和维度字段。</p><p style="margin-top:12px">原型姓名和数值都是占位；不包含真实投票数、趋势或直播状态。当前实现没有表头排序，因此没有添加可点击排序表头。</p>');break;
    case 'teams':openDialog('管理队伍','队伍配置独立于房间分组及工作区预设。',`<div class="section-bar"><h3>本地队伍</h3><span>演示</span></div>${['一队','二队'].map((t,i)=>`<div class="setting-row"><div><h3>${t}</h3><p>4 位示例成员 · ${i?'阿岚':'北屿'}担任队长</p></div>${button('编辑','edit-team')}</div>`).join('')}<p style="margin-top:18px">导入会先展示匹配、冲突与保留项，确认后才应用。</p>`,button('关闭','close-dialog')+button('预览导入','import-preview','primary'));break;
    case 'edit-team':openDialog('编辑队伍','占位编辑，不修改原生队伍配置。','<label for="team-name">队伍名称</label><input id="team-name" type="text" value="一队"><p style="margin-top:12px">正式界面保留成员分配、移除、重命名与排序。</p>',button('取消','close-dialog')+button('保存示例','save-settings','primary'));break;
    case 'import-preview':openDialog('确认导入预览','示例差异；尚未应用到任何队伍。','<div class="state-preview">'+icon('info')+'<div>1 个待新增队伍 · 1 个保留队伍<p>这些数量是演示值，不代表真实导入结果。</p></div></div><p style="margin-top:16px">正式导入校验来源是否变化，保存失败回滚，冲突成员需明确处理。</p>',button('取消','close-dialog')+button('确认演示','save-settings','primary'));break;
    case 'presets':openDialog('工作区预设','房间与布局的组合，和本地队伍分开保存。','<label for="preset-name">预设名称</label><input id="preset-name" type="text" maxlength="30" value="晚间一起看"><div class="setting-row"><div><h3>晚间一起看</h3><p>4 个演示房间</p></div>'+button('应用示例','apply-preset')+'</div>',button('关闭','close-dialog')+button('保存当前预设','save-settings','primary'));break;
    case 'apply-preset':dialog.close();setPage('watch');toast('已切换到示例工作区。');break;
    case 'audio':openDialog('音频与弹幕','覆盖展开 · 控件仅作演示',settingRow('全局静音','关闭所有房间声音。',toggle('全局静音',state.muted,'muted'))+settingRow('声音模式','单路焦点或多音轨。','<select class="select" aria-label="声音模式"><option>单路焦点</option><option>多音轨</option></select>')+settingRow('显示弹幕','正式界面还保留逐房设置、治理和统计。',toggle('显示弹幕',state.danmaku,'danmaku')),button('完成','close-dialog','primary'),'drawer');break;
    case 'notifications':openDialog('通知设置','以下为视觉演示，不写入系统设置。',settingRow('允许通知','启用后可设置各类提醒。',toggle('允许通知'))+['开播','下播','播放异常','播放恢复','收藏标题变化'].map(t=>settingRow(t+'提醒','',toggle(t+'提醒',t!=='下播'))).join(''),button('完成','close-dialog','primary'));break;
    case 'close-preview':openDialog('准备结束观看？','请选择本次关闭方式。',`<p>进入后台后，音频、状态检查和通知可能继续。完全退出将关闭房间、播放器和服务。</p><div class="component-row" style="margin-top:18px"><label><input type="checkbox"> 记住我的选择</label></div>`,button('取消','close-dialog')+button('进入后台','demo-close')+button('完全退出','demo-close','primary'));break;
    case 'demo-close':dialog.close();toast('关闭方式已演示，评审网页保持打开。');break;
    case 'demo-remove':dialog.close();toast('移除确认已演示；未移除真实房间。');break;
    case 'save-settings':dialog.close();toast('已显示保存成功反馈；没有修改原生配置。');break;
    case 'check-update':toast('更新检查仅作入口演示，未发送网络请求。');break;
    case 'demo-action':toast('操作反馈示例。');break;
    case 'demo-retry':toast('重试反馈示例；没有真实服务请求。');break;
    case 'remove-preview':openDialog('移除房间？','移除与关闭整个应用是不同的操作。','<p>此处只预览确认样式，不移除真实房间。</p>',button('取消','close-dialog')+button('移除示例','demo-remove','danger'));break;
    case 'mute':state.muted=!state.muted;syncWatch();break;
    case 'danmaku':state.danmaku=!state.danmaku;syncWatch();break;
    case 'layout':openDialog('观看布局','三种配色共用同一布局；切配色不会重置。',`<p>画面优先：主画面与三路预览并排，工具不占网格空间。也可比较等分或底列构图。这里仅演示四路，正式应用保留现有自动、主画面及双主画面能力。</p><div class="layout-options">${['主画面 + 侧列','等分网格','主画面 + 底列'].map(l=>`<button class="button" data-layout-choice="${l}" aria-pressed="${state.layout===l}">${icon('grid')}${l}</button>`).join('')}</div><p>视频按 16:9 完整显示，比例不合时保留黑边，不裁切内容。</p>`,button('收起','close-dialog'),'drawer');break;
    case 'quality':openDialog('播放画质','正式界面只展示服务实际返回的可用画质。','<p>当前网页没有播放源，因此不虚构可用清晰度。保留“自动”作为控件外观示例。</p>');break;
    case 'room-menu':openDialog('房间操作','演示公共菜单样式。',`<div class="review-comparison">${button('设为主画面','demo-action')}${button('重新检查播放源','demo-retry')}${button('刷新房间资料','demo-retry')}${button('移除房间','remove-preview','danger')}</div>`);break;
    case 'playback':toast('播放器控件示例；没有真实播放会话。');break;
    case 'fullscreen':openDialog('主画面预览','构图占位，不是真实播放器全屏。',`<div style="height:260px;position:relative;overflow:hidden;background:var(--bg)">${scene(0)}</div>`);break;
  }
}
document.addEventListener('click',event=>{
  const target=event.target.closest('button');if(!target)return;
  if(target.dataset.palette){setScheme(target.dataset.palette);return;}
  if(target.dataset.page){setPage(target.dataset.page);return;}
  if(target.dataset.action){act(target.dataset.action);return;}
  if(target.dataset.filter){state.filter=target.dataset.filter;renderPage();return;}
  if(target.dataset.tab){state.rankTab=target.dataset.tab;state.query='';renderPage();return;}
  if(target.dataset.settings){state.settingsTab=target.dataset.settings;renderPage();return;}
  if(target.dataset.memberDetail){state.selected=Number(target.dataset.memberDetail);if(dialog.open||window.innerWidth<=1100)detailModal(members.find(m=>m.id===state.selected),'member');else renderPage();return;}
  if(target.dataset.rankDetail){state.rankSelected=Number(target.dataset.rankDetail);if(dialog.open||window.innerWidth<=1100)detailModal(members.find(m=>m.id===state.rankSelected),'rank');else renderPage();return;}
  if(target.dataset.join){const member=members.find(m=>m.id===Number(target.dataset.join));if(member.room==='待确认'){openDialog('确认房间号','该主播尚未有已确认的房间映射。','<p>正式客户端需要确认房间号后才能加入。网页不会用随机号码替代真实映射。</p>');return;}state.joined.add(member.id);if(dialog.open)dialog.close();renderPage();toast(`${member.name}已加入演示工作区；未开启真实播放。`);return;}
  if(target.dataset.event){openDialog(`${state.rankTab} · 场次 ${target.dataset.event}`,'赛事详情排版示例。','<h3>赛事记录</h3><p style="margin-top:12px">这里保留日期、标题、详情、备注与总览的阅读区域。正式实现读取现有档案文本，不从缺失字段推算胜率或动态状态。</p>');return;}
  if(target.dataset.layoutChoice){state.layout=target.dataset.layoutChoice;syncWatch();dialog.close();return;}
});
document.addEventListener('compositionstart',event=>{if(event.target.id==='search-input')searchComposing=true;});
document.addEventListener('compositionend',event=>{if(event.target.id==='search-input'){searchComposing=false;state.query=event.target.value;renderPage();}});
document.addEventListener('input',event=>{if(event.target.id==='search-input'){state.query=event.target.value;if(!event.isComposing&&!searchComposing)renderPage();}});
document.addEventListener('change',event=>{
  if(event.target.id==='team-filter'){state.team=event.target.value;renderPage();}
  if(event.target.id==='view-state'){state.viewState=event.target.value;renderPage();}
  if(event.target.id==='close-behavior')toast('关闭偏好变更反馈示例；没有修改原生设置。');
  const audioKey=event.target.dataset.audio;
  if(audioKey==='muted'||audioKey==='danmaku'){state[audioKey]=event.target.checked;syncWatch();toast('已更新音频与弹幕的演示状态。');}
});
document.addEventListener('keydown',event=>{
  if(event.key==='Escape'){
    if(dialog.open)return;
    if(state.immersive){event.preventDefault();setImmersive(false);return;}
  }
  if(dialog.open&&event.key==='Tab'){
    const controls=dialogFocusables();
    if(!controls.length)return;
    event.preventDefault();
    const current=controls.indexOf(document.activeElement);
    const next=current<0?(event.shiftKey?controls.length-1:0):(current+(event.shiftKey?-1:1)+controls.length)%controls.length;
    controls[next].focus({preventScroll:true});
    return;
  }
  if(event.key==='/'&&!dialog.open&&!event.target.matches('input,textarea,select')){const input=document.getElementById('search-input');if(input){event.preventDefault();input.focus();}}
});
document.getElementById('record-choice').addEventListener('click',openPreference);
document.getElementById('review-notes').addEventListener('click',()=>openDialog('C 组件，画面优先','本轮固定组件结构，只比较绿色配色。',`<p>常驻房间侧栏和双层标题工具区已经收起。房间、布局、声音使用覆盖式抽屉；添加和预设使用弹窗。画面控件在悬停或键盘聚焦时出现；底部工具可收起。沉浸预览支持 Esc 返回。</p><div class="review-comparison" style="margin-top:20px">${Object.entries(schemes).map(([id,s])=>`<section><h3>${id.toUpperCase()} / ${s.name}</h3><p>${s.detail}</p></section>`).join('')}</div><p style="margin-top:20px">全部配色仍待确认。所有数据与画面为占位，网页不替代原生功能测试。</p>`));
function updateSize(){document.getElementById('window-size').textContent=`${window.innerWidth} × ${window.innerHeight}`;}
window.addEventListener('resize',()=>{updateSize();updateDrawerPosition();});
document.addEventListener('pointerdown',event=>{const tile=event.target.closest('.video-tile');if(tile&&!event.target.closest('button'))tile.focus({preventScroll:true});});
document.querySelectorAll('[data-icon]').forEach(el=>el.innerHTML=icon(el.dataset.icon));
try{const saved=JSON.parse(localStorage.getItem(preferenceKey));if(saved?.revision==='r2'&&schemes[saved.scheme]&&typeof saved.note==='string')state.preference=saved;}catch{/* Persistence is optional; the review still works. */}
updateSize();updatePreferenceLabel();renderPage();setScheme(state.scheme);
