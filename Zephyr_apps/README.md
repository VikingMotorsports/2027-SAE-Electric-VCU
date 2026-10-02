# Zephyr_apps

Zephyr applications for the VMS Module (STM32C092RCT6). The custom board definition lives in this folder, so no changes to the Zephyr workspace are needed.

## Layout

```
Zephyr_apps/
├── boards/vmsboards/vms_module_rev1/   # custom board definition
├── dts/bindings/vendor-prefixes.txt    # registers the "vmsboards" vendor prefix
├── blinky/                             # example app
└── <your_app>/
```

## Prerequisites

- Zephyr workspace set up with the Zephyr SDK (written against Zephyr 4.3.x; the STM32C092 needs a recent release)
- STM32CubeProgrammer installed, with `STM32_Programmer_CLI` on your PATH
- ST-LINK V3 MINI connected to the board's STDC14 header (keep its firmware up to date)

## Building and flashing

Run from your Zephyr workspace (with the venv active). Set `$apps` to your local path of this folder:

```powershell
$apps = "C:\path\to\2027-SAE-Electric-VCU\Zephyr_apps"
west build -p always -b vms_module_rev1 "$apps\blinky" -d "$apps\blinky\build"
west flash -d "$apps\blinky\build"
```

`west flash` uses STM32CubeProgrammer over SWD by default and finds the ST-LINK automatically. If you have several probes attached, add `--tool-opt="sn=<serial>"`. OpenOCD is available as a fallback with `--runner openocd`.

Add `build/` to `.gitignore`.

### Using the flashing script

The repo also has a PowerShell script in `Zephyr_Scripts/` that activates the Zephyr venv, builds and flashes an app, and returns to the repo. If you use it, you do not need the manual commands above. Just edit the variables at the top of the script:

```powershell
$projectname = 'zephyrproject'                 # your Zephyr workspace folder name
$projectdir  = '~\zephyr'                      # folder containing the workspace
$board       = 'vms_module_rev1'               # use this for the VMS Module (was nucleo_c092rc)
$app         = 'counter'                       # app folder name inside Zephyr_apps
$path        = 'C:\path\to\2027-SAE-Electric-VCU'   # your local clone of this repo
```

Set `$board` to `vms_module_rev1` to target the custom board, or `nucleo_c092rc` for a stock Nucleo. The app must still contain the `BOARD_ROOT` lines from the section below, or the build will not find the custom board. The script builds into the Zephyr workspace's default build folder, so `-p always` (already in the script) clears stale builds when you switch boards or apps.

## Creating a new app for this board

1. Create the app folder under `Zephyr_apps/` (copying a Zephyr sample such as `samples/basic/blinky` is fine).
2. Add these two lines to the app's `CMakeLists.txt`, **before** `find_package(Zephyr ...)`:

   ```cmake
   cmake_minimum_required(VERSION 3.20.0)

   list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
   list(APPEND DTS_ROOT   ${CMAKE_CURRENT_SOURCE_DIR}/..)

   find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
   project(my_app)
   ```

   `BOARD_ROOT` lets Zephyr find `vms_module_rev1`. `DTS_ROOT` makes the vendor prefix file apply (avoids vendor warnings). Without these lines the build fails with "No board named 'vms_module_rev1' found". Adjust the `/..` if your app is nested deeper.
3. Build and flash as above.

Board-specific devicetree changes for an app (extra peripherals, pin assignments) go in `<your_app>/boards/vms_module_rev1.overlay`, which Zephyr applies automatically.

## Board summary (`vms_module_rev1`)

| Item | Setting |
|---|---|
| MCU | STM32C092RCT6 (LQFP64), based on NUCLEO-C092RC |
| Clocks | No HSE/LSE fitted. SYSCLK from internal HSI at 48 MHz, LSI for watchdog |
| User LEDs | `led0` = PA5, `led1` = PC9, both **active low** |
| Console | USART2 (PA2/PA3), 115200 baud, via ST-LINK VCP |
| CAN-FD | FDCAN1 on PD0 (RX) / PD1 (TX), MCP2562FD transceiver with standby on PD2. `zephyr,canbus` is set |
| FDCAN clock | PCLK (48 MHz), because the Nucleo's HSE source does not exist on this board |

Everything else is not enabled by default. Header-broken-out pins are available for peripherals; enable them with an app overlay.

## Important notes

- **CAN timing:** the internal HSI is only about +/-1% accurate and drifts with temperature. This is fine for bench testing with moderate classic CAN bitrates (125 to 500 kbit/s). It is marginal for a production vehicle bus and for CAN-FD data rates. The C0 series has no PLL, so some high CAN-FD data rates (5 and 8 Mbit/s) cannot be hit with the 48 MHz CAN clock.
- **Running on a stock NUCLEO-C092RC:** `west build -b nucleo_c092rc` also works for most apps, since it's the same MCU. Note that the Nucleo's LED0 is active high, and its clocks use the on-board HSE. The custom board definition above is the source of truth for the VMS Module.
- **Naming:** the board name is lowercase `vms_module_rev1` everywhere (folder, file names, `board.yml`, `-b` argument). Zephyr is case sensitive here.

## Troubleshooting

- **"No board named ... found":** the `BOARD_ROOT` line is missing from the app's `CMakeLists.txt`, or it is placed after `find_package(Zephyr ...)`. To check discovery independently, run `west boards --board-root $apps -n vms`. It should list `vms_module_rev1`.
- **Board listed but build fails:** read the first devicetree or Kconfig error. The board files live in `boards/vmsboards/vms_module_rev1/`.
- **"CAN device not ready":** check that `zephyr,canbus` and the `&fdcan1` clock settings in `vms_module_rev1.dts` were not overridden by an app overlay.
- **`west flash` cannot connect:** check the STDC14 connection, that the board is powered, and that CubeProgrammer is installed and on PATH.

## Board files

| File | Purpose |
|---|---|
| `board.yml` | Board metadata and discovery entry point (needs `full_name`, `vendor`, `socs`) |
| `vms_module_rev1.dts` | Hardware description: clocks, pins, LEDs, CAN, console |
| `vms_module_rev1_defconfig` | Default Kconfig options |
| `Kconfig.vms_module_rev1` | Selects the SoC |
| `vms_module_rev1.yaml` | Metadata used by the test and CI tooling |
| `board.cmake` | Flash runner settings (STM32CubeProgrammer default, OpenOCD fallback) |
