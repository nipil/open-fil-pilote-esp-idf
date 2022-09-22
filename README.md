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

L'entrée permettant le réinitialisation du matériel est la GPIO36.

Pour réinitialiser le matériel à sa version usine (la version que vous téléversé via l'outil ESP-IDF)
- Maintenir la GPIO36 à l'état bas, déclencher un redémarrage, et continuer de maintenir la GPIO36 à l'état bas pendant 6 secondes
- Ceci déclenchera le formattage du stockage interne (préférences, les comptes, les plannings, paramètres wifi, etc...)
- Ceci forcera le démarrage du matériel sur la partition "factory" et le microgiciel chargé sur l'ESP via l'outil ESP-IDF

Si vous utilisez ce firmware avec un matériel que vous avez conçu vous-même, ou un kit de développement "nu", en temps normal l'entrée GPIO36 doit être maintenue à l'état haut (via une "pull-up resistor") pour éviter une remise usine accidentelle.
Si vous utiliser un matériel prédéfini (comme les cartes M1E1) ceci est déjà réalisé grâce au bouton CFG/RUN qui pilote l'entrée GPIO36 de manière sécurisée. 

# Over the air update

Vous pouvez téléverser les mises à jour de microgiciel manuellement :

- soit le via l'interface Web
- soit via l'outil CURL

Exemple de commande CURL, pour les utilisateurs avancés :

    curl --silent --show-error --header "Content-Type: application/octet-stream" -X POST --insecure https://admin:admin@adresseip/ofp-api/v1/upgrade --data-binary @firmware.bin

Si possible, vérifiez votre microgiciel avant de le téléverser :

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
