(function(){
  const N=24, reduit=matchMedia('(prefers-reduced-motion: reduce)').matches;
  function matrice(id, etape){
    const cv=document.getElementById(id); if(!cv) return; const c=cv.getContext('2d'), s=cv.width/N;
    const g=Array.from({length:N},()=>Array(N).fill(null));
    const set=(x,y,col)=>{ if(x>=0&&x<N&&y>=0&&y<N) g[y][x]=col; };
    let t=0;
    function dessine(){
      g.forEach(r=>r.fill(null)); etape(t++,set,g);
      c.fillStyle='#0c0d14'; c.fillRect(0,0,cv.width,cv.height);
      for(let y=0;y<N;y++)for(let x=0;x<N;x++){
        c.fillStyle=g[y][x]||'#1b1d2b';
        c.beginPath(); c.arc(x*s+s/2,y*s+s/2,s*(g[y][x]?.4:.17),0,7); c.fill();
      }
    }
    dessine();
    if(!reduit) setInterval(dessine,140);
  }

  // Snake : boucle autour d'un rectangle, pomme au centre
  const boucle=[]; for(let x=4;x<=19;x++)boucle.push([x,5]); for(let y=6;y<=17;y++)boucle.push([19,y]);
  for(let x=18;x>=4;x--)boucle.push([x,17]); for(let y=16;y>=6;y--)boucle.push([4,y]);
  matrice('g-snake',(t,set)=>{
    for(let i=0;i<9;i++){const p=boucle[(t-i+boucle.length*9)%boucle.length]; set(p[0],p[1],i?'#34d27a':'#a7ffcf');}
    set(12,11,t%6<3?'#ff4d5e':'#c23646');
  });

  // Briques : blocs 2x2 qui tombent dans 12 colonnes et s'empilent ; la pile repart à zéro
  const cols=['#f7b731','#ff4d5e','#4db8ff','#34d27a','#b56bff'];
  let pile=[], piece=null, haut=Array(12).fill(0);
  matrice('g-briques',(t,set)=>{
    if(!piece) piece={x:Math.floor(Math.random()*12),y:0,c:cols[Math.floor(Math.random()*5)]};
    if(piece.y>=N-2-haut[piece.x]*2){
      pile.push(piece); haut[piece.x]++; piece=null;
      if(haut[pile[pile.length-1].x]>=8){pile=[];haut=Array(12).fill(0);}
    } else piece.y++;
    pile.concat(piece?[piece]:[]).forEach(p=>{for(let i=0;i<4;i++)set(p.x*2+i%2,p.y+(i>>1),p.c);});
  });

  // Chenillard : une chenille de boules colorées qui ondule, un tireur en bas
  matrice('g-chenille',(t,set)=>{
    const k=['#ff4d5e','#f7b731','#b56bff','#4db8ff','#34d27a','#f7b731'];
    for(let i=0;i<6;i++){const x=(t/2+i*3)%N, y=4+Math.round(Math.sin((t/2+i*3)/3)*2); set(Math.floor(x),y,k[i]); set(Math.floor(x)+1,y,k[i]);}
    const tx=12+Math.round(Math.sin(t/12)*7); set(tx,21,'#e8e8f0'); set(tx-1,22,'#e8e8f0'); set(tx,22,'#e8e8f0'); set(tx+1,22,'#e8e8f0');
    for(let y=9+(t%10);y<20;y+=10) set(tx,y,'#4db8ff');
  });

  // Palet : deux raquettes, une balle qui rebondit
  matrice('g-palet',(t,set)=>{
    const p=(t%66), bx=p<33?2+p*0.667:24-(p-33)*0.667, by=11+Math.round(Math.sin(t/5)*8);
    for(let y=0;y<N;y+=3) set(12,y,'#4a4e66');
    for(let i=-2;i<=2;i++){ set(1,Math.round(by)+i,'#4db8ff'); set(22,Math.round(by)+i,'#ff5fa8'); }
    set(Math.round(bx),Math.round(by),'#f7b731');
    set(5,2,'#4db8ff'); set(18,2,'#ff5fa8');
  });
})();

/* Formulaire : envoi par fetch vers FormSubmit (le 1er envoi demande une confirmation par e-mail à bonjour@). */
(function(){
  const f=document.getElementById('form'), etat=document.getElementById('form-etat'); if(!f) return;
  f.addEventListener('submit',async e=>{
    e.preventDefault();
    etat.textContent='Envoi…';
    try{
      const r=await fetch(f.action,{method:'POST',body:new FormData(f),headers:{Accept:'application/json'}});
      const j=await r.json(); if(!r.ok||j.success==='false') throw 0;
      f.reset(); etat.textContent='Merci, votre demande est envoyée. Nous vous répondons rapidement.';
    }catch(_){ etat.textContent='L’envoi a échoué. Réessayez dans un instant.'; }
  });
})();
