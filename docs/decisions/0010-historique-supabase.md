# 0010 : l'historique dans une base Postgres, alimentée par le broker

- **Date** : 2026-10-09
- **Étape** : 6

## Contexte

Les graphes de la page ont besoin d'un historique des mesures. Le broker ne
garde que la dernière mesure, et le site n'avait aucun stockage.

La première idée était de garder les 24 dernières heures dans l'ESP32, en
mémoire vive, et de les publier en message retenu. Matthias a voulu un
historique **sur le long terme** : des semaines, des mois, ce qu'une mémoire
vive effacée à chaque redémarrage ne peut pas tenir.

Contraintes : rien à payer, et ne pas élargir le projet, donc pas de serveur à
héberger et à maintenir.

## Décision

- Toutes les 5 min, la station publie la **moyenne** de ses mesures sur
  `routine/station/samples` ([`docs/protocol.md`](../protocol.md)).
- Une **règle EMQX** (intégration de données, connecteur « HTTP Server »)
  transmet chaque échantillon à une route du site, avec un jeton secret.
- La route le range dans une **base Postgres chez Supabase**, créée depuis
  l'onglet Storage de Vercel, dans la région de Francfort comme le broker et
  les fonctions du site.
- Une seconde route, publique et mise en cache, renvoie les points des
  graphes : échantillons de 5 min sur 24 h, moyennes par heure sur 7 et
  30 jours, par jour sur un an.

## Alternatives écartées

- **24 h dans la mémoire de l'ESP32** : rien à ajouter côté serveur, mais
  l'historique s'arrête à 24 h et disparaît à chaque redémarrage.
- **Neon**, le Postgres proposé par défaut dans Vercel : son offre gratuite
  compte des heures de calcul, et la base ne s'endort qu'après 5 min sans
  activité. Avec une écriture toutes les 5 min, elle ne dormirait jamais et
  dépasserait son quota.
- **Upstash Redis**, l'ancien Vercel KV : les quotas tiennent, mais Redis ne
  calcule pas de moyennes ; il aurait fallu tenir à la main les moyennes par
  heure et par jour.
- **L'ESP32 qui écrit directement sur le site en HTTPS** : une seconde
  connexion TLS sur la carte, un second certificat, et `loop()` bloquée pendant
  l'envoi. EMQX fait ce relais gratuitement.
- **Un programme abonné au broker qui tourne en permanence** (PC, petit
  serveur) : une machine de plus à faire tourner et à maintenir.
- **Une tâche planifiée Vercel** qui irait chercher les mesures : sur l'offre
  gratuite, elle ne peut tourner qu'une fois par jour.

## Conséquences

- **Rien à héberger, rien à payer** : environ 9 000 envois par mois, pour un
  quota gratuit d'un million chez EMQX ; quelques dizaines de Mo par an, pour
  500 Mo chez Supabase.
- **La station ne garde presque rien** : seulement les échantillons en attente
  du réseau (2 h). Un échantillon arrivé en retard se range à sa place, grâce à
  son heure.
- **Sécurité** : la route d'écriture n'accepte que le jeton d'EMQX et valide
  chaque échantillon ; la table a la sécurité au niveau des lignes activée sans
  aucune règle, donc seul le serveur du site y accède ; la lecture publique est
  mise en cache et ne sollicite pas la base.
- **Les pertes possibles** : un échantillon refusé par le broker (règle
  d'autorisation absente) ou que la route n'a pas pu enregistrer est perdu ;
  ni la station ni EMQX ne le renvoient plus tard.
- **À surveiller** : Supabase met en pause un projet gratuit sans activité
  pendant 7 jours. Tant que la station tourne, elle écrit toutes les 5 min ;
  si elle reste éteinte plus d'une semaine, il faudra relancer le projet
  depuis le tableau de bord de Supabase.
