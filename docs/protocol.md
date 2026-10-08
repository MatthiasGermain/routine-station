# Protocole ESP32 ↔ web

Ce document est le contrat entre le firmware de la station et la section
« Station » de la page `/routine` du site (repo `portfolio`) : topics MQTT,
format JSON des mesures et des commandes.
C'est la référence commune aux deux dépôts. Si le firmware et le site ne sont
pas d'accord, c'est ce fichier qui a raison.

> Rempli à l'étape 3 (connexion au broker cloud). Les sections ci-dessous
> fixent seulement le plan du document.

## Broker et accès

_À venir : adresse, ports, TLS, les deux accès (lecture seule, commande)._

## Topics

_À venir : liste des topics, sens de circulation, qualité de service._

## Mesures (ESP32 → web)

_À venir : format JSON, unités, fréquence d'envoi._

## Commandes (web → ESP32)

_À venir : types de commande autorisés, bornes des valeurs, réponse de
l'ESP32._

## État de la station

_À venir : comment la page sait que la station est en ligne ou hors ligne._
