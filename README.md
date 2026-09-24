# tuyamotd

`tuyamotd` is an OpenWrt daemon that connects Tuya Cloud MQTT actions to local
ESP hardware exposed through `ubus`.

Supported actions are:

- `switch_pin` - switch an ESP pin on or off
- `get_measurements` - read temperature and humidity
- `get_devices` - list detected ESP devices

## Usage

Tuya credentials are supplied when starting the daemon:

```sh
tuyamotd -i DEVICE_ID -s DEVICE_SECRET -p PRODUCT_ID
```

Use `-d` to run in the background. Without it, the daemon stays in the
foreground. Logs are sent to syslog.

The daemon reads MQTT and ubus settings from the OpenWrt UCI package
`/etc/config/tuyamotd` and requires a running `ubus` instance with the `ubved`
service available for ESP device operations.

For OpenWrt packaging and installation, see the companion
[tuyamotd-package repository](https://github.com/mondaidai/tuyamotd-package).