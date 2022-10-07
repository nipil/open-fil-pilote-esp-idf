# A quoi ça sert

Ca remplit deux objectifs

- créer un certificat autosigné à chaque compilation, pour que chaque build contient un certificat différent
- compiler un certificat dans le microgiciel, pour qu'il y ait toujours une solution de repli en cas de "reset usine"

Merci de porter attention aux points suivants :

- ne pas essayer de remplacer ou de supprimer ce certificat autosigné, ou vous allez casser le "reset usine"
- ne pas changer les noms de fichier (`autosign.crt` and `autosign.key`) car ils sont référencés dans le code source (`webserver.c` and `api_mgmt.c`)

Une info importante sur les certificats auto-signés et la notion de confiance

- dans tous les cas les navigateurs vont générer une alerte (qu'il est possible d'accepter pour la suite)
- l'ajout d'une information "ip" en plus du nom ne rajoute pas grand chose, mais c'est possible si vous le souhaitez
- dans tous les cas c'est un certificat non signé, il n'y a aucune confiance possible

Dans tous les cas, sachez que vous pouvez changer de certificat ultérieurement, que ça soit par la WebUI ou l'API.

# Noms et IP

Le fichier `info.txt` contient les informations utilisées pour générer le certificat autosigné

- la première ligne est obligatoire et donne le nom du certificat
- les lignes suivantes sont optionnelles, et listent les adresses IP à inclure dans le certificat

# Exécution automatique

La génération du certificat autosigné est automatique dans le processus de construction du microgiciel.

Si l'un ou l'autre fichier `autosign.*` n'est pas présent, il sera généré grâce au fichier `info.txt`.

# Exécution manuelle

Si vous tenez absolument à créer le certificat, il est aussi possible de le générer manuellement.

Ouvrez un terminal ESP-IDF, et déplacez vous dans le présent répertoire, et exécutez

    python gen.py

Deux fichiers sont générés:

- autosign.crt
- autosign.key

# Vérifications

Vous pouvez utiliser une commande `openssl` pour vérifier les infos dans le certificat:

    openssl x509 -in autosign.crt -noout -text

NOTE: `openssl` n'est pas fourni dans le framework ESP-IDF, il faut l'installer vous même.

# Examples

Example 1: `info.txt`

    openfilpilote.local

Résultat 2:

    openssl x509 -in autosign.crt -noout -text
    
    Certificate:
        Data:
            Version: 3 (0x2)
            Serial Number: 1000 (0x3e8)
            Signature Algorithm: sha256WithRSAEncryption
            Issuer: CN = openfilpilote.local
            Validity
                Not Before: Oct  6 11:26:32 2022 GMT
                Not After : Oct  3 11:26:32 2032 GMT
            Subject: CN = openfilpilote.local
            ...
            X509v3 extensions:
                ...
                X509v3 Subject Alternative Name:
                    DNS:openfilpilote.local

Example 2: `info.txt`

    openfilpilote.local
    192.168.1.24
    192.168.3.48

Résultat 2:

    openssl x509 -in autosign.crt -noout -text

    Certificate:
        Data:
            Version: 3 (0x2)
            Serial Number: 1000 (0x3e8)
            Signature Algorithm: sha256WithRSAEncryption
            Issuer: CN = openfilpilote.local
            Validity
                Not Before: Oct  6 11:32:25 2022 GMT
                Not After : Oct  3 11:32:25 2032 GMT
            Subject: CN = openfilpilote.local
            ...
            X509v3 extensions:
                ...
                X509v3 Subject Alternative Name:
                    DNS:openfilpilote.local, DNS:192.168.1.24, IP Address:192.168.1.24, DNS:192.168.3.48, IP Address:192.168.3.48
