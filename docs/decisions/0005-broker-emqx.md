# 0005 : EMQX Serverless comme broker MQTT

- **Date** : 2026-10-08
- **Étape** : 3

## Contexte

Le projet prévoyait HiveMQ Cloud, dans son offre gratuite « Serverless ». Au
moment de créer le broker, cette offre n'existait plus : HiveMQ ne permet plus
d'en créer, et les brokers existants s'arrêtent le 31 décembre 2026. Il reste
chez HiveMQ un essai payant de 15 jours, ou une licence gratuite pour héberger
le broker soi-même.

Le broker doit être joignable depuis Internet par l'ESP32 et par le site sur
Vercel, chiffrer les échanges (TLS), accepter les connexions WebSocket du
navigateur, et séparer les droits : la page ne doit pouvoir que lire, la route
API que commander.

## Décision

**EMQX Serverless**, offre gratuite, région Europe (Francfort), avec une limite
de dépense à 0 $ : si le quota gratuit était dépassé, le service s'arrêterait au
lieu de facturer.

- Quota gratuit par mois : 1 million de minutes de session (la station connectée
  en permanence en consomme environ 45 000) et 1 Go de trafic.
- TLS obligatoire : MQTT sur le port 8883, WebSocket sur le port 8084.
- Utilisateurs avec mot de passe, et règles d'autorisation par utilisateur et
  par topic.

## Alternatives écartées

- **HiveMQ Cloud Starter** : le même service qu'avant, mais payant après 15
  jours d'essai.
- **Broker hébergé soi-même** (Mosquitto, ou HiveMQ avec sa licence gratuite)
  sur un serveur : contrôle total, mais un serveur, un nom de domaine et des
  certificats à entretenir, hors du sujet du projet.
- **Broker public de test** : aucune authentification, tous les messages
  visibles par tout le monde. Exclu.

## Conséquences

- Le modèle de sécurité est plus fin que prévu : chez HiveMQ gratuit, une
  permission valait pour tous les topics ; chez EMQX, chaque utilisateur n'a
  accès qu'à ses topics (voir [`docs/protocol.md`](../protocol.md)).
- **Le déploiement est mis en pause si aucun client ne s'y connecte pendant
  5 jours.** Si la station reste éteinte plus longtemps, il faut le relancer à
  la main dans la console EMQX.
- Le firmware utilise du MQTT standard : changer de broker ne demanderait que
  l'adresse, les identifiants et le certificat racine.
