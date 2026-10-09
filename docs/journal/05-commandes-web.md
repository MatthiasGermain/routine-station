# Étape 5 : les commandes depuis le web

- **Date** : 2026-10-09
- **Tag** : `v0.5-commands`
- **Code** : le firmware dans ce dépôt, la page et la route API dans le dépôt du
  site, [`MatthiasGermain`](https://github.com/MatthiasGermain/MatthiasGermain)

## Objectif

Piloter la station depuis la page `/routine` : faire tourner et arrêter le
moteur, tester l'alarme, couper le buzzer, régler un seuil d'alerte de
température. Seul Matthias, connecté, peut envoyer un ordre ; la station
revalide tout et garde ses propres règles de sécurité.

## Ce qui a été fait

### Firmware : le seuil d'alerte de température

`CLAUDE.md` prévoyait de « régler des seuils » depuis le web. Depuis l'abandon du
capteur de flamme ([décision 0008](../decisions/0008-arret-urgence-tactile.md)),
il n'en restait aucun. Le firmware gagne donc une alerte de température, avec
un seuil réglable par commande : c'est aussi la seule commande qui porte une
valeur numérique, donc la seule où « valeurs dans les bornes » a un sens.

- **La commande** :
  `{"id": "…", "type": "threshold", "action": "set", "temperature_c": 28}`.
  Même forme que les autres (`type` et `action`), un entier de 10 à 40 °C.
  `28.5`, `"28"` ou `true` sont refusés (`invalid_command`), une valeur hors
  bornes aussi (`out_of_range`, nouveau code d'erreur). Elle est acceptée même
  pendant un arrêt d'urgence : changer un seuil n'a rien de dangereux.
- **`src/temperature_alert`**, nouveau module :
  - l'alerte commence quand la température reste au-dessus du seuil pendant
    5 s, et finit quand elle reste 1 °C sous le seuil pendant 5 s. Ce délai et
    cet écart empêchent le bruit du LM35 (quelques dixièmes de degré) de la
    faire clignoter au voisinage du seuil ;
  - la LED bleue soudée sur la carte (GPIO2, déclarée depuis l'étape 0 mais
    inutilisée) s'allume pendant l'alerte : rien à câbler ;
  - le seuil est gardé dans la mémoire flash (NVS, bibliothèque `Preferences`)
    et survit à un redémarrage ; il n'y est réécrit que s'il change, la flash
    s'usant à chaque écriture. Au tout premier démarrage, il vaut 30 °C ;
  - c'est une alerte de **supervision**, pas un arrêt d'urgence : ni buzzer ni
    moteur bloqué. Elle tourne dans `loop()`, pas dans une tâche dédiée : une
    reconnexion au broker peut la retarder de quelques secondes, ce qui est
    sans conséquence ici.
- **Messages** ([`docs/protocol.md`](../protocol.md)) : deux champs dans chaque
  mesure, `temperature_threshold_c` et `temperature_high`, et deux événements,
  `temperature_high` et `temperature_normal`. La page de l'étape 4 ignorait
  déjà les champs et les événements inconnus : le nouveau firmware a pu être
  téléversé avant la mise à jour du site sans rien casser.
- **Broker** : l'utilisateur `web-command` créé dans EMQX, avec un seul droit :
  publier sur `routine/station/commands`.

### Site

Un commit,
[`ef3634c`](https://github.com/MatthiasGermain/MatthiasGermain/commit/ef3634cd71e67fa2ba99299675604ae3ab3c90b6) :

- **`src/lib/station/commands.ts`** : la liste des commandes permises, en miroir
  de [`docs/protocol.md`](../protocol.md), et leur validation stricte (aucun
  champ en plus, seuil entier de 10 à 40) ; les motifs de refus de la station
  en clair (`alarm_active`, `nothing_to_silence`, `out_of_range`…).
- **La route `POST /api/station/command`**, côté serveur
  ([décision 0009](../decisions/0009-session-mot-de-passe-commandes.md)). Avant
  de rien publier, elle refuse : sans session (`401`), d'une autre origine ou
  sans en-tête `Origin` (`403`), autre chose que du JSON (`415`), plus de 1 Ko
  (`413`), une commande non prévue ou hors bornes (`400`). Sinon, elle crée
  l'`id` (16 caractères aléatoires), se connecte en MQTT sur TLS (port 8883,
  certificat vérifié) avec `web-command`, publie en QoS 1, attend l'accusé du
  broker, se déconnecte et répond `202` avec l'`id`. Broker injoignable :
  `502` ; délai dépassé : `504`. Une fonction Vercel est limitée à 10 s : 7 s
  pour la connexion, 9 s au total.
- **La page, vue propriétaire seulement** : boutons Avant, Arrière, Stop,
  Tester l'alarme, Couper le buzzer, et un champ de seuil (10 à 40 °C).
  - Les boutons s'adaptent à l'état : pas de démarrage du moteur ni de test
    pendant une alarme, « Couper » seulement quand l'alarme sonne, tout
    désactivé hors ligne. C'est du confort : la station décide.
  - Chaque commande affiche son retour : « fait », « refusé » avec le motif, ou
    « pas de réponse en 10 s », en rappelant qu'une commande envoyée hors ligne
    est perdue. La réponse de la station arrive par `replies` (abonnement de
    `web-viewer`) et se retrouve par l'`id`.
- **Dans les deux vues** : le seuil en vigueur sous la température, la carte en
  orange pendant une alerte, et les événements de température dans la liste.
  La vue visiteur n'a aucun bouton.
- **Trois variables côté serveur seulement** (type « Sensitive » dans Vercel,
  jamais envoyées au navigateur) : `STATION_MQTT_URL`,
  `STATION_COMMAND_USERNAME` et `STATION_COMMAND_PASSWORD`. L'adresse du broker
  n'est écrite dans aucun dépôt.

## Difficultés rencontrées

- **Ce qu'est un entier en JSON.** ArduinoJson ne reconnaît comme entier qu'une
  valeur écrite sans décimale : `28.0` est un nombre à virgule, donc refusé.
  C'est le comportement voulu (le protocole demande un entier), mais il fallait
  le vérifier dans le code de la bibliothèque et l'écrire dans le protocole,
  pour que le site envoie bien `28`.
- **EMQX Serverless confirme parfois la connexion lentement.** Sept envois de
  « Stop » depuis le PC : le plus souvent 0,25 à 0,7 s, une fois 2,4 s, et une
  fois un échec au bout de 5 s (« connack timeout » : la connexion TLS s'ouvre,
  mais le broker ne confirme pas la session MQTT à temps). Le délai de
  connexion est passé à 7 s. Un dépassement est signalé comme tel (`504`),
  avec le conseil de vérifier l'état de la station avant de réessayer : la
  commande a pu partir.
- **Le contrôle d'origine d'Astro ne couvre que les formulaires**, pas une
  requête JSON : la route vérifie elle-même l'en-tête `Origin`.
- **La réponse de la station peut arriver avant celle de la route** : la
  station répond par le broker pendant que la réponse HTTP revient de Vercel.
  La page garde les réponses arrivées en avance et les rapproche ensuite par
  l'`id`.
- **Le tableau de bord ne défile pas** : avec les boutons, le bandeau prenait
  environ 200 px et le planning se tassait. Sur grand écran, les commandes sont
  sur la ligne de l'état de la station ; sur téléphone, en dessous.

## Résultat

Firmware, testé sur la carte en envoyant les commandes depuis MQTT Explorer
avec l'utilisateur `web-command` :

- [x] au démarrage, le seuil lu en mémoire flash s'affiche (30 °C la première
  fois)
- [x] seuil réglé à 20 °C par 24,4 °C : l'alerte part environ 6 s plus tard
  (5 s de confirmation, plus la seconde entre deux lectures), LED bleue
  allumée ; remonté à 30 °C : fin de l'alerte environ 6 s plus tard
- [x] `45` refusé (`out_of_range`), `28.5` et `"28"` refusés
  (`invalid_command`) ; les commandes moteur mal formées aussi
- [x] seuil accepté pendant un arrêt d'urgence
- [x] seuil réglé à 26 °C, carte redémarrée : 26 °C relu en mémoire flash
- [x] `web-command` fait tourner et arrêter le moteur

Route du site :

- [x] 15 refus, sans aucune connexion au broker : sans session, cookie
  falsifié, autre origine, sans `Origin`, mauvais `Content-Type`, corps de
  2 Ko, JSON illisible, champ en trop, direction inconnue, `id` fourni par le
  navigateur, seuil à `45`, à `28.5`, à `"28"`, tentative de réarmement à
  distance (`alarm reset`), méthode `GET`
- [x] vue visiteur sans aucun bouton, en français et en anglais

De bout en bout, depuis la page :

- [x] moteur dans les deux sens, puis arrêt ; test de l'alarme
- [x] arrêt d'urgence touché, puis « Couper le buzzer » ; « Avant » refusé
  pendant l'arrêt d'urgence, avec le motif affiché
- [x] seuil réglé sous la température : avertissement sur la page et LED bleue
  sur la carte ; remonté : fin de l'alerte
- [x] coupure réseau simulée (touche `n`) : « pas de réponse », et la commande
  ne s'exécute pas au retour du réseau
- [x] délai entre le clic et la réponse « fait » : 1,6 s pour un « Stop »
  mesuré depuis le PC ; en ligne, très variable, d'une demi-seconde à 8 s,
  tant que la fonction Vercel tournait aux États-Unis. Passée à Francfort
  (`fra1`), à côté du broker : nettement plus rapide à l'usage (pas encore
  chiffré)

## Limites connues

- **La latence est instable en ligne** : d'une demi-seconde à 8 s entre le
  clic et « fait », près de la limite de 9 s de la route. Trois causes
  s'additionnent : la fonction Vercel tourne par défaut aux États-Unis alors
  que le broker est à Francfort, chaque commande ouvre une nouvelle connexion
  TLS, et EMQX Serverless confirme parfois la connexion lentement. Les
  fonctions du site tournent désormais à Francfort (`fra1`, réglage du projet
  Vercel, une seule région possible avec l'offre gratuite) : c'est nettement
  mieux. Restent la connexion TLS à chaque commande et les lenteurs
  occasionnelles d'EMQX.
- **Ni double authentification ni limite de tentatives** sur le mot de passe
  ([décision 0009](../decisions/0009-session-mot-de-passe-commandes.md)) : la
  sécurité physique reste sur la carte.
- **Aucun historique des commandes** : rien n'est stocké, ni côté site ni côté
  broker.
- **Une commande envoyée hors ligne est perdue**, par conception : elle ne
  s'exécute jamais plus tard.
- **L'alerte de température passe par `loop()`** : une reconnexion au broker
  peut la retarder de quelques secondes.

## Captures

![Le bandeau Station de la vue propriétaire, avec les commandes](../assets/05-commandes-web.png)

*Le bandeau de la vue propriétaire : l'état de la station, les commandes du
moteur et de l'alarme (« Couper le buzzer » grisé : rien ne sonne), le seuil
réglé à 26 °C, rappelé sous la température.*
