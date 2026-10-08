# Étape 3 : connexion au broker cloud

- **Date** : 2026-10-08
- **Tag** : `v0.3-mqtt`

## Objectif

Relier la station à Internet : publier les mesures et les alarmes sur un broker
MQTT dans le cloud, recevoir des commandes, le tout chiffré en TLS, et se
reconnecter seule quand le Wi-Fi ou le broker tombent. Sans jamais que
l'alarme flamme dépende du réseau.

## Ce qui a été fait

- **Broker** : EMQX Serverless, offre gratuite
  ([décision 0005](../decisions/0005-broker-emqx.md)).
- **Protocole** : [`docs/protocol.md`](../protocol.md), le contrat avec le
  site : 5 topics sous `routine/station/`, le format JSON des mesures et des
  événements, 4 commandes, leur validation et leurs règles de sécurité, et 3
  utilisateurs MQTT limités chacun à leurs topics.
- **`src/network`** : Wi-Fi, puis MQTT sur TLS avec vérification du certificat
  du broker ; reconnexion avec des délais croissants ; testament
  `{"online": false}` ; heure par NTP ; les événements d'alarme sont gardés
  pendant une coupure et envoyés au retour
  ([décision 0006](../decisions/0006-pubsubclient.md), PubSubClient).
- **`src/messages`** et **`src/commands`** : les JSON du protocole, et la
  validation stricte des commandes (tout ce qui n'est pas prévu est refusé).
- **`src/alarm`** : test de 3 s et coupure du buzzer, demandés par commande.
  Aucune commande ne peut éteindre une alarme due à une flamme.
- **`src/motor`** : rotation continue sur commande, dans sa propre tâche
  FreeRTOS ([décision 0007](../decisions/0007-moteur-tache-freertos.md)).
- **Secrets** : `include/secrets.h` (Wi-Fi, identifiants MQTT) ignoré par git,
  modèle commité dans `include/secrets.example.h`. Le certificat racine du
  broker, public, est dans `include/broker_ca.h`.
- **Outils de test** : MQTT Explorer sur le PC, avec un utilisateur `debug`
  temporaire ; touche `n` du moniteur série pour simuler une coupure réseau de
  60 s.

## Difficultés rencontrées

- **L'offre gratuite prévue n'existait plus.** HiveMQ Cloud a arrêté son offre
  « Serverless » : le bouton de création menait vers une licence pour héberger
  le broker soi-même. Passage à EMQX Serverless, qui permet en plus des droits
  par topic.
- **Les règles d'autorisation ne s'appliquaient à personne.** Elles avaient été
  saisies dans l'onglet « Client ID » avec les noms d'utilisateurs, alors que la
  station se présente sous l'identifiant `routine-station`. Le broker, en mode
  « liste noire » par défaut, était donc ouvert à tous. Corrigé : règles dans
  l'onglet « Username », puis une règle « tous les utilisateurs : `#`,
  refusé » pour passer en liste blanche.
- **Un Wi-Fi faible.** La station est loin de la box (−68 à −75 dBm), le PC
  est une tour qu'on ne déplace pas, et l'ESP32 n'a que son antenne gravée sur
  la carte. Conséquences mesurées dans les logs :
  - jusqu'à 3 minutes pour rejoindre le Wi-Fi au démarrage ;
  - une tentative de connexion au broker sur deux échouait au bout de 5,0 s
    exactement : quand le premier paquet se perd, l'ESP32 le renvoie après 3 s
    puis 9 s, et la limite de 5 s ne laissait que deux essais ;
  - les connexions tombaient au bout de 30 s, deux fois le délai du « ping »
    de 15 s : la liaison se dégradait, et la station ne s'en rendait compte
    qu'au ping suivant ;
  - pendant chaque reconnexion, `loop()` était bloquée et le moteur se figeait.

  Corrections, sans rien déplacer : 12 s pour ouvrir la connexion (3 essais),
  ping toutes les 30 s, veille de la radio Wi-Fi coupée, relance du Wi-Fi
  toutes les 10 s au lieu de 30 s, moteur dans sa propre tâche.
- **Une coupure réseau sans toucher à la box.** La touche `n` éteint la radio
  de l'ESP32 pendant 60 s sans prévenir le broker, comme une vraie panne.

## Résultat

Compilation réussie :

```
RAM:   [==        ]  24.1% (used 78932 bytes from 327680 bytes)
Flash: [=======   ]  71.1% (used 932509 bytes from 1310720 bytes)
```

La mémoire flash passe de 21 % à 71 % : c'est la pile Wi-Fi et TLS.

| Mesure | Résultat |
|--------|----------|
| Connexion au broker (TLS compris) | 1,5 à 5 s en général |
| Temps connecté, avant les corrections | 70 % de la session |
| Temps connecté, après, une fois le Wi-Fi rejoint | 98 % ; la connexion est encore recyclée toutes les 60 s environ, et rétablie en 1,5 à 5 s |
| Wi-Fi au démarrage | jusqu'à 3 min avec une relance toutes les 30 s ; 3 s au dernier essai avec 10 s |
| Alarme pendant une coupure réseau simulée | réaction en 18,05 ms, comme avec le réseau |
| Retour après la coupure de 60 s | Wi-Fi en 4 s, broker en 2,0 s, sans intervention |

Commandes testées depuis MQTT Explorer :

| Commande | Réponse |
|----------|---------|
| moteur `start` (avant, arrière), `stop` | `ok`, le moteur obéit |
| alarme `test` | `ok`, LED et buzzer 3 s, moteur bloqué |
| moteur `start` pendant un test | `alarm_active` |
| alarme `silence` pendant un test | `ok` |
| direction inconnue, champ en trop | `invalid_command` |
| `hello` | `invalid_json`, avec `"id": null` |

Vérification sur la carte :

- [x] connexion TLS au broker, certificat vérifié
- [x] mesures publiées toutes les 5 s, visibles dans MQTT Explorer
- [x] commandes reçues, validées, exécutées, avec une réponse pour chacune
- [x] reconnexion seule après les coupures, réelles ou simulées
- [x] l'alarme flamme fonctionne pendant une coupure réseau
- [x] le moteur ne se fige plus pendant les reconnexions

## Observations

- **Le briquet dans le noir.** Pièce sombre (photorésistance vers 25 %), le
  briquet fait bondir la lumière visible de +1,8 à +2,25 V. Si la vérification
  de la lumière visible de la
  [décision 0004](../decisions/0004-flamme-ou-lumiere-du-jour.md) restait
  active dans le noir, elle masquerait la flamme : sa désactivation sous 50 %
  est confirmée.
- **La température affichée a gagné environ 1,5 °C** depuis que le Wi-Fi est
  actif (25 °C au lieu de 23,5 °C). Hypothèse : l'ESP32 chauffe et le LM35 est
  juste à côté. À vérifier en éloignant le capteur.

## Limites connues

- **La radio** : la connexion au broker est recyclée environ une fois par
  minute avec ce signal. Les mesures ont alors quelques secondes de trou. Seuls
  un meilleur emplacement ou une antenne externe y remédieraient, ce que les
  contraintes du projet excluent pour l'instant.
- **Le testament arrive après environ 45 s** (1,5 fois le ping de 30 s) : une
  station disparue n'est signalée hors ligne qu'au bout de ce délai.

## Captures

_À ajouter dans `docs/assets/` : une capture de MQTT Explorer avec les
mesures, les commandes et les réponses._
