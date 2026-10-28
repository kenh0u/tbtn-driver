# tbtn-driver

Linux ACPI kernel driver for the Panasonic TOUGHBOOK tablet front-bezel hardware buttons (`TBTN` / `MAT0034`).

Tested on **Panasonic Toughbook CF-20 (CF-20-2)** running NixOS (Linux 6.12+).

---

## Installation (NixOS)

This repository provides a Nix flake with a built-in NixOS module.

### 1. Add Flake Input

Add `tbtn-driver` to your system's `flake.nix`:

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    tbtn-driver = {
      url = "github:kenh0u/tbtn-driver";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, tbtn-driver, ... }: {
    nixosConfigurations.your-hostname = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      modules = [
        ./configuration.nix
        tbtn-driver.nixosModules.default
      ];
    };
  };
}
```

### 2. Enable the Module

Importing `inputs.tbtn-driver.nixosModules.default` automatically:
- Builds the out-of-tree kernel module against your active `boot.kernelPackages`.
- Adds the package to `boot.extraModulePackages`.
- Adds `"tbtn_driver"` to `boot.kernelModules` so it loads on boot.

---

## Usage Example

Because `tbtn-driver` reports genuine press and release events (without synthetic auto-release), buttons can be held, chorded, or tapped repeatedly while another is held.

Here is an example configuration using [`keyd`](https://github.com/rvaiya/keyd) for tablet navigation:

```nix
{ pkgs, ... }:

{
  services.keyd = {
    enable = true;
    keyboards.default = {
      ids = [ "*" "0000:0000:f4b3f222" ];
      settings = {
        global = {
          chord_timeout = 300;
          disable_modifier_guard = 1;
        };
        main = {
          # Rotate button (↴): remap to F24 for dedicated OSK toggle
          f13 = "f24";

          # Navigation buttons: A1=Left, A2=Right, A1+A2=Up
          f14 = "overload(a1_hold, left)";
          f15 = "overload(a2_hold, right)";
          "f14+f15" = "up";

          # Volume buttons: -=VolDown, +=VolUp, -++=Down
          volumedown = "overload(voldown_hold, volumedown)";
          volumeup = "overload(volup_hold, volumeup)";
          "volumedown+volumeup" = "down";

          # Windows Button: single tap = Overview (Mod+O), hold = Mod modifier
          leftmeta = "overload(meta, M-o)";
        };
        a1_hold = {
          f15 = "up";
        };
        a2_hold = {
          f14 = "up";
        };
        voldown_hold = {
          volumeup = "down";
        };
        volup_hold = {
          volumedown = "down";
        };
      };
    };
  };
}
```

---

## Technical Details

### ACPI Specification & Device Handling

On the Toughbook CF-20, physical buttons are routed through multiple controllers. The `tbtn-driver` attaches specifically to the ACPI device node `TBTN` (`MAT0034`).

- **ACPI Device ID**: `MAT0034`
- **ACPI Event**: `0x80`
- **Method**: The driver evaluates the `HINF` method on each event:
  - Bit 7 (`0x80`): Indicates press vs. release state.
  - Bits 0–6 (`0x7F`): Key identifier.

### Button Mappings

| Physical Button | Front Bezel Position | ACPI `HINF` (Press / Release) | Linux Keycode | Evdev Name |
| :--- | :--- | :--- | :--- | :--- |
| **Rotate (↴)** | Button 6 (Rightmost) | `0x36` / `0x37` | `183` | `KEY_F13` |
| **A1 Button** | Button 1 (Leftmost) | `0x38` / `0x39` | `184` | `KEY_F14` |
| **A2 Button** | Button 2 | `0x42` / `0x43` | `185` | `KEY_F15` |

Other front bezel buttons on the CF-20 are handled by existing drivers and do not require this driver:
- **Volume Down (`-`) / Volume Up (`+`)**: Handled by the EC through `Panasonic Laptop Support` (`KEY_VOLUMEDOWN` = 114, `KEY_VOLUMEUP` = 115).
- **Windows Button**: Handled via the internal keyboard controller on `AT Translated Set 2 keyboard` (`KEY_LEFTMETA` = 125).

### Real-Time Event Reporting (Non-Autorelease)

Default ACPI button drivers often call `sparse_keymap_report_event` with `autorelease = true`, which immediately sends a key release right after press. `tbtn-driver` passes `autorelease = false` and explicitly tracks both the press scancodes (`0x36`, `0x38`, `0x42`) and release scancodes (`0x37`, `0x39`, `0x43`), ensuring accurate key state reporting.

### Event Verification

Button events can be inspected in real time using `evtest`:

```bash
sudo evtest
```

Select the device named **`Panasonic TBTN Tablet Buttons`** to verify press and release events:

```text
Event: time 1730123456.123456, type 1 (EV_KEY), code 184 (KEY_F14), value 1
Event: time 1730123456.123456, -------------- SYN_REPORT ------------
Event: time 1730123456.323456, type 1 (EV_KEY), code 184 (KEY_F14), value 0
Event: time 1730123456.323456, -------------- SYN_REPORT ------------
```
