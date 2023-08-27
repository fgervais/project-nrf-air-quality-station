# Air Quality Station

![overview](assets/img/overview.jpg)

# Project Management

## Init

```bash
mkdir project-nrf-air-quality-station
cd project-nrf-air-quality-station
docker run --rm -u $(id -u):$(id -g) -v $(pwd):/new -w /new -e ZEPHYR_BASE="" nordicplayground/nrfconnect-sdk:v2.4-branch \
        bash -c "west init -m https://github.com/fgervais/project-nrf-air-quality-station.git . && west update"
```

## Build

```bash
cd application
docker compose run --rm nrf west build -b my_board -s app
```

## menuconfig

```bash
cd application
docker compose run --rm nrf west build -b my_board -s app -t menuconfig
```

## Clean

```bash
cd application
rm -rf build/
```

## Update

```bash
cd application
docker compose run --rm nrf west update
```

## Flash

### nrfjprog
```bash
cd application
docker compose -f docker-compose.yml -f docker-compose.device.yml \
        run nrf west flash
```

### pyocd
```bash
cd application
pyocd flash -e sector -t nrf52840 -f 4000000 build/zephyr/zephyr.hex
```

# Hardware

https://github.com/fgervais/project-nrf-air-quality-station_hardware
