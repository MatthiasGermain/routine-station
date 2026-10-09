# 0009 : la session par mot de passe protège aussi les commandes

- **Date** : 2026-10-09
- **Étape** : 5

## Contexte

Les commandes font tourner un moteur : seul Matthias doit pouvoir les envoyer.
`CLAUDE.md` prévoyait de protéger la route API des commandes avec Auth.js et une
connexion GitHub limitée à son compte, « ou équivalent ».

Entre-temps, l'étape 4 a mis la vue propriétaire de `/routine` derrière une
session par mot de passe : un cookie signé (HMAC-SHA256 de sa date
d'expiration), valable 30 jours, `HttpOnly`, `Secure`, `SameSite=Lax`, sans rien
stocker côté serveur, et un mot de passe comparé en temps constant. Les boutons
de commande sont dans cette vue.

Reste aussi à choisir comment la route publie sur le broker.

## Décision

**La même session protège la route des commandes** (`POST /api/station/command`
sur le site) : sans session valide, elle répond `401` et ne publie rien.

La route publie en MQTT avec l'utilisateur **`web-command`**, qui n'a qu'un seul
droit : publier sur `routine/station/commands`. Son mot de passe ne quitte
jamais le serveur Vercel.

## Alternatives écartées

- **Auth.js et une connexion GitHub** : une application OAuth à déclarer et une
  dépendance de plus, pour un seul utilisateur, et une deuxième connexion sur
  une page qui en a déjà une.
- **L'API HTTP d'EMQX pour publier** : une simple requête HTTPS, mais sa clé a
  des droits bien plus larges que `web-command`, limité à un seul topic.
- **Publier depuis le navigateur** : impossible sans y mettre un mot de passe
  qui permet d'écrire. Celui de `web-viewer` est public justement parce qu'il
  ne peut que lire.

## Conséquences

- **Une seule connexion**, déjà en place et testée à l'étape 4, et rien de plus
  à héberger.
- **Assumé : ni double authentification ni limite de tentatives**, faute de
  rien stocker. Un mot de passe qui fuit permet de piloter le moteur.
- **Ce qui le rend acceptable** :
  - la sécurité physique reste sur la carte : l'arrêt d'urgence ne se réarme
    que sur place, le moteur est refusé pendant une alarme, et la station
    revalide chaque commande ;
  - le mot de passe est long et aléatoire ;
  - changer `ROUTINE_SESSION_SECRET` révoque toutes les sessions d'un coup.
- **Défense en profondeur sur la route** : en plus de la session, elle vérifie
  l'en-tête `Origin` (le contrôle d'origine d'Astro ne couvre que les
  formulaires, pas une requête JSON), exige du JSON de 1 Ko au plus, valide la
  commande et crée elle-même son id.
- **À surveiller** : la latence, d'une demi-seconde à 8 s en ligne. La fonction
  Vercel tourne par défaut aux États-Unis alors que le broker est à Francfort,
  chaque commande ouvre une connexion TLS, et EMQX Serverless confirme parfois
  la connexion lentement (une connexion sur sept mesurées a échoué au bout de
  5 s).
