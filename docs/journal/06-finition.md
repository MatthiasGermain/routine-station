# Étape 6 : finition

- **Date** : 2026-10-09
- **Tag** : `v1.0`
- **Code** : le firmware dans ce dépôt, les routes et les graphes dans le dépôt
  du site, [`MatthiasGermain`](https://github.com/MatthiasGermain/MatthiasGermain)

## Objectif

Qu'un visiteur comprenne le projet en 30 secondes et le voie fonctionner : des
graphes de l'historique sur la page, une démo animée, des photos, un README
complet et un bilan.

## Ce qui a été fait

### L'historique des mesures, sur le long terme

L'idée de départ, 24 h gardées dans la mémoire de l'ESP32, a été abandonnée :
Matthias voulait un historique sur des mois. Les échantillons passent donc par
le broker jusqu'à une base Postgres, sans serveur à héberger
([décision 0010](../decisions/0010-historique-supabase.md), contrat dans
[`docs/protocol.md`](../protocol.md)).

**Firmware** :

- `src/samples`, nouveau module : chaque seconde, une lecture des capteurs ;
  toutes les 5 min, la moyenne de la tranche, calée sur l'horloge (10:00,
  10:05…), publiée sur `routine/station/samples`. Rien tant que l'heure n'est
  pas connue ; une tranche entamée moins d'une minute avant sa fin est
  abandonnée.
- `src/network` : une file de 24 échantillons (2 h) pendant les coupures ;
  pleine, elle oublie les plus anciens. La file des événements et celle des
  échantillons se vident par la même fonction.
- `src/main.cpp` : une seule lecture par seconde nourrit l'alerte de
  température et les moyennes.

**Broker et base** :

- EMQX : le droit pour `station` de publier sur `samples`, et une règle
  (`SELECT payload FROM "routine/station/samples"`) qui envoie chaque
  échantillon au site par un connecteur « HTTP Server », avec un jeton secret.
- Supabase, créé depuis l'onglet Storage de Vercel, région de Francfort : une
  table `station_samples` dont l'heure est la clé, la sécurité au niveau des
  lignes activée sans aucune règle d'accès.

**Site**, en quatre commits :

1. [`65b495a`](https://github.com/MatthiasGermain/MatthiasGermain/commit/65b495a258a77bb1581d05c421273d6b0c9d45f0),
   **l'enregistrement** : `POST /api/station/samples`, appelée par EMQX. Jeton
   comparé en temps constant (`401`), 1 Ko au plus (`413`), JSON seulement
   (`415`), validation stricte (`400`) : exactement `time`, `temperature_c`,
   `light_pct`, heure UTC ronde de 5 min, au plus 5 min dans le futur et 7 jours
   dans le passé, valeurs dans les bornes. Insertion idempotente : un
   échantillon reçu deux fois n'est gardé qu'une fois.
2. [`d331fd7`](https://github.com/MatthiasGermain/MatthiasGermain/commit/d331fd7dc68ef1600736c4d4ed429012d7e63e8e),
   **la lecture** : `GET /api/station/history?range=24h|7d|30d|1y`, publique.
   Échantillons bruts sur 24 h, moyennes par heure sur 7 et 30 jours, par jour
   de l'heure de Paris sur un an ; pour chaque point, la moyenne, le minimum et
   le maximum de la température, la moyenne de la lumière. Mise en cache 5 min
   par Vercel ; tout autre paramètre est refusé, pour qu'une adresse inventée
   ne contourne pas le cache.
3. [`d884da6`](https://github.com/MatthiasGermain/MatthiasGermain/commit/d884da6e5d9be3a6549b1236860d81e17ab212a9),
   **un correctif** : une requête à la fois vers la base (voir les
   difficultés).
4. [`eb87a06`](https://github.com/MatthiasGermain/MatthiasGermain/commit/eb87a0603b498fd38180e3022a024d1ecfa9143c),
   **les graphes**, dans la vue visiteur seulement (`/routine` et
   `/en/routine`) :
   - deux graphes en SVG dessiné à la main, sans bibliothèque (3,8 Ko
     compressés) : la température, avec une bande du minimum au maximum sur les
     longues périodes et la ligne du seuil en vigueur en pointillés ; la
     lumière ;
   - un sélecteur 24 h / 7 jours / 30 jours / 1 an ; l'axe du temps couvre
     toute la période, si bien qu'un historique encore court se voit comme tel ;
   - pas de trait à travers un trou de données (station éteinte, coupure) ;
   - au survol ou au toucher, la tranche et sa valeur sur les deux graphes ;
   - des états honnêtes : historique en cours de constitution, aucune mesure
     sur la période, historique indisponible ;
   - un résumé texte de chaque graphe pour les lecteurs d'écran.

### Démo, photos, README et bilan

_À compléter._

## Difficultés rencontrées

- **Les échantillons disparaissaient sans erreur.** La station publiait
  (`history sample 20:15 UTC` dans le moniteur série), mais rien n'arrivait en
  base : `station` n'avait pas encore le droit de publier sur `samples`. Avec
  des droits en liste blanche, le broker jette la publication sans rien dire ;
  en QoS 0, la station n'a aucun accusé de réception et la croit partie. Les
  échantillons d'avant la règle sont perdus ; le suivant (20:30 UTC) est
  arrivé aussitôt. Leçon, écrite dans le protocole : une nouvelle publication
  commence par sa règle d'autorisation.
- **Le pooler de Supabase se bloquait quand deux requêtes se chevauchaient.**
  Les graphes restaient sur « Chargement… ». Reproduit hors du site : deux
  lectures simultanées sur la même connexion, la première répond en 0,4 s, la
  seconde jamais, et toutes les suivantes restent bloquées derrière elle. La
  bibliothèque envoie les requêtes à la suite sans attendre les réponses, et le
  pooler, en mode transaction, perd le fil. En production, un visiteur au
  moment d'un enregistrement aurait suffi. Correctif : une requête à la fois ;
  10 lectures simultanées passent alors en 1,3 à 1,6 s. Garde-fous : un client
  sans réponse en 8 s est remplacé, et la page abandonne au bout de 15 s en le
  disant.
- **L'arrondi des moyennes** : la température est stockée en `real`, où 24,9
  vaut 24,8999996 ; la moyenne de 25,0 et 24,9 donnait 24,9. Conversion en
  `numeric` avant la moyenne.
- **L'adresse fournie par l'intégration Vercel** contient des paramètres que la
  bibliothèque ne comprend pas tous : le site n'en garde que l'hôte, le compte
  et la base, et impose lui-même le chiffrement.

## Résultat

Historique :

- [x] la station publie un échantillon toutes les 5 min (premier vu dans le
  moniteur série : 19:50 UTC, 24,4 °C, 31 %)
- [x] route d'enregistrement : 13 cas refusés sans toucher la base (sans jeton,
  mauvais jeton, mauvais type, 2 Ko, JSON illisible, champ en trop, heure pas
  ronde ou dans le futur, valeurs hors bornes, `GET`) ; un échantillon envoyé
  deux fois : une seule ligne
- [x] de bout en bout : premier échantillon en base au plus 14 s après sa
  publication, le suivant au plus 3 s ; six échantillons d'affilée sans trou
- [x] coupure réelle du Wi-Fi vers 22:42 (signal à −73 dBm) : la page affiche
  la station hors ligne, et l'échantillon de la tranche en cours arrive quand
  même
- [x] route de lecture : les quatre périodes, les moyennes par jour calées sur
  minuit à Paris ; en production, la première lecture va à la base, les
  suivantes sont servies par le cache de Vercel
- [x] graphes : données réelles, une année simulée avec des trous, seuil hors
  du graphe, états sans données, téléphone (375 px), anglais

_À compléter : démo, README, bilan._

## Limites connues

- **Un échantillon refusé ou non enregistré est perdu** : ni la station (QoS
  0) ni EMQX ne le renvoient.
- **2 h d'échantillons au plus** en attente pendant une coupure ; au-delà, les
  plus anciens sont oubliés.
- **Supabase met en pause** un projet gratuit inactif depuis 7 jours : si la
  station reste éteinte plus d'une semaine, il faudra relancer la base depuis
  son tableau de bord.
- **L'historique commence le 9 octobre 2026** au soir : les graphes sur 30
  jours et un an se rempliront avec le temps.

## Captures

_À ajouter : les graphes après au moins une journée de données, et le GIF de
démo._
