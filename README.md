## xwebview

An X server viewer for the browser.  Similar to VNC, but aiming for lower latency and lossless image quality.

`xwebview` grabs pixel data directly from the X server, compresses it using LZ4, and sends it to a client over a WebSocket connection.  Although there are still optimizations to be made, the experience is noticeably snappier than VNC (and remains reasonably quick under high load).

### Demo

(Framerate and resolution limited for filesize, see [demo.mp4](https://github.com/Cubified/xwebview/blob/main/demo.mp4) for original [but still low-res] recording)

![Demo](https://github.com/Cubified/xwebview/blob/main/demo.gif)

### Compiling

`xwebview` depends upon:

     - liblz4(-dev)
     - libX11(-dev)
     - libXrandr(-dev)
     - libXdamage(-dev)
     - libXfixes(-dev)
     - xdotool

It also depends upon [lz4-asm](https://www.npmjs.com/package/lz4-asm), but all relevant files are bundled within this project to avoid `npm`/`yarn` being necessary to install one dependency.

Compile with:

    $ make

### Running

The binary produced will include the following parts:

     - Static file server (for serving client-side HTML, JS, etc.) -- see `server/fsserver.h` for standalone
     - WebSocket server (for sending image data to and receiving input events from the client) -- uses [wsServer](https://github.com/Theldus/wsServer)
     - X server event handler (for X damage/screen update events)

Run it with:

     $ cd client
     $ ../server/xwebview

And open:

     http://localhost:5123
     (or)
     http://{your IP address}:5123

In a web browser.
