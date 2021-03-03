/*
 * app.js: X server viewer for the browser
 */

//////////////////////////////
// LZ4
//
const lz4module = lz4init();
lz4module.then((lz4)=>{
  window.lz4 = lz4.lz4js;
});

//////////////////////////////
// GLOBAL VARIABLES
//
let socket = new WebSocket(`ws://${location.hostname}:8080`);
let canv = document.getElementById('canv');
let ctx = canv.getContext('2d');
let w, h;
let incoming = {
  x: 0,
  y: 0,
  w: 0,
  h: 0,
  length: 0
};
let monitors = [];
let monitor_ind = 0;

//////////////////////////////
// WEBSOCKETS
//
socket.onmessage = function(event){
  if(event.data.length <= 64){
    if(event.data.toString().indexOf('MONITOR:') > -1){
      monitors.push(JSON.parse(event.data.split('MONITOR:')[1]));
      monitors.sort((a, b)=>{
        if(a.x < b.x){
          return -1;
        } else if(a.x > b.x){
          return 1;
        }
        return 0;
      });
      canv.width = monitors.reduce((acc, mon)=>{
        return (acc+mon.w);
      }, 0);
      canv.height = monitors.reduce((acc, mon)=>{
        return (acc+mon.h);
      }, 0);
      set_monitor();
      return;
    } else if(event.data.toString().indexOf('GETREADY:') > -1){
      incoming = JSON.parse(event.data.split('GETREADY:')[1]);
      return;
    }
  }

  const reader = new FileReader();
  reader.addEventListener('loadend', ()=>{
    if(reader.result.byteLength !== incoming.length) return;

    let arr = new Uint8ClampedArray(lz4.decompress(new Uint8Array(reader.result)));
    for(let i=3;i<(incoming.w*incoming.h*4)+4;i+=4){
      arr[i] = 255;
      arr[i-3] ^= arr[i-1]; /* BGRA -> RGBA, XOR in-place swap */
      arr[i-1] ^= arr[i-3];
      arr[i-3] ^= arr[i-1];
    }
    let data = new ImageData(arr, incoming.w, incoming.h);
    ctx.putImageData(data, incoming.x, incoming.y);
  });
  reader.readAsArrayBuffer(event.data);
};

socket.onclose = function(event){
  canv.style.opacity = '0.5';
  document.title = 'xwebview | Connection Closed';
};

window.addEventListener('beforeunload', ()=>{
  socket.close();
});

//////////////////////////////
// USER INPUT
//
function handle_mouse(e, mode){
  let mon = monitors[monitor_ind];
  let scale = window.innerWidth/mon.w;
  socket.send(`${mode}:${e.button+1}:${Math.round((e.pageX/scale)+mon.x)}:${Math.round((e.pageY/scale)+mon.y)}`);
}
canv.addEventListener('mousedown', (e)=>{
  handle_mouse(e, 0);
});
canv.addEventListener('mouseup', (e)=>{
  handle_mouse(e, 1);
});

function handle_key(e, mode){
  /* TODO: Add more
   *
   * Unfortunately, I haven't found a
   * way to generate this programmatically
   * because every Unicode-to-Xlib mapping
   * available online only covers characters,
   * not keys (unsurprisingly).  This means that
   * what is listed here (arrow keys, modifier keys,
   * etc.) must be added manually, because
   * Javascript's e.key does the trick for
   * everything else.
   */
  const keysyms = {
    ' ': 'KP_Space',
    'Shift': 'Shift_L',
    'Alt': 'Alt_L',
    'Enter': 'Return',
    'Backspace': 'BackSpace',
    'CapsLock': 'Caps_Lock',
    'Tab': 'Tab',
    'Control': 'Control_L',
    'ArrowLeft': 'Left',
    'ArrowRight': 'Right',
    'ArrowUp': 'Up',
    'ArrowDown': 'Down',
    'PageUp': 'KP_Page_Up',
    'PageDown': 'KP_Page_Down'
  };

  let str = `${mode}:0:0:0:`;

  if(keysyms[e.key] == undefined){
    if(e.key.length == 1) str += e.key;
  } else {
    str += keysyms[e.key];
  }

  socket.send(str);
}
document.addEventListener('keydown', (e)=>{
  if(e.key !== 'F11'){
    e.preventDefault();
  }

  handle_key(e, 2);
});
document.addEventListener('keyup', (e)=>{
  handle_key(e, 3);
});

//////////////////////////////
// MONITOR VIEW
//
function set_monitor(){
  let mon = monitors[monitor_ind];
  let scale = window.innerWidth/mon.w;

  canv.style.transformOrigin = `${mon.x}px ${mon.y}px`;
  canv.style.transform = `translate(-${mon.x}px, -${mon.y}px) scale(${scale})`;

  document.getElementById('monitor-left').disabled = (monitor_ind==0);
  document.getElementById('monitor-right').disabled = (monitor_ind==monitors.length-1);
}

document.getElementById('monitor-left').addEventListener('click', ()=>{
  if(monitor_ind > 0){
    monitor_ind--;
    set_monitor();
  }
});
document.getElementById('monitor-right').addEventListener('click', ()=>{
  if(monitor_ind < monitors.length-1){
    monitor_ind++;
    set_monitor();
  }
});

window.addEventListener('resize', ()=>{
  set_monitor();
});
