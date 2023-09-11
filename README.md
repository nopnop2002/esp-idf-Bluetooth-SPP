# esp-idf-Bluetooth-SPP
Classic Bluetooth SPP example for esp-idf.   
You can communicate in Classic Bluetooth SPP Profile using 2 of ESP32.   

I modified from this.   

https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/bluedroid/classic_bt/bt_spp_acceptor

https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/bluedroid/classic_bt/bt_spp_initiator


# Software requirement    
ESP-IDF V4.4/V5.x.   

# Acceptor for M5Stack


```
git clone https://github.com/nopnop2002/esp-idf-Bluetooth-SPP
cd esp-idf-Bluetooth-SPP/bt_spp_acceptor/
idf.py menuconfig
idf.py flash
```

\*There is no MENU ITEM where this application is peculiar.   


# Initiator for M5Stick

```
git clone https://github.com/nopnop2002/esp-idf-Bluetooth-SPP
cd esp-idf-Bluetooth-SPP/bt_spp_initiator_Stick/
idf.py menuconfig
idf.py flash
```

\*There is no MENU ITEM where this application is peculiar.   


# Initiator for M5StickC

```
git clone https://github.com/nopnop2002/esp-idf-Bluetooth-SPP
cd esp-idf-Bluetooth-SPP/bt_spp_initiator_StickC/
idf.py menuconfig
idf.py flash -b 115200
```

\*There is no MENU ITEM where this application is peculiar.   

__You need to specify Baud rate for flashing.__   


# Initiator for M5StickC+

```
git clone https://github.com/nopnop2002/esp-idf-Bluetooth-SPP
cd esp-idf-Bluetooth-SPP/bt_spp_initiator_StickC+/
idf.py menuconfig
idf.py flash -b 115200
```

\*There is no MENU ITEM where this application is peculiar.   

__You need to specify Baud rate for flashing.__   

