# ESP32-S3 Hardware Security

Capstone v0.4 — Secure Storage & Trusted Boot

Target: ESP32-S3 · ESP-IDF v6.1

## 1. Security objectives and threat model

The ESP32-S3 is installed outside the building, in a physically accessible cistern environment. The security design prioritizes **preventing unauthorized firmware execution and disclosure of credentials**, while keeping local USB/UART maintenance and HTTPS OTA available.

We distinguish **device compromise** from **physical destruction or removal**. An attacker with physical access could remove the device or damage it; preventing every possible denial-of-service attack is therefore not a realistic objective for the firmware security configuration alone.

Our principal requirements are:

- Prevent unauthorized firmware from executing.
- Protect Wi-Fi credentials, MQTT private keys, and other stored secrets.
- Preserve local USB/UART access for maintenance and recovery (without implying operator authentication).
- Continue supporting signed firmware updates via HTTPS OTA.

> **Accepted trade-off — availability is not guaranteed**
>
> **Secure Download Mode restricts UART ROM commands; it does not authenticate the operator.** Physical access can therefore permit unauthenticated flash writes and sector erases. These actions can corrupt the installed firmware or configuration and stop the device working. We accept this denial-of-service risk because the outdoor device can also be removed or destroyed physically.
>
> **Writing flash is not the same as running an attacker's firmware.** Secure Boot v2 and Flash Encryption are intended to prevent an attacker from installing and booting modified, unauthorized application code, assuming trusted keys and verified boot have not been compromised.

## 2. Selected architecture for v0.4

| Feature | Description |
| :--- | :--- |
| **Secure Boot v2** | Only firmware signed with our trusted signing key can execute. |
| **Release-mode Flash Encryption** | A device-specific key protects firmware and sensitive flash contents. |
| **NVS Encryption** | Stored configuration and credentials are encrypted using protected NVS keys. |
| **Secure Download Mode** | UART maintenance commands are restricted, without authenticating the operator. |
| **Signed OTA updates** | The ESP verifies firmware signatures before accepting updates. |
| **Host-encrypted serial updates** | Locally flashed firmware is encrypted for the device and destination address. |
| **Disabled JTAG** | Normal hardware debugging access is blocked. |
| Flash encryption key | Host-generated and unique per device; backed up securely |
| Firmware signing key | RSA-3072 private key; backed up securely |
| USB/UART flash erase | Sector-aligned `erase-region` can erase part or all of flash, including NVS; eFuses and their keys remain unchanged. |

**Why Flash Encryption even though our firmware is open source?** We are protecting flash-resident secrets and reducing physical tampering opportunities, not hiding the implementation. Espressif recommends combining Flash Encryption with Secure Boot to mitigate attacks such as changing flash contents after signature verification.

### Physical intruder scenarios — what is and is not prevented

These scenarios were examined explicitly because the cistern electronics are accessible from outside the building. The distinctions below are essential to the security claim.

| Intruder action | Result / protection boundary |
| :--- | :--- |
| Issue `write-flash` or `erase-region` over UART | **Possible without owner keys** in Secure Download Mode. May corrupt or disable the device, but does not by itself enable execution of unauthorized code. |
| Compile the public firmware with a modified credential-stealing `config.html`, then flash it | **Cannot normally boot:** a modified application lacks the authorized Secure Boot signature; host-side flash encryption also requires the device-specific AES key. A destructive flash write alone does not bypass these checks. |
| Modify the genuine HTTPS configuration page in external flash | **Protected by the signed application image:** in this project `config.html` is included in the application at build time via `asm()`, not loaded from an independently writable web filesystem. |
| Extract the existing ESP32's Wi-Fi or TLS credentials from raw flash | **Protected at rest** by Flash Encryption and encrypted NVS, assuming proper key provisioning. Encryption does not protect secrets already exposed through another software vulnerability or leaked build files. |
| Replace the whole ESP32 board with an impersonating device | **Not prevented by Secure Boot or Flash Encryption on the original board.** A replacement can clone the public UI and attempt to trick the operator into entering credentials. This requires HTTPS server identity verification on the client, documented separately. |
| Replay older, correctly signed firmware | **Separate concern:** signature validity alone is not anti-rollback. Assess versioning, per-device ciphertext availability, partition layout and eFuse-based anti-rollback before claiming downgrade resistance. |

### Project-specific protection of web assets and certificates

For the existing `app_https_server` component, `config.html` and the HTTPS server certificate/private key are embedded into the application binary during the build using `asm()`. They are consequently covered by the signed executable image; Flash Encryption protects the private key at rest in the deployed firmware. Certificate and key files are created before building and **are not published in the public repository**.

This arrangement protects the integrity of the *original* device's web page, but does **not** authenticate a physically substituted ESP32 to Safari. Client-side HTTPS identity validation is a distinct workstream: [HTTPS_SERVER_IDENTITY.md](HTTPS_SERVER_IDENTITY.md).

## 3. Prepare the board and signed build

Start with a fresh, flash-erased ESP32-S3 with security eFuses unset. Activate ESP-IDF and run commands from the project root after creating the HTTPS certificate and key in [README 6.1](../README.md#61-create-the-devices-https-certificate).

> **Security eFuse changes are permanent.** Provision the encryption key before first secure boot. Back up both keys and retain them for later builds.

### 3.1 Connect and inspect the fresh board

Connect through USB-to-UART; replace Ubuntu port `/dev/ttyACM0` as needed. macOS uses `/dev/cu.*`. For a VM, pass the USB device to the guest; remove an old board's serial-number filter if necessary.

```bash
idf.py -p /dev/ttyACM0 efuse-summary
```

Expect `SECURE_BOOT_EN = False`, `SPI_BOOT_CRYPT_CNT = 0`, `ENABLE_SECURITY_DOWNLOAD = False`, writable `RD_DIS`, and empty, writable key blocks. If the fresh board's flash has not yet been erased, do that now:

```bash
idf.py -p /dev/ttyACM0 erase-flash
```

### 3.2 Create the signing key

Use this layout, exclude `/security/` in the root `.gitignore`, and generate each key once:

```text
security/
└── secure_boot_v2/
    └── secure_boot_signing_key.pem
```

```bash
mkdir -p security/secure_boot_v2
openssl genrsa -out security/secure_boot_v2/secure_boot_signing_key.pem 3072
```

### 3.3 Configure Secure Boot v2 and build

Run `idf.py menuconfig`. Under **Security features**, select:

| Setting | Value |
| --- | --- |
| App Signing Scheme / Secure Boot version | RSA / Secure Boot v2 |
| Enable hardware Secure Boot in bootloader | Enabled |
| Sign binaries during build | Enabled |
| Secure boot private signing key | `security/secure_boot_v2/secure_boot_signing_key.pem` |
| Aggressive key revoke / Allow potentially insecure options | Disabled |
| Do not disable the ability to further read protect eFuses | Unchecked; the encryption key will be provisioned before first boot |
| Flash bootloader along with other artifacts | Unchecked; flash it separately in step 4.4 |
| UART ROM download mode | Permanently switch to Secure mode |

Leave Flash Encryption disabled for this first build. Save, then run:

```bash
idf.py build
```

This builds and signs the application and bootloader without changing device eFuses. Provision the encryption key next.

## 4. Provision encryption and perform the first flash

### 4.1 Generate and provision the flash-encryption key

Add the flash-encryption directory. The complete key layout is now:

```text
security/
├── secure_boot_v2/
│   └── secure_boot_signing_key.pem
└── flash_encryption/
    └── flash_encryption_key.bin
```

```bash
mkdir -p security/flash_encryption
idf.py secure-generate-flash-encryption-key security/flash_encryption/flash_encryption_key.bin
wc -c security/flash_encryption/flash_encryption_key.bin
```

The AES-128 XTS key file must contain **32 bytes**. Back it up before programming the empty `BLOCK_KEY0`:

```bash
idf.py -p /dev/ttyACM0 efuse-burn-key \
  BLOCK_KEY0 \
  security/flash_encryption/flash_encryption_key.bin \
  XTS_AES_128_KEY
```

The command pauses at its confirmation prompt. Check the block and purpose, enter `BURN` there, and wait for `Successful` and the shell prompt. Then run the separate read-only check:

```bash
idf.py -p /dev/ttyACM0 efuse-summary
```

Verify `KEY_PURPOSE_0 = XTS_AES_128_KEY` is write-protected and `BLOCK_KEY0` is read/write-protected (masked as `??`). Flash Encryption is still disabled.

### 4.2 Enable Release-mode Flash Encryption and NVS Encryption

Run `idf.py menuconfig` again. Keep the Secure Boot settings above and select:

| Menu / setting | Value |
| :--- | :--- |
| Security features → Enable flash encryption on boot | Enabled |
| Flash Encryption Key Source | eFuse Key Block |
| Size of generated XTS-AES key | AES-128 (256-bit key) |
| Enable usage mode | Release |
| Encrypt contents up to app image length / Check Flash Encryption enabled on app startup | Enabled |
| UART ROM download mode | Permanently switch to Secure mode |
| Component config → NVS → Enable NVS encryption | Enabled |
| Component config → NVS Security Provider → NVS Encryption: Key Protection Scheme | Using Flash Encryption |

The tested SDK defaults to NVS HMAC protection with unset key ID `-1`, causing a build error. Choose **Using Flash Encryption**: NVS generates its keys in `nvs_key`, without another eFuse key.

### 4.3 Check the partition layout and rebuild

The current shared `partitions.csv` is:

```
# Name,     Type, SubType, Offset, Size, Flags
nvs,      data, nvs,     48K,    24K,
nvs_key,  data, nvs_keys, ,      4K,   encrypted
otadata,  data, ota,     ,       8K,
phy_init, data, phy,     ,       4K,
factory,  app,  factory, 128K,   4M,
ota_0,    app,  ota_0,   ,       4M,
ota_1,    app,  ota_1,   ,       4M,
```

Set **Partition Table → Offset of partition table** to `0xb000` (44K), matching `sdkconfig.defaults`. Our bootloader grew to 40K with Secure Boot, then 44K with encryption. If it grows again, move the table and NVS together. This layout also supports unsecured builds.

```bash
idf.py build
```

### 4.4 Flash the signed bootloader, then the application

```bash
idf.py -p /dev/ttyACM0 bootloader-flash
idf.py -p /dev/ttyACM0 flash monitor
```

`build` includes the bootloader. `bootloader-flash` checks it and invokes esptool; it is equivalent to the printed flash command. Ordinary `flash` excludes the bootloader with our settings.

Do not manually reset between commands. First boot encrypts the signed images with the provisioned key, activates Secure Boot and Secure Download Mode, and initializes encrypted NVS. Keep power connected and inspect the boot log.

**Why ordinary `idf.py flash` works for initial provisioning:** Flash Encryption is still off, so the board accepts signed plaintext images. First boot encrypts flash and activates the Release policy: later application writes must contain ciphertext, and ROM download-time encryption is disabled. Ordinary `flash` still supplies plaintext. This is a permanent device-state change, not a one-use command limit; flashing the bootloader again does not reset it.

After activation, Secure Download Mode blocks `efuse-summary`. Query security status with:

```bash
python -m esptool --chip esp32s3 --port /dev/ttyACM0 \
  --no-stub get-security-info
```

Confirm **Secure Boot v2, Flash Encryption, and Secure Download Mode** are enabled before using the encrypted serial-update workflow below.

## 5. Routine builds and updates

### 5.1 Build both firmware files

```bash
idf.py build
```

With Flash Encryption enabled, the post-build step uses your host key and the factory offset from the partition table to produce:

| File | Use |
| :--- | :--- |
| `build/firmware-v0.4.bin` | Signed application; also copied to `ota-server/bin/` when that directory exists |
| `build/firmware-v0.4-factory-encrypted.bin` | Signed application, then encrypted for the factory partition at 128K (`0x20000`) |

Filenames use `v` followed by `FIRMWARE_VERSION` from [firmware.hpp](../components/app_cmn/include/app/cfg/firmware.hpp). Host encryption depends on the destination address. The encrypted factory image must be written at 128K.

### 5.2 Flash through VS Code's custom task

Set the serial port with **ESP-IDF: Select Port to Use**. Add this setting to your local `.vscode/settings.json`, matching the filename to your firmware version:

```json
{
  "idf.customTask": "python -m esptool --chip esp32s3 --port \"${config:idf.port}\" --baud 460800 --no-stub --after hard-reset write-flash --force --flash-size 16MB $((128 * 1024)) \"build/firmware-v0.4-factory-encrypted.bin\""
}
```

The task uses `$((128 * 1024))` because esptool addresses do not accept a literal `128K`. `--no-stub` selects the ROM loader; `--force` permits the prepared ciphertext; `--after hard-reset` restarts the board.

The task updates factory only; it does not change an OTA-selected boot partition.

Tested sequence: **Full Clean → Build Project → Execute Custom Task → Monitor Device**. Full Clean is optional. Update the task's filename or address when the firmware version or factory offset changes.

In a terminal, run the same esptool command, then `idf.py -p /dev/ttyACM0 monitor`. Ordinary `flash monitor` is for initial provisioning before encryption activates.

### 5.3 Continue using signed HTTPS OTA

Serve `ota-server/bin/firmware-v0.4.bin` through the existing OTA server. The ESP verifies the signature and encrypts writes to the selected OTA partition. The host-encrypted factory file is for serial flashing, not this OTA path.

## 6. References

- [Espressif — Secure Boot v2 setup (ESP32-S3)](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/security/secure-boot-v2.html#how-to-enable-secure-boot-v2)
- [Espressif — Host-generated Flash Encryption key](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/security/flash-encryption.html#using-host-generated-key)
- [Espressif — Host-side encryption and serial updates](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/security/flash-encryption.html#manually-encrypting-files)
- [Espressif — Flash Encryption-based NVS Encryption](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/storage/nvs_encryption.html#nvs-encryption-flash-encryption-based-scheme)
- [esptool — Encrypted flash protection and Secure Download Mode](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/esptool/basic-commands.html#encrypted-flash-protection)
- [VS Code ESP-IDF extension — Execute Custom Task setting](https://docs.espressif.com/projects/vscode-esp-idf-extension/en/latest/settings.html#tasks-terminal)
- [HTTPS Server Identity — separate design issue](HTTPS_SERVER_IDENTITY.md)

---

Commands use the project's current firmware version, partition layout and single-board key files.
