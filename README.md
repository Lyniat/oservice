# OService

Multiplayer networking library for [DragonRuby Game Toolkit](https://dragonruby.org) with LAN, Steam, and GOG[¹](#notes) support.
Built on [Unet](https://github.com/codecat/unet) and [OSSP](https://github.com/Lyniat/ossp).

# Build

## LAN & ENet

- Enabled by default, can be disabled when launching OService from your app.

## Steam

1. Add `UNET_MODULE_STEAM=ON` to *build-unix* or pass it to CMake[²](#notes).
2. Add `STEAM_APP_ID=(your app id)` the same way.

## Windows

- Use MSVC. GCC/MSYS2 may work but hasn’t been tested recently.

## macOS

1. Run `build-unix`.
2. Sign the extension if you plan to distribute it[³](#notes).

## Linux

1. Run `build-unix`.
2. Run `patchelf --set-rpath $ORIGIN oservice.so` so the Steam library is loaded from the same directory.

# Usage

See [documentation](https://oservice.lyniat.games).

Place libraries as shown below.
`steam_api64.dll` must be in the game root.

```
- "game root directory"
    - "game executable"
    - steam_api64.dll
    - native
        - linux-amd64
            - libsteam_api.so
            - oservice.so
        - macos
            - libsteam_api.dylib
            - oservice.dylib
        - windows-amd64
            - oservice.dll
```

# Debugging

- On macOS, resign the DragonRuby app to allow attaching a debugger.
- On Linux, use `LD_DEBUG=libs ./dragonruby dr-socket-unet` to list missing dependencies.

# Notes

¹ GOG support exists, but is untested (I do not have a dev account).<br>
² Requires a Steamworks account.<br>
³ Requires an Apple certificate.
