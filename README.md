# reSavedata (Memory Card edition)

PSP2 savedata redirector, by [Princess of Sleeping](https://github.com/Princess-of-Sleeping/reSavedata).
This fork keeps the games' savedata on the **official Memory Card**, so a
savestate plugin such as [JamaisVu](https://github.com/delon5/JamaisVu) can
back it up and roll it back without ever writing to another drive.

## What it does

Every game's `savedata0:` is redirected to a plain folder (no PFS encryption):

- `uma0:data/resavedata/<TITLEID>/` when a storage plugin (StorageMgr, YAMT,
  gamesd, usbmc) put an SD2Vita, PSVSD or USB stick on ux0 and the Memory Card
  on uma0;
- `ux0:data/resavedata/<TITLEID>/` otherwise (the Memory Card is ux0).

The folder is created the first time the game starts; nothing has to be made
by hand. The drive is decided when the first game starts after boot (when the
mounts are settled) and kept until the next reboot. System applications
(`NPXS…`) are not redirected.

This is the plugin's 2021 mechanism (the `savedata0:` overlay and the
safemem/sdslot set-up), not the 2022 one, with two changes: every game is
redirected, and the folder is on the Memory Card.

## Existing saves

Saves are not moved automatically. A game whose saves are still in the
system's folder starts from empty until they are copied to
`<drive>:data/resavedata/<TITLEID>/`. Copy them out of
`ux0:user/00/savedata/<TITLEID>/` with VitaShell (which decrypts them on the
way out); saves made with the 2022 reSavedata in `ux0:data/resavedata` are
already in the right layout and only need moving to the card's drive if the
card is not ux0.

## Installation

Put `resavedata.skprx` in `ur0:tai/` (or `ux0:tai/`) and add its path to the
`*KERNEL` section of `config.txt`, **after** the storage plugin that mounts
the Memory Card on uma0 if you use one.

Firmwares: 3.55, 3.60, 3.65, 3.67–3.73.

## Building

[vitasdk](https://vitasdk.org/) with taiHEN; `./build.sh`, or the GitHub
Actions workflow, whose artifact holds `resavedata.skprx`.

## License

GPL-3.0, see LICENSE.
