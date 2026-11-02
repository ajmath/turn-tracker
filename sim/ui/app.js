'use strict';
const $ = id => document.getElementById(id);
const names = ['Discovering','Unassigned','Enrollment','Selected','Finishing setup','Waiting for claim','Playing','Paused','Enrollment conflict','Storage error'];
let initialized=false;
let state, running = true, chain = Promise.resolve(), pending = false, importing = false;
let replay = [], trace = [], samples = [], eventLimit = 5000, replayComplete = false;
const pressed = new Set();
function notify(text, error=false) { $('notice').textContent=text; $('notice').classList.toggle('error',error); }
function record(command) {
  if (command==='state') return;
  if (command.startsWith('reset ')) { replay=[]; replayComplete=true; trace=[]; samples=[]; }
  const last = replay.at(-1);
  if (command.startsWith('advance ') && last?.startsWith('advance ') && Number(last.split(' ')[1])+Number(command.split(' ')[1])<=10000)
    replay[replay.length-1]='advance '+(Number(last.split(' ')[1])+Number(command.split(' ')[1]));
  else if (replay.length<10000) replay.push(command);
  else replayComplete=false;
}
function command(text, options={}) {
  const task = chain.then(async () => {
    const response=await fetch('/api/command',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({command:text})});
    const data=await response.json();
    if (!response.ok || data.error) throw new Error(data.error || 'Simulator request failed');
    if(!initialized){
      initialized=true;$('count').value=data.pucks.length;$('seed').value=data.seed;
      if(text==='state'&&data.time===0){replay=[`reset ${data.pucks.length} ${data.seed}`];replayComplete=true;}
    }
    record(text); state=data;
    if (data.trace) { trace.push(...data.trace.trimEnd().split('\n')); if(trace.length>eventLimit)trace=trace.slice(-eventLimit); }
    if (text.startsWith('advance ')) { samples.push({time:data.time,leds:data.pucks.map(p=>p.led)}); if(samples.length>300)samples.shift(); }
    render();
    if (!options.quiet) notify(running?'Running · use the puck buttons or gesture shortcuts.':'Paused · queue a gesture, then step time or press Run.');
    return data;
  });
  chain=task.catch(error=>{running=false;renderRun();notify(error.message,true);});
  return task;
}
function act(text) { if(importing)return;command(text).catch(()=>{}); }
function renderRun() { $('play').textContent=running?'Pause':'Run'; $('play').setAttribute('aria-pressed',String(running)); }
function buildCards(count) {
  $('pucks').innerHTML=''; $('history').innerHTML='';$('lights').replaceChildren();
  for(let i=0;i<count;i++) {
    const card=document.createElement('article'); card.className='puck-card'; card.id='puck-'+i;
    card.innerHTML=`<header><h3>Puck ${i+1}</h3><span class="badge" data-role></span></header><button class="puck-button" data-puck="${i}" aria-label="Press puck ${i+1}" aria-pressed="false">${i+1}</button><div class="puck-status"></div><div class="actions"><button data-command="gesture ${i} tap">Tap</button><button data-command="gesture ${i} hold">Hold 3s</button><button data-command="gesture ${i} enroll">Start enrollment</button><button data-command="restart ${i}" data-restart>Restart</button></div><div class="facts"></div><div class="toggles"><label><input type="checkbox" data-power="${i}"> Power</label><label><input type="checkbox" data-isolate="${i}"> Isolate</label><label><input type="checkbox" data-storage="${i}"> Fail writes</label></div>`;
    $('pucks').append(card);
    const light=document.createElement('span');light.className='overview-light';light.textContent=i+1;light.setAttribute('role','img');$('lights').append(light);
    const row=document.createElement('div'); row.className='history-row'; row.innerHTML=`<span>${i+1}</span><canvas width="600" height="24" aria-label="Puck ${i+1} LED timeline"></canvas>`; $('history').append(row);
  }
  const table=document.createElement('table');table.className='links-table';
  let html='<tr><th>→</th>'+Array.from({length:count},(_,i)=>`<th>${i+1}</th>`).join('')+'</tr>';
  for(let i=0;i<count;i++)html+=`<tr><th>${i+1}</th>`+Array.from({length:count},(_,j)=>`<td><button class="link${i===j?' self':''}" data-from="${i}" data-to="${j}" ${i===j?'disabled':''} aria-label="Link puck ${i+1} to puck ${j+1}">${i===j?'—':'●'}</button></td>`).join('')+'</tr>';
  table.innerHTML=html; $('links').replaceChildren(table);
}
function orderText(order) { return order.length?order.map(i=>i<0?'?':String(i+1)).join(' → '):'—'; }
function render() {
  if(!state)return;
  if($('pucks').children.length!==state.pucks.length)buildCards(state.pucks.length);
  const seconds=state.time/1000;$('clock').textContent=`${String(Math.floor(seconds/60)).padStart(2,'0')}:${(seconds%60).toFixed(2).padStart(5,'0')}`;
  $('network').textContent=`${state.queued} in flight · ${state.dropped} dropped`;
  $('faults').checked=state.faults;
  for(const p of state.pucks) {
    const card=$('puck-'+p.id), button=card.querySelector('.puck-button');
    const light=$('lights').children[p.id];light.classList.toggle('lit',p.led);light.setAttribute('aria-label',`Puck ${p.id+1} light ${p.led?'on':'off'}`);light.title=`Puck ${p.id+1}: ${p.powered?names[p.status]:'powered off'}`;
    card.classList.toggle('off',!p.powered);button.classList.toggle('lit',p.led);button.classList.toggle('pressed',p.down);button.disabled=!p.powered;button.setAttribute('aria-pressed',String(p.down));
    const role=card.querySelector('[data-role]');role.textContent=p.coordinator===p.id?'Coordinator':p.selected.length||p.enrollment!=='0'?'Setup':p.order.length?'Participant':'Unassigned';
    const status=card.querySelector('.puck-status');status.textContent=!p.powered?'Powered off':p.down?'Button held · '+names[p.status]:names[p.status];status.classList.toggle('error',p.status>=8);
    const facts=card.querySelector('.facts');facts.replaceChildren();
    const lines=[`Order  ${orderText(p.order)}`,`Selected  ${orderText(p.selected)}`,`Membership  ${p.membership}`,`Enrollment  ${p.enrollment}`,`Session ${p.session} · rev ${p.revision}`,`Active position ${p.active===255?'unknown':p.active+1} · writes ${p.writes} · retries ${p.retries}`];
    for(const line of lines){const el=document.createElement('div');el.textContent=line;facts.append(el);}
    card.querySelector('[data-power]').checked=p.powered;card.querySelector('[data-storage]').checked=p.storageFault;
    card.querySelector('[data-isolate]').checked=state.links[p.id].every((value,j)=>j===p.id||value&&state.links[j][p.id]);
    for(const action of card.querySelectorAll('[data-command]'))action.disabled=!p.powered;
  }
  for(const button of $('links').querySelectorAll('[data-from]')){
    const from=Number(button.dataset.from),to=Number(button.dataset.to);if(from===to)continue;
    const blocked=state.links[from][to];button.classList.toggle('blocked',blocked);button.textContent=blocked?'×':'●';button.setAttribute('aria-pressed',String(blocked));
  }
  const canvases=$('history').querySelectorAll('canvas');
  canvases.forEach((canvas,i)=>{const ctx=canvas.getContext('2d');ctx.clearRect(0,0,600,24);samples.forEach((sample,j)=>{ctx.fillStyle=sample.leds[i]?'#f5ca67':'#26353f';ctx.fillRect(j*2,1,2,22);});});
  const showPackets=$('packets').checked;
  $('events').textContent=trace.filter(line=>showPackets||!/^(rx|drop|table) /.test(line)).slice(-200).join('\n');$('events').scrollTop=$('events').scrollHeight;
  renderRun();
}
$('pucks').addEventListener('click',event=>{const button=event.target.closest('[data-command]');if(button)act(button.dataset.command);});
$('pucks').addEventListener('change',event=>{
  const el=event.target;
  for(const [key,verb] of [['power','power'],['isolate','isolate'],['storage','storage']])if(el.dataset[key]!==undefined)act(`${verb} ${el.dataset[key]} ${el.checked?1:0}`);
});
function press(button) { if(importing)return;const id=Number(button.dataset.puck);if(pressed.has(id)||button.disabled)return;pressed.add(id);act(`button ${id} 1`); }
function release(button) { const id=Number(button.dataset.puck);if(!pressed.delete(id))return;act(`button ${id} 0`); }
$('pucks').addEventListener('pointerdown',event=>{const button=event.target.closest('[data-puck]');if(!button||event.button!==0)return;event.preventDefault();button.focus();button.setPointerCapture(event.pointerId);press(button);});
for(const type of ['pointerup','pointercancel','lostpointercapture'])$('pucks').addEventListener(type,event=>{const button=event.target.closest('[data-puck]');if(button)release(button);});
$('pucks').addEventListener('keydown',event=>{const button=event.target.closest('[data-puck]');if(button&&[' ','Enter'].includes(event.key)){event.preventDefault();if(!event.repeat)press(button);}});
$('pucks').addEventListener('keyup',event=>{const button=event.target.closest('[data-puck]');if(button&&[' ','Enter'].includes(event.key)){event.preventDefault();release(button);}});
window.addEventListener('blur',()=>{for(const id of [...pressed])release(document.querySelector(`[data-puck="${id}"]`));});
$('links').addEventListener('click',event=>{const button=event.target.closest('[data-from]');if(!button||button.disabled)return;const from=Number(button.dataset.from),to=Number(button.dataset.to);act(`link ${from} ${to} ${state.links[from][to]?0:1}`);});
$('play').onclick=()=>{if(importing)return;running=!running;renderRun();notify(running?'Running':'Paused · queued gestures advance only when you step time.');};
$('step').onclick=()=>{running=false;act('advance 100');};$('second').onclick=()=>{running=false;act('advance 1000');};
$('faults').onchange=()=>act('faults '+($('faults').checked?1:0));$('packets').onchange=render;
$('reset').onclick=()=>{if(importing)return;const seed=Number($('seed').value);if(!Number.isInteger(seed)||seed<0||seed>4294967295){notify('Seed must be a whole number from 0 to 4294967295.',true);return;}pressed.clear();act(`reset ${$('count').value} ${seed}`);};
function download(name,content,type) {const url=URL.createObjectURL(new Blob([content],{type}));const anchor=document.createElement('a');anchor.href=url;anchor.download=name;anchor.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
$('export').onclick=async()=>{if(importing)return;running=false;await chain;if(!replayComplete){notify('Start a new table to record a complete replay (or load a saved replay). Existing sessions after page reload cannot be exported as complete replays.',true);return;}download(`turn-table-${state.seed}.json`,JSON.stringify({version:1,commands:replay},null,2),'application/json');renderRun();};
$('load').onclick=()=>{if(!importing)$('import').click();};
$('trace').onclick=()=>download('turn-table.trace',trace.join('\n')+'\n','text/plain');
$('import').onchange=async event=>{
  const file=event.target.files[0];if(!file)return;running=false;importing=true;renderRun();
  try{
    if(file.size>1000000)throw new Error('Replay is too large');
    const data=JSON.parse(await file.text());
    if(data.version!==1||!Array.isArray(data.commands)||!data.commands.length||data.commands.length>10000||!data.commands[0].startsWith('reset '))throw new Error('Expected a version-1 table replay beginning with reset');
    let duration=0;for(const command of data.commands){if(typeof command!=='string'||command.length>100||/[\r\n]/.test(command))throw new Error('Invalid replay command');if(command.startsWith('advance '))duration+=Number(command.split(' ')[1]);}
    if(!Number.isFinite(duration)||duration>3600000)throw new Error('Replay exceeds one simulated hour');
    for(let i=0;i<data.commands.length;i++){await command(data.commands[i],{quiet:true});if(i%20===0)notify(`Replaying ${i+1} / ${data.commands.length}`);}
    $('count').value=state.pucks.length;$('seed').value=state.seed;notify('Replay complete · paused at its final state.');
  }catch(error){notify(error.message,true);}finally{importing=false;event.target.value='';}
};
setInterval(async()=>{if(!running||pending||importing)return;pending=true;try{await command('advance '+50*Number($('speed').value),{quiet:true});}catch{}finally{pending=false;}},50);
act('state');
