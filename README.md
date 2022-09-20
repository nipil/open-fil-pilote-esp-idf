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

# roadmap

- add partial reload buttons
- certificate update API
- certificate fallback if custom certs fail
- custom partition
- firmware update
