# tuyamotd

`tuyamotd` is an OpenWrt daemon that connects a router or embedded device to
Tuya Cloud. It uses UBus to collect system and network statistics, then
publishes them as Tuya device properties over MQTT.

Reports include total/free RAM, uptime, CPU usage, and network interface
traffic and IPv4 addresses. The daemon also supports the Tuya `save_log`
action, which writes messages to the action log.

It exposes MQTT connection state, report counters, and telemetry timing through
UBus:

```sh
ubus call tuyamotd status
```

The OpenWrt package and service integration are maintained in the companion
repository:

<https://github.com/mondaidai/tuyamotd-package/tree/tuya-openwrt-daemon>

Runtime messages are written to syslog under the `tuyamotd` identifier.