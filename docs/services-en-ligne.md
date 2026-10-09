# Les services en ligne : EMQX, Vercel, Supabase

Ce qui est configuré dans chaque service, où trouver les données, et quoi
vérifier quand quelque chose ne passe plus. Le format des messages est dans
[`protocol.md`](protocol.md) ; les choix sont expliqués dans les décisions
[0005](decisions/0005-broker-emqx.md) (EMQX),
[0009](decisions/0009-session-mot-de-passe-commandes.md) (commandes) et
[0010](decisions/0010-historique-supabase.md) (historique).

**Aucun secret ici** : mots de passe, jetons et adresses complètes vivent dans
`include/secrets.h` (station), dans les variables Vercel et dans le `.env` local
du site, jamais dans un dépôt.

```
             ┌──────────────── EMQX Serverless (Francfort) ────────────────┐
ESP32 ──TLS──▶ topics routine/station/…                                    │
             │   └─ règle « samples » ──HTTP──▶ site : /api/station/samples ─┼──▶ Supabase
             └──────────────▲──────────────────────────▲──────────────────┘     (Postgres,
                            │ WebSocket (web-viewer)    │ MQTT (web-command)      Francfort)
                     page /routine            site : /api/station/command          ▲
                                          site : /api/station/history ─────────────┘
                     (Vercel, fonctions à Francfort)
```

## EMQX Serverless

- **Déploiement** : offre gratuite, région `eu-central-1` (Francfort),
  plafond de dépense à 0 $ ([décision 0005](decisions/0005-broker-emqx.md)).
  Ports : 8883 (MQTT sur TLS) et 8084 (WebSocket sur TLS, chemin `/mqtt`).
- **Utilisateurs** (Access Control → Authentication) : `station`,
  `web-viewer`, `web-command`. Plus de compte de test.
- **Règles d'autorisation** (Access Control → Authorization, onglet
  **Username**), en liste blanche. Préfixe `routine/station/` omis :

  | Utilisateur | Publier | S'abonner |
  |-------------|---------|-----------|
  | `station` | `measurements`, `events`, `status`, `replies`, `samples` | `commands` |
  | `web-viewer` | rien | `measurements`, `events`, `status`, `replies` |
  | `web-command` | `commands` | rien |
  | tous (règle « All Users ») | `#` refusé | `#` refusé |

  Une publication sans règle est **jetée sans erreur** : toute nouvelle
  publication commence par sa règle.
- **Intégration de données** (Data Integration) :
  - un connecteur **HTTP Server**, adresse de base
    `https://matthias-germain.vercel.app` ;
  - une règle `SELECT payload FROM "routine/station/samples"`, avec une action
    sur ce connecteur : `POST`, chemin `/api/station/samples`, en-têtes
    `content-type: application/json` et
    `authorization: Bearer <STATION_INGEST_TOKEN>`, corps `${payload}`.
- **Quotas gratuits par mois** : 1 million de minutes de session, 1 Go de
  trafic, 1 million d'actions de règle (environ 9 000 utilisées) ; au plus 2
  connecteurs et 4 règles.

## Vercel (projet `matthias-germain`)

- **Région des fonctions** : Francfort (`fra1`), Settings → Functions. Une
  seule région possible avec l'offre gratuite ; un redéploiement est
  nécessaire après un changement.
- **Variables de la station** (Settings → Environment Variables), en
  Production et Preview, et dans le `.env` local du site pour le
  développement :

  | Variable | Rôle | Type conseillé |
  |----------|------|----------------|
  | `ROUTINE_PASSWORD` | mot de passe de la vue propriétaire | Sensitive |
  | `ROUTINE_SESSION_SECRET` | clé qui signe les sessions ; la changer les révoque toutes | Sensitive |
  | `PUBLIC_STATION_MQTT_URL`, `_USERNAME`, `_PASSWORD` | connexion WebSocket de la page avec `web-viewer` (publics par nature) | Config |
  | `STATION_MQTT_URL` | `mqtts://<adresse>:8883`, pour la route des commandes | Sensitive |
  | `STATION_COMMAND_USERNAME`, `STATION_COMMAND_PASSWORD` | l'utilisateur `web-command` | Sensitive |
  | `STATION_INGEST_TOKEN` | le jeton envoyé par EMQX à la route d'enregistrement | Sensitive |
  | `STATION_DB_POSTGRES_URL` | la base, adresse « poolée » (voir plus bas) | Sensitive, créée par l'intégration |

  Les autres variables `STATION_DB_*` (clés Supabase, adresses Prisma ou
  directe) ont été créées par l'intégration mais ne servent pas. Une variable
  « Sensitive » n'est plus lisible après sa création : pour la changer, on la
  remplace.

## Supabase (la base de l'historique)

- **Création** : depuis Vercel, onglet **Storage** → Create Database →
  **Supabase**, offre Free, région Francfort, préfixe des variables
  `STATION_DB`, environnements Production et Preview, « Sensitive » activé,
  sans branches de prévisualisation. Le tableau de bord Supabase s'ouvre depuis
  la base dans l'onglet Storage (**Open in Supabase**).
- **La table**, créée dans le SQL Editor de Supabase :

  ```sql
  create table public.station_samples (
    time timestamptz primary key,
    temperature_c real not null check (temperature_c between -40 and 125),
    light_pct smallint not null check (light_pct between 0 and 100)
  );
  alter table public.station_samples enable row level security;
  ```

  La sécurité au niveau des lignes est activée **sans aucune règle** : les clés
  publiques de Supabase ne peuvent rien lire ni écrire. Seul le serveur du site
  y accède, par la connexion Postgres.
- **La connexion du site** : `STATION_DB_POSTGRES_URL`, le « Transaction
  pooler » (port 6543), fait pour les fonctions qui ouvrent et ferment une
  connexion à chaque appel. Dans ce mode, pas de requêtes préparées et **une
  requête à la fois** par connexion (sinon le pooler se bloque, voir le
  [journal 06](journal/06-finition.md)). En local, copier l'adresse
  « Transaction pooler » (bouton **Connect** de Supabase) dans le `.env` du
  site, sous le même nom.
- **Volume** : une ligne toutes les 5 min, environ 105 000 par an, quelques
  dizaines de Mo, pour 500 Mo gratuits.

## Consulter l'historique

- **Sur la page** : https://matthias-germain.vercel.app/routine, graphes sur
  24 h, 7 jours, 30 jours ou un an.
- **En JSON** : `GET https://matthias-germain.vercel.app/api/station/history?range=7d`
  (`24h`, `7d`, `30d` ou `1y`), mis en cache 5 min.
- **Dans Supabase** : Table Editor → `station_samples` (export CSV possible),
  ou le SQL Editor :

  ```sql
  -- Combien de points, depuis quand
  select count(*), min(time), max(time) from station_samples;

  -- Les dernières 24 h
  select * from station_samples
  where time > now() - interval '24 hours'
  order by time;

  -- Moyenne, minimum et maximum par jour, à l'heure de Paris
  select date_trunc('day', time at time zone 'Europe/Paris') as jour,
         round(avg(temperature_c)::numeric, 1) as temp_moy,
         min(temperature_c) as temp_min,
         max(temperature_c) as temp_max,
         round(avg(light_pct)) as lumiere_moy
  from station_samples
  group by 1
  order by 1;
  ```

## Quand les points n'arrivent plus

Dans l'ordre :

1. **La station** : le moniteur série affiche-t-il `>>> history sample …`
   toutes les 5 min ? Sinon, l'heure n'est pas encore synchronisée ou la
   station est hors ligne.
2. **Le droit de publier** : règle `station` → `samples` → Publish dans EMQX.
3. **La règle EMQX** : ses statistiques (succès, échecs) dans Data
   Integration. Des échecs : voir l'étape suivante.
4. **Les journaux Vercel** de `/api/station/samples` : `401`, le jeton
   d'EMQX ne correspond plus à `STATION_INGEST_TOKEN` ; `400`, échantillon
   refusé par la validation ; `502`, base injoignable.
5. **Supabase** : le projet est-il en pause ? Un projet gratuit inactif
   pendant 7 jours est mis en pause (la station écrit toutes les 5 min, donc
   seulement si elle est restée éteinte plus d'une semaine). Le relancer depuis
   le tableau de bord.

**Changer le jeton d'enregistrement** : en générer un nouveau
(`node -e "console.log(require('crypto').randomBytes(32).toString('base64url'))"`),
le remplacer dans Vercel et dans l'en-tête de l'action EMQX, redéployer le site.
Les échantillons envoyés entre les deux sont perdus.

## Tout reconstruire

1. EMQX : déploiement, les trois utilisateurs, les règles d'autorisation
   ci-dessus ; mettre l'adresse et les identifiants de `station` dans
   `include/secrets.h`.
2. Vercel : la région `fra1`, puis la base Supabase depuis l'onglet Storage ;
   créer la table dans Supabase.
3. Vercel : les variables du tableau ci-dessus ; déployer le site.
4. EMQX : le connecteur HTTP et la règle `samples`, avec le jeton.
5. Téléverser le firmware et vérifier les étapes de la section précédente.
