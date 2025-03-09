# What is this

This is a repo with different network related implementations in C.

It includes:

- A simple HTTP Sever
- A feature complete WebSocket impleementation
- A simple FTP Server

## Why?

I had to write a really simple http server as an assignment at university. After that I added more features on top of that http server and also some more things. It is written in C, because it is challenging, especially since this got bigger than initially intended. This project ranges over many years, so the code is not the same in every place. It can be messy in some places.

## Features

### HTTP Server

This server serves just static http, and has a hit counter (just for fun).

It also supports https (if sou start it correctly and have openssl installed).
It uses openssl for that.

### WebSocket Server

You have to connect to a static URL and it's just a plain old WS Echo server. It passes most / (all without WS extensions) of the [autobahn test suite](https://github.com/crossbario/autobahn-testsuite). Even the ones with UTF-8. I use the [utf8proc](https://github.com/JuliaStrings/utf8proc/) library there.

### FTP Server

This only supports a few simple FTP things, atm I am writing this, it's not even finished. So maybe it does more things. But I want it to work with filezilla, default settings and be able to handle the default workflow. FTP is much more complicated than HTTP or Websockets, so it might take some time.

## How to build

You need [meson](https://mesonbuild.com/) and then you just can run:

```bash
meson setup build
meson compile -C build
./build/server --help
```
