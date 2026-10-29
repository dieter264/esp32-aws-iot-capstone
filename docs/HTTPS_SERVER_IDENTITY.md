# ESP32-S3 HTTPS Server Identity

Capstone v0.4 — Browser Trust & Device Impersonation

Target: ESP32-S3 SoftAP at https://192.168.4.1/config

## 1. Separate security objective

**Ensure the browser recognizes the genuine ESP32-S3 HTTPS server and rejects an impersonating board.** This is a separate security workstream from [ESP32-S3 Hardware Security](ESP32-S3_HARDWARE_SECURITY.md).

An attacker with physical access could replace the board, advertise a look-alike Wi-Fi access point, and serve an identical configuration page using the public source code. The attacker would not thereby obtain the original board's encrypted credentials, but could try to trick the operator into *re-entering* credentials.

> **Crucial distinction**
>
> **Secure Boot and Flash Encryption authenticate and protect the original device's firmware; they do not let the browser recognize whether the physical board has been replaced.** Correct HTTPS certificate validation is needed for that.

## 2. Project integration

| Area | Current state |
| :--- | :--- |
| HTTPS endpoint | `https://192.168.4.1/config` via the ESP32 SoftAP |
| Web content | `config.html` resides in `app_https_server` and is embedded into the firmware binary at build time via `asm()` |
| Server certificate and key | Created before the build, not in the public repository, and embedded in the application via `asm()` |
| Client trust | Configure the operating system or browser to trust the intended public certificate or issuing CA |

The basic certificate-generation command is in [README 6.1](../README.md#61-create-the-devices-https-certificate). Inspect the certificate actually used by your build; its subject, extensions and validity depend on how it was generated.

A manually bypassed certificate warning does **not** establish the identity of the genuine server. Configure trust before entering credentials.

## 3. Required identity verification

The browser must verify all of the following before credentials are entered:

1. **Trust:** The server certificate chains to a trusted issuer or is the explicitly trusted server certificate.
2. **Address match:** For the project's default URL, the leaf certificate must include `subjectAltName = IP:192.168.4.1`. A Common Name (CN) alone is not an adequate substitute. If you change the address or use a hostname, use the corresponding IP or DNS SAN.
3. **Private-key possession:** The TLS server demonstrates possession of the private key corresponding to the presented certificate.
4. **Other checks:** The certificate must be valid for TLS server use, within its validity interval, and accepted by the browser's certificate-validation policy.

The `/config` URL path does not participate in TLS identity matching. A replacement device cannot impersonate the trusted HTTPS identity merely by copying the open-source HTML or choosing the same Wi-Fi SSID and IP address.

> **User-operation rule**
>
> **Never enter Wi-Fi, MQTT, or other sensitive credentials after bypassing a browser certificate warning.** Treat a new or unexpected warning as evidence of a changed certificate, misconfiguration, or possible device impersonation until independently investigated.

## 4. Certificate trust options

| Option | Properties |
| :--- | :--- |
| **A. Explicitly trust one server certificate** | Suitable for a small setup if supported by the client. Import the original public certificate from the trusted build environment. If the certificate is replaced or renewed, review/update client trust. |
| **B. Private Certificate Authority (preferred for renewal)** | Create a private CA, trust its public CA certificate on each client, and issue a device-specific server certificate for the device's address. Renew the leaf without re-trusting every renewal, provided the issuer and identity remain trusted. |

For the private CA approach, use a unique TLS server private key for each device. Keep the *CA private key* in the trusted development environment only; it must never be embedded in firmware or committed to the repository. Only issue certificates for devices you control. Trusting the CA means trusting its certificate issuance policy, so the CA must not issue interchangeable certificates for unknown replacement boards.

Test the chosen trust mechanism in each supported browser; neither option requires rewriting `app_https_server`.

## 5. Certificate generation requirements

For the default device address and an RSA server key, the leaf certificate needs:

```ini
subjectAltName = IP:192.168.4.1
basicConstraints = critical,CA:FALSE
keyUsage = critical,digitalSignature,keyEncipherment
extendedKeyUsage = serverAuth
```

Choose a renewal interval appropriate to the deployment (for example, one year). Reuse the device's certificate and key between builds; replace or renew them deliberately.

Run the following generation and inspection commands in a Linux shell **from the project root**. This is the build environment; the browser client may be a different computer running any operating system.

### Example: inspect the project's certificate

```bash
openssl x509 -in components/app_https_server/certs/server.crt -noout \
  -subject -dates -ext subjectAltName,extendedKeyUsage
```

### Example: private CA and device certificate

The commands below create the CA files in `security/ca_secrets/` and the server files directly in `components/app_https_server/certs/`. Both directories must be excluded from Git. Replace `192.168.4.1` if the device uses a different address. The CA commands prompt for a passphrase to protect `ca.key`.

```bash
mkdir -p security/ca_secrets components/app_https_server/certs

# Create a private CA with an encrypted private key.
openssl genpkey -algorithm RSA -aes-256-cbc \
  -pkeyopt rsa_keygen_bits:3072 -out security/ca_secrets/ca.key
openssl req -x509 -new -sha256 -days 3650 \
  -key security/ca_secrets/ca.key -out security/ca_secrets/ca.crt \
  -subj "/CN=ESP32-S3 Private CA" \
  -addext "basicConstraints=critical,CA:TRUE" \
  -addext "keyUsage=critical,keyCertSign,cRLSign"

# Create a device-specific server key and CSR.
openssl genpkey -algorithm RSA \
  -pkeyopt rsa_keygen_bits:2048 -out components/app_https_server/certs/server.key
openssl req -new -key components/app_https_server/certs/server.key \
  -out components/app_https_server/certs/server.csr \
  -subj "/CN=esp32-config"

# Create the server certificate extensions file.
cat > components/app_https_server/certs/server-ext.cnf <<'EOF'
basicConstraints = critical,CA:FALSE
keyUsage = critical,digitalSignature,keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = IP:192.168.4.1
EOF

openssl x509 -req -in components/app_https_server/certs/server.csr \
  -CA security/ca_secrets/ca.crt -CAkey security/ca_secrets/ca.key \
  -CAserial security/ca_secrets/ca.srl -CAcreateserial \
  -out components/app_https_server/certs/server.crt \
  -days 365 -sha256 -extfile components/app_https_server/certs/server-ext.cnf
```

Generate the CA once and retain it for future certificate issuance. The server files are already in the build's expected location; rebuild to embed them. **Do not embed `ca.key`.**

### Client trust: the computer running the browser

The **client computer** is the host of the browser that opens the ESP configuration page. It may run macOS, Windows, Linux, or another OS, independently of the build environment. It needs no project checkout or ESP-IDF installation.

For the private CA approach, transfer only the public `security/ca_secrets/ca.crt` to that computer through a trusted route, then configure certificate trust there. Neither the CA private key nor the server private key is needed on the browser host.

OS-specific examples:

- **macOS:** Import `ca.crt` into Keychain Access and configure its trust for TLS server authentication; see [Apple's certificate trust settings](https://support.apple.com/guide/keychain-access/kyca11871/mac).
- **Windows:** Import `ca.crt` into the **Trusted Root Certification Authorities** store; see [Microsoft's certificate-store guide](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/trusted-root-certification-authorities-certificate-store).
- **Linux:** Follow the distribution's trust-store procedure, such as the Ubuntu/Debian example below.

### Example: trust the private CA on Ubuntu/Debian

Only for an Ubuntu/Debian **browser host**, with the `ca-certificates` package installed: add the public CA certificate to the system trust store. This example runs from the project root on that host; if the build runs elsewhere, replace the source path with the location of the transferred public certificate.

```bash
sudo cp security/ca_secrets/ca.crt /usr/local/share/ca-certificates/esp32-local-ca.crt
sudo update-ca-certificates
```

This example follows [Ubuntu's CA installation procedure](https://ubuntu.com/server/docs/how-to/security/install-a-root-ca-certificate-in-the-trust-store/). Other Linux distributions may use different commands. Install only the public `ca.crt`; neither private key is needed to establish client trust.

Browsers may use a separate certificate store, especially when packaged in a sandbox. Check the browser's trust settings and test the connection rather than assuming a system-store update is sufficient. For example, [Firefox documents CA setup on Linux](https://support.mozilla.org/en-US/kb/setting-certificate-authorities-firefox#linux).

## 6. Validation checklist and security limits

1. Inspect the generated certificate and confirm a **SAN matching the configured address or hostname**, server-auth purpose, validity, and matching private key.
2. Rebuild the firmware after provisioning the new HTTPS certificate and key; confirm the embedded certificate is the one expected.
3. Import and trust the appropriate *public* certificate (trusted CA root or individual self-signed server leaf) on each client.
4. Visit `https://192.168.4.1/config` in each supported browser and verify it opens **without bypassing a certificate warning**. Use the configured device address if different.
5. **Negative test:** connect to a deliberately different, untrusted test certificate for the same address and verify the browser rejects or warns rather than silently accepting it.
6. Record certificate renewal, backup, recovery and key-rotation procedures. Do not let every firmware build silently replace the device identity.

**Scope:** HTTPS server identity helps detect network and whole-device impersonation. It cannot stop physical theft, hardware destruction, a compromised client computer, or disclosure of secrets through application flaws. Device-specific MQTT authentication is a separate concern.

> **Implementation status**
>
> Certificate generation is supported by the project. Client trust setup and browser validation are manual deployment steps; a successful firmware build alone does not confirm server identity verification.

## 7. References

- [ESP32-S3 Hardware Security — companion architecture document](ESP32-S3_HARDWARE_SECURITY.md)
- [Apple — Change certificate trust settings in Keychain Access](https://support.apple.com/guide/keychain-access/kyca11871/mac)
- [Microsoft — Trusted Root Certification Authorities store](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/trusted-root-certification-authorities-certificate-store)
- [Ubuntu — Install a root CA certificate in the trust store](https://ubuntu.com/server/docs/how-to/security/install-a-root-ca-certificate-in-the-trust-store/)
- [Firefox — Set up Certificate Authorities, including Linux examples](https://support.mozilla.org/en-US/kb/setting-certificate-authorities-firefox)
- [OpenSSL — req command](https://docs.openssl.org/master/man1/openssl-req/)
- [OpenSSL — x509 command](https://docs.openssl.org/master/man1/openssl-x509/)
- [Espressif — Secure Boot v2](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/security/secure-boot-v2.html)
