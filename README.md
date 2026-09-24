# tuyamotd

Small Tuya IoT monitoring daemon that sends system stats to the cloud.

## Build

From the repository root:

```sh
make
```

The executable is written to `build/tuyamotd`.

## Run

```sh
./build/tuyamotd \
  --device-id DEVICE_ID \
  --device-secret DEVICE_SECRET \
  --product-id PRODUCT_ID
```

Add `--daemon` to run in the background.

## What it sends

Every 15 seconds, the daemon reports:

- total and free RAM
- system uptime
- CPU usage
- IPv4 network interfaces, including address, netmask, received bytes, and transmitted bytes

Incoming Tuya action payloads are appended to `/tmp/tuya_action.log`.

