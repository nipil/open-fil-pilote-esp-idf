# open-fil-pilote-esp-idf

Firmware pour les cartes domotiques Open-fil-pilote

# configuration via menuconfig

Certains paramètres compilés sont modifiables v 

    idf.py menuconfig

Puis allez dans les sous menus :

    Component config
        Open Fil Pilote

Les valeurs par défaut sont correctes pour la France et le firmware de base.

# configuration manuelle additionnelle

Voir le document certs/README.md pour la génération des certificats

# extensions

Installer l'exension ESP-IDF
Installer le framework ESP-IDF via l'extension
Configurer l'analyseur de code de l'éditeur via la commande de palette "ESP-IDP: Add vscode configuration folder"

# factory reset

Hold GPIO36 low, trigger reboot, continue to hold GPIO36 low for 5 seconds or more
This formats the NVS flash partition, and sets the factory partition (where the firmware is flashed) as boot
This way, you revert everything to the default of the flashed firmware.

# Over the air update

Verify firmware prior to publishing it :

    python C:\esp\esp-idf\components\esptool_py\esptool\esptool.py --chip esp32 image_info firmware.bin

    esptool.py v3.3.2-dev
    Image version: 1
    Entry point: 400810b4
    6 segments

    Segment 1: len 0x07de8 load 0x3f400020 file_offs 0x00000018 [DROM]
    Segment 2: len 0x02340 load 0x3ffb0000 file_offs 0x00007e08 [BYTE_ACCESSIBLE,DRAM]
    Segment 3: len 0x05ec0 load 0x40080000 file_offs 0x0000a150 [IRAM]
    Segment 4: len 0x14ee0 load 0x400d0020 file_offs 0x00010018 [IROM]
    Segment 5: len 0x0548c load 0x40085ec0 file_offs 0x00024f00 [IRAM]
    Segment 6: len 0x00010 load 0x50000000 file_offs 0x0002a394 [RTC_DATA]
    Checksum: 99 (valid)
    Validation Hash: faf99c858c49b46ebe3a26c46e4eaae676333ff64cfe6594b8109530fd10b288 (valid)

# roadmap

- certificate update API
- certificate fallback if custom certs fail
- firmware update
