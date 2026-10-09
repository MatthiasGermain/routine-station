# Étape 4 : la page `/routine` en direct

- **Date** : 2026-10-09
- **Tag** : `v0.4-live-page`
- **Code** : dans le dépôt du site,
  [`MatthiasGermain`](https://github.com/MatthiasGermain/MatthiasGermain)
- **Page** : https://matthias-germain.vercel.app/routine

## Objectif

Afficher la station en direct sur la page `/routine` du site, sans exposer le
tableau de bord personnel qu'elle contenait déjà (planning, tâches Notion), et
sans qu'un visiteur puisse rien envoyer à la station.

## Ce qui a été fait

Côté site, en trois commits :

1. [`a510be2`](https://github.com/MatthiasGermain/MatthiasGermain/commit/a510be20c6e32d022b01c83be8c94cbcb012e714),
   **formulaires acceptés derrière Vercel** : un prérequis découvert en route
   (voir les difficultés).
2. [`8b17ac4`](https://github.com/MatthiasGermain/MatthiasGermain/commit/8b17ac430453dab804f15f92d981f932d6e50861),
   **mot de passe et deux vues** :
   - sans session, une vue visiteur « Station connectée », dans la mise en page
     du site, aussi en anglais sous `/en/routine` ;
   - avec session, le tableau de bord de Matthias, inchangé, en `noindex` ;
   - la session est un cookie signé (HMAC-SHA256 de sa date d'expiration),
     valable 30 jours, `HttpOnly`, `Secure`, `SameSite=Lax`, sans rien stocker
     côté serveur ; le mot de passe est comparé en temps constant ;
   - `/api/routine-tasks.json` répond `401` sans session : jusque-là, il
     renvoyait les tâches Notion à n'importe qui ;
   - toutes les réponses de `/routine` sont en `Cache-Control: private,
     no-store` ;
   - un bouton « Voir en direct » sur la fiche du projet, page Projets du site.
3. [`c755cb7`](https://github.com/MatthiasGermain/MatthiasGermain/commit/c755cb74c455d44cb18174904a4b13cffec815fb),
   **bandeau Station** :
   - `src/lib/station/protocol.ts` : les types et la validation de chaque
     message, en miroir de [`docs/protocol.md`](../protocol.md) ; un message
     invalide est ignoré ;
   - un client MQTT dans le navigateur (mqtt.js, chargé en différé : 105 Ko
     compressés, seulement sur `/routine`), avec reconnexion automatique et un
     abonnement par topic ;
   - un composant en deux formats : compact en haut du tableau de bord, en
     grand dans la vue visiteur, avec les événements reçus depuis l'ouverture
     de la page ;
   - les états affichés : connexion au broker, broker injoignable, en ligne,
     hors ligne depuis…, mesure périmée (cartes grisées), heure inconnue, arrêt
     d'urgence en rouge, buzzer coupé, test.

Côté broker : l'utilisateur `web-viewer`, en lecture seule, créé dans EMQX
(règles dans l'onglet « Username », en liste blanche).

## Difficultés rencontrées

- **Le formulaire de contact du site était cassé en production.** Derrière
  Vercel, la protection CSRF d'Astro ignorait l'en-tête `X-Forwarded-Host`,
  croyait servir `localhost` et rejetait tous les formulaires POST (`403`,
  « Cross-site POST form submissions are forbidden »). Le formulaire de mot de
  passe aurait eu le même sort. Correction : `security.allowedDomains` dans
  `astro.config.mjs`. Le serveur de développement n'applique pas ce contrôle :
  le bug ne se voyait qu'en ligne.
- **Les styles du tableau de bord fuyaient dans la vue visiteur.** Avec les deux
  vues importées dans la même page, Astro chargeait tous leurs styles, dont les
  règles globales du tableau de bord sur `html` et `body`. Le tableau de bord
  est donc devenu une page à part, que `/routine` sert à sa propre adresse avec
  `Astro.rewrite`.
- **mqtt.js en développement.** Vite prépare la bibliothèque autrement qu'au
  build : `connect` n'y était qu'une propriété de l'export par défaut
  (« connect is not a function »).
- **La configuration.** Une adresse sans `wss://` (« Missing protocol ») ; et
  Vercel refuse de ranger une variable `PUBLIC_` comme secret, puisqu'elle part
  dans le navigateur : type « Config », ces valeurs étant publiques par nature.
- **Tester `web-viewer` dans MQTT Explorer.** Par défaut, l'outil s'abonne à
  `#`, que `web-viewer` n'a pas le droit de lire : rien ne s'affichait jusqu'à
  remplacer cet abonnement par les quatre topics. Et publier sur un topic qui
  contient `#` fait couper la connexion par le broker.

## Résultat

Vérifié en local, puis en ligne :

- [x] visiteur : la station en direct, en français et en anglais, sur
  ordinateur et sur téléphone
- [x] mauvais mot de passe refusé avec un message ; bon mot de passe : le
  tableau de bord ; déconnexion
- [x] `/api/routine-tasks.json` : `401` sans session ; cookie falsifié refusé
- [x] arrêt d'urgence touché : la carte passe au rouge en moins d'une seconde
  et l'événement s'affiche avec le temps de réaction ; le réarmement sur place
  se voit aussi
- [x] coupure réseau simulée (touche `n`) : « hors ligne depuis… », puis
  retour en ligne
- [x] `web-viewer` : lecture des quatre topics ; une commande publiée avec lui
  n'atteint pas la station
- [x] thème sombre du tableau de bord

## Limites connues

- **Pas d'historique** : la page n'affiche que la dernière mesure. Les courbes
  viennent à l'étape 6 ; piste : l'ESP32 garde 24 h de mesures et les publie en
  message retenu, sans base de données.
- **Les événements ne sont pas retenus** : la page ne montre que ceux reçus
  depuis son ouverture.
- **Pas de limite de tentatives** sur le mot de passe, faute de rien stocker :
  il est long et aléatoire. Pour révoquer toutes les sessions, on change la clé
  qui les signe.
- **Le mot de passe de `web-viewer` est public**, par conception : il ne permet
  que de lire.

## Captures

![La page /routine publique, station en ligne](../assets/04-page-en-direct.png)
