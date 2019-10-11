# Linux BLE library

## run example

```bash
./run.sh <EXAMPLE_NAME>

e.g.
./run.sh scan_ble
```

## reopen bluetooth if occur error

https://stackoverflow.com/a/23059924

```bash
sudo hciconfig hci0 down
sudo hciconfig hci0 up
service bluetooth restart
```
