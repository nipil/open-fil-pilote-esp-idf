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

Si possible, vérifiez votre microgiciel à l'aide du framework ESP-IDF (si installé) avant de le téléverser :

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

# troubleshooting

## Lenteurs de la première connexion en HTTPS

C'est normal, l'ESP32 est un super microcontrôleur mais pas une bête de course !

Pour l'établissement de la connexion TLS, on constate :

- 1.7 secondes incompressibles avec un certificat 1024 bits (faiblement sécurisé)
- 3.5 secondes incompressibles avec un certificat 2048 bits (bien sécurisé)
- 7.2 secondes incompressibles avec un certificat 4096 bits (ultra sécurisé)

Ensuite, tout dépend du mode d'accès :

- via le navigateur, comme il y a réutilisation implicite des connexions ("keep-alive"), les requêtes suivantes via le navigateurs sont bien plus rapides
- via CURL ou similaire, par défaut, chaque commande CURL établit une nouvelle connexion, donc devra attendre quelques secondes à chaque fois. Cependant si vous enchainez toutes vos URL dans la même commande CURL, alors la commande CURL réutilisera aussi la même connexion ("keep-alive")

## Conversion de certificats et clés privés en un "bundle"

L'objectif est d'avoir une clé privée au format texte PEM et à la structure PKCS#8.

Pour un certificat/clé source au format PKCS#12 (.p12) :

    openssl pkcs12 -in input.p12 -out output.key -nodes -nocerts
    openssl pkcs12 -in input.p12 -out output.crt -nodes -nokeys -chain

Pour un certificat/clé source déjà au formats PEM (.crt, .key ou .pem) assurez vous d'avoir le bon format :

    openssl pkcs8 -in input.key -topk8 -nocrypt -outform pem -out output.key
    openssl x509 -in input.crt -outform pem -out output.crt

Vérifier ensuite que les marqueurs de début/fin sont corrects :

- votre clé privée (output.key) contient bien "BEGIN PRIVATE KEY" et "END PRIVATE KEY" (PKCS#8)
- chacun de vos certificats (.crt) contient bien "BEGIN CERTIFICATE" et "END CERTIFICATE" (RFC 7468 section 4)

Finalement, regroupez le contenu de tous ces fichiers dans un même fichier texte :

- en commençant par le certificat du serveur
- puis éventuellement ceux des autorités intermédiaires
- puis celui de l'autorité racine
- puis la clé privée (non chiffrée !)

Et téléversez le fichier résultant vers le microgiciel (soit par la WebUI, soit par l'API, soit par une recompilation).

Après redémarrage, le microcontrôleur devrait présenter le certificat fourni.


## Erreur: "ERR_SSL_VERSION_OR_CIPHER_MISMATCH" ou similaire

Par exemple Chrome affiche "Le client et le serveur ne sont pas compatibles avec une version de protocole ou une méthode de chiffrement SSL commune"

Dans les logs du moniteur, ça se traduit par :

    E (297207) esp-tls-mbedtls: mbedtls_ssl_handshake returned -0x6980 with custom certificate`

Cela peut signifer que dans le "bundle" de certificat fourni (soit à la compilation, soit via la WebUI soit via l'API) ne présente pas le certificat qui dépend de la clé privée *en premier* dans la liste du bundle.

Solution: respecter les consignes indiquées dans la WebUI pour la construction du bundle !

## Erreur: "ERR_CONNECTION_RESET" ou similaire

Par exemple Chrome affiche "Connexion réinitialisée"

Dans les logs du moniteur, ça se traduit par :

    I (52720) esp_https_server: performing session handshake
    E (52750) esp-tls-mbedtls: mbedtls_pk_parse_keyfile returned -0x3C00
    E (52750) esp-tls-mbedtls: Failed to set server pki context
    E (52750) esp-tls-mbedtls: Failed to set server configurations, returned [0x8019] (ESP_ERR_MBEDTLS_PK_PARSE_KEY_FAILED)
    E (52760) esp-tls-mbedtls: create_ssl_handle failed, returned [0x8019] (ESP_ERR_MBEDTLS_PK_PARSE_KEY_FAILED)
    E (52780) esp_https_server: esp_tls_create_server_session failed
    W (52790) httpd: httpd_accept_conn: session creation failed
    W (52790) httpd: httpd_server: error accepting new connection

Raison : la clé privée du certificat HTTPS a été fournie chiffrée, alors qu'elle doit être en clair.

Solution: respecter les consignes indiquées dans la WebUI pour la construction du bundle !

## Erreur: "NET::ERR_CERT_AUTHORITY_INVALID" ou similaire

Par exemple Chrome affiche "Votre connexion n'est pas privée"

Dans les logs du moniteur, ça se traduit par :

    E (36310) esp-tls-mbedtls: mbedtls_ssl_handshake returned -0x7780
    E (36310) esp_https_server: esp_tls_create_server_session failed
    W (36320) httpd: httpd_accept_conn: session creation failed
    W (36330) httpd: httpd_server: error accepting new connection

Raison : le certificat utilisé est un certificat autosigné. Il n'y a rien à faire si ce n'est soit accepter l'exception de sécurité dans vos navigateurs, ou bien obtenir un certificat reconnu, et le téléverser via la WebUI ou l'API.

## Clé privée invalide lors du chargement d'un bundle de certificats

La clé privée doit être au format PEM, 

Solution: respecter les consignes indiquées dans la WebUI pour la construction du bundle !
