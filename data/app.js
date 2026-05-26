const qs = new URLSearchParams(location.search);
const TOKEN = qs.get("t") || "";
let CLIENT_MAC = localStorage.getItem("client_mac") || "";

function hdrs(){
  const h = {};
  if (TOKEN) h["X-Token"] = TOKEN;
  if (CLIENT_MAC) h["X-Client-MAC"] = CLIENT_MAC;
  return h;
}

async function apiGet(url){
  const r = await fetch(url, {headers: hdrs()});
  if(!r.ok) throw new Error(await r.text());
  return await r.json();
}
async function apiPost(url, bodyObj){
  const r = await fetch(url, {
    method:"POST",
    headers: {"Content-Type":"application/json", ...hdrs()},
    body: JSON.stringify(bodyObj)
  });
  if(!r.ok) throw new Error(await r.text());
  return await r.json();
}

function setStatus(ok){
  const el = document.getElementById("status");
  el.textContent = ok ? "online" : "offline";
  el.style.color = ok ? "var(--ok)" : "var(--bad)";
}

function tabInit(){
  document.querySelectorAll(".tab").forEach(btn=>{
    btn.addEventListener("click", ()=>{
      document.querySelectorAll(".tab").forEach(b=>b.classList.remove("active"));
      document.querySelectorAll(".view").forEach(v=>v.classList.remove("active"));
      btn.classList.add("active");
      document.getElementById("view-"+btn.dataset.tab).classList.add("active");
    });
  });
}

function logScan(s){
  const out = document.getElementById("scanOut");
  out.textContent += s + "\n";
  out.scrollTop = out.scrollHeight;
}

async function doSweep(){
  const start = document.getElementById("startHz").value.trim();
  const stop  = document.getElementById("stopHz").value.trim();
  const step  = document.getElementById("stepHz").value.trim();

  logScan(`> sweep start=${start} stop=${stop} step=${step}`);
  const j = await apiGet(`/api/radio/sweep?start=${encodeURIComponent(start)}&stop=${encodeURIComponent(stop)}&step=${encodeURIComponent(step)}`);
  (j.points || []).forEach(p=>{
    logScan(`${p.f} Hz  RSSI ~ ${p.r.toFixed(1)} dBm`);
  });
  logScan("> done");
}

function scriptsRow(item){
  const div = document.createElement("div");
  div.className = "item";
  const left = document.createElement("div");
  left.innerHTML = `<div class="name">${item.name}</div><div class="meta">${item.size} B</div>`;
  const actions = document.createElement("div");
  actions.className = "actions";

  const bDl = document.createElement("button");
  bDl.className = "px-btn px-btn-ghost";
  bDl.textContent = "Download";
  bDl.onclick = ()=> {
    const u = `/api/scripts/download?path=${encodeURIComponent(item.name)}&t=${encodeURIComponent(TOKEN)}`;
    window.open(u, "_blank");
  };

  const bDel = document.createElement("button");
  bDel.className = "px-btn";
  bDel.textContent = "Delete";
  bDel.onclick = async ()=> {
    const path = item.name;
    const r = await fetch("/api/scripts/delete", {
      method:"POST",
      headers: {"Content-Type":"application/x-www-form-urlencoded", ...hdrs()},
      body: `path=${encodeURIComponent(path)}`
    });
    if(!r.ok) alert(await r.text());
    await refreshScripts();
  };

  actions.appendChild(bDl);
  actions.appendChild(bDel);

  div.appendChild(left);
  div.appendChild(actions);
  return div;
}

async function refreshScripts(){
  const list = document.getElementById("scriptsList");
  list.innerHTML = "";
  const j = await apiGet("/api/scripts");
  (j.items || []).forEach(it=>{
    list.appendChild(scriptsRow(it));
  });
}

async function uploadScript(){
  const f = document.getElementById("fileUp").files[0];
  if(!f) return alert("Wybierz plik.");
  const fd = new FormData();
  fd.append("file", f, f.name);

  const r = await fetch("/api/scripts/upload", {method:"POST", headers: hdrs(), body: fd});
  if(!r.ok) return alert(await r.text());
  await refreshScripts();
}

async function loadConfig(){
  const j = await apiGet("/api/config");
  document.getElementById("wifiSsid").value = j.wifi?.ssid || "";
  document.getElementById("requireToken").checked = !!j.security?.requireToken;

  const macList = document.getElementById("macList");
  macList.innerHTML = "";
  (j.security?.macWhitelist || []).forEach(m=>{
    const div = document.createElement("div");
    div.className = "item";
    div.innerHTML = `<div><div class="name">${m}</div><div class="meta">whitelist</div></div>`;
    const act = document.createElement("div");
    act.className = "actions";
    const b = document.createElement("button");
    b.className = "px-btn px-btn-ghost";
    b.textContent = "Usuń";
    b.onclick = async ()=>{
      const cur = await apiGet("/api/config");
      const arr = (cur.security?.macWhitelist || []).filter(x=>x !== m);
      await apiPost("/api/config", {
        wifi:{ssid:document.getElementById("wifiSsid").value, pass: document.getElementById("wifiPass").value},
        security:{requireToken:document.getElementById("requireToken").checked, macWhitelist:arr}
      });
      await loadConfig();
    };
    act.appendChild(b);
    div.appendChild(act);
    macList.appendChild(div);
  });
}

async function saveConfig(){
  await apiPost("/api/config", {
    wifi:{
      ssid: document.getElementById("wifiSsid").value,
      pass: document.getElementById("wifiPass").value
    },
    security:{
      requireToken: document.getElementById("requireToken").checked,
      macWhitelist: await (async ()=>{
        const cur = await apiGet("/api/config");
        return cur.security?.macWhitelist || [];
      })()
    }
  });
  alert("Zapisano. Jeśli zmieniłeś Wi‑Fi, zrestartuj urządzenie.");
}

function addMac(){
  const mac = document.getElementById("macAdd").value.trim().toUpperCase();
  if(!mac) return;
  CLIENT_MAC = mac;
  localStorage.setItem("client_mac", mac);

  // add to whitelist too
  apiGet("/api/config").then(cur=>{
    const arr = cur.security?.macWhitelist || [];
    if(!arr.includes(mac)) arr.push(mac);
    return apiPost("/api/config", {
      wifi:{ssid:document.getElementById("wifiSsid").value, pass: document.getElementById("wifiPass").value},
      security:{requireToken:document.getElementById("requireToken").checked, macWhitelist:arr}
    });
  }).then(loadConfig);
}

async function init(){
  tabInit();

  document.getElementById("btnSweep").onclick = ()=>doSweep().catch(e=>alert(e));
  document.getElementById("btnClear").onclick = ()=>document.getElementById("scanOut").textContent="";
  document.getElementById("btnRefreshScripts").onclick = ()=>refreshScripts().catch(e=>alert(e));
  document.getElementById("btnUpload").onclick = ()=>uploadScript().catch(e=>alert(e));
  document.getElementById("btnSaveCfg").onclick = ()=>saveConfig().catch(e=>alert(e));
  document.getElementById("btnAddMac").onclick = ()=>addMac();

  try{
    await apiGet("/api/ping");
    setStatus(true);
  }catch(e){
    setStatus(false);
  }

  try{ await refreshScripts(); }catch(e){}
  try{ await loadConfig(); }catch(e){}
}

init();