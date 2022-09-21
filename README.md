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

# roadmap

- certificate update API
- certificate fallback if custom certs fail
- firmware update
