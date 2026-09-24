# MIB2 High Software Collector

`custom.sh` is a read-only research collector for Harman/QNX **MIB2 High / MHI2** units. It is intended to collect software identification and the firmware files that may be relevant to CarPlay RGI, navigation, BAP, FPK/cluster integration, and related dependencies from firmware variants that are not yet available to the project maintainer.

Current collector version: **1.0.2**

## What it does

The collector:

- checks the available commands and selects compatible fallback methods before collection starts;
- verifies that the SD card is actually writable by creating, reading back, renaming, deleting, and syncing a test file;
- detects M.I.B.'s `/net/mmx` layout as well as direct/local QNX filesystem layouts;
- records vehicle/software identifiers, system information, mounts, filesystem information, and tool capabilities;
- searches known and discovered firmware locations for relevant executables, libraries, and configuration/metadata files;
- searches by known filenames, filename patterns, relevant embedded strings, and direct shared-library dependencies;
- preserves the original target filesystem paths in the collected copy;
- records checksums and a detailed operation log;
- performs final SD-card sync and verification before displaying **SAFE TO REMOVE THE SD CARD**.

The collector does **not** remount firmware filesystems, patch binaries, change permissions, modify coding/configuration, stop or restart services, or write to the vehicle firmware. Its intended writes are only to the verified removable SD/USB output location.

## Requirements

- Harman/QNX MIB2 High / MHI2 unit.
- A working [M.I.B. - More Incredible Bash](https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash) installation/SD card.
- The M.I.B. SD card must be used in **SD1** for the normal GEM launcher workflow.
- Enough free space on the SD card for the collected files.

This collector is designed for firmware research and has been syntax-tested and exercised against synthetic direct-QNX and M.I.B. `/net/mmx` layouts. New firmware variants can still expose layouts or commands not previously encountered, which is why the collector records detailed capability and failure information.

## Installation on the M.I.B. SD card

1. Remove the M.I.B. SD card from the vehicle and open it on a computer.
2. Open the `mod` folder on the SD card.
3. If `mod/custom.sh` already contains something you want to keep, back it up first, for example as `custom.sh.bak`.
4. Download the collector [`custom.sh`](custom.sh).
5. Copy it to the SD card as:

   ```text
   /mod/custom.sh
   ```

6. Make sure the filename is exactly `custom.sh`, not `custom.sh.txt`.
7. Safely eject the SD card from the computer.

M.I.B. itself uses `/mod/custom.sh` for its custom-script launcher.

## Running the collector

1. Insert the M.I.B. SD card into **SD1**.
2. Open the Green Engineering Menu / M.I.B. menu as you normally would.
3. Select the M.I.B. item labelled:

   ```text
   Run /mod/custom.sh from m.i.b. SD
   ```

4. M.I.B. may show its normal generic warning about programming/flashing. The collector itself does not flash or patch the unit.
5. Leave the SD card inserted and let the collector finish. Do not press Back or remove the card while it is working.
6. The collector shows simple numbered progress steps. Searching can take a few minutes depending on the unit and firmware.
7. Remove the SD card **only after** the collector displays:

   ```text
   It is now SAFE TO REMOVE THE SD CARD.
   ```

## After collection

On the SD card there will be a new folder with a descriptive name similar to:

```text
MIB2_SKODA_MHI2_ER_SKG13_P4523_1_20260924_135524
```

The exact name is generated from the detected brand/software information and collection date whenever those values can be identified.

A successful collection contains `COLLECTION_COMPLETE.txt`. Open it and confirm it contains:

```text
Status: COMPLETE
```

Then ZIP the **entire generated folder** on your computer and send the ZIP to the project maintainer. Do not send only individual binaries; the logs, manifests, identity data, and directory structure are important for comparing firmware variants.

The collector intentionally leaves the result as a normal folder on the MIB unit instead of spending extra time and CPU creating an archive there. ZIP it on the computer afterwards.

## Main output files

Typical output includes:

```text
COLLECTION_COMPLETE.txt
collector.log
collector_source.sh
manifest.tsv
capabilities.txt

system/
  identity.txt
  system_info.txt
  mounts.txt
  df.txt
  sd_card_check.txt
  collection_summary.txt

discovery/
  filesystem_inventory.txt
  identity_candidates.txt
  relevant_matches.txt
  string_match_evidence.txt
  dependency_names.txt

files/
  ...collected firmware files with target paths preserved...
```

`collector.log` is intentionally detailed. It records command/tool selection, fallbacks, scan locations, matches, copies, failures, hashes, SD-card checks, and final verification so failed or unusual runs can be diagnosed without asking the vehicle owner to reproduce every step manually.

## If something goes wrong

### `COLLECTION NOT STARTED`

The collector could not prove that removable storage was writable. No firmware collection is started in this state. Check the SD card on a computer, including the physical lock switch if present, free space, and filesystem health. Only run the collector again after changing/fixing something.

### `STORAGE ERROR - COLLECTION STOPPED`

The SD card stopped accepting writes during collection. Follow the message on screen, check the card on a computer, and only retry after the storage problem has been addressed.

### `COLLECTION NOT COMPLETED`

Do **not** repeatedly run the script in a loop. The collector already retries its final verification internally. Remove the card when the screen says it is safe, then send the generated collection folder to the project maintainer for inspection.

### The result folder exists but there is no `COLLECTION_COMPLETE.txt`

Treat the run as incomplete. Keep the folder and its `collector.log`; they may show exactly where the run stopped.

## Privacy note

The scan roots are intentionally focused on system code and avoid normal removable media, map/media collections, paired-phone databases, and other obvious user-data areas. However, firmware metadata can still contain device-specific identifiers. Review a collection before posting it publicly. For initial research, sharing the ZIP privately with the project maintainer is preferable.

## Restoring another custom script

After collecting, you can remove this `mod/custom.sh` from the M.I.B. SD card and restore your previous `custom.sh` backup if you had one.
