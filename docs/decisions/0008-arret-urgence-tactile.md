# 0008 : remplacer le capteur de flamme par un arrêt d'urgence tactile

- **Date** : 2026-10-09
- **Étape** : après l'étape 3

## Contexte

La règle de la [décision 0004](0004-flamme-ou-lumiere-du-jour.md) passait
tous les essais de l'étape 2. En usage réel, la station tournant toute la
journée près de la fenêtre, elle n'a pas tenu :

- **fausse alarme à chaque retour de la lumière après une ombre de quelques
  secondes** : pendant l'ombre, la référence descendait avec les capteurs ;
  au retour, la photorésistance saturait (92 %) alors que l'infrarouge
  continuait de monter, et la hausse passait pour une flamme. Au rejeu,
  l'ancienne règle donnait 15 fausses alarmes sur 20 ombres simulées, de 2 à
  40 s ;
- **troisième règle**, une mémoire de l'ombre (référence qui ne redescend
  qu'en 5 min, attente de 400 ms dans les cas douteux) : 0 fausse alarme au
  rejeu, mais sur la carte, un briquet à 5 cm était ignoré
  (`flame +776 mV, light +101 mV`). De près, la flamme éclaire assez la
  photorésistance pour ressembler à un retour du jour ;
- **quatrième règle**, la proportion entre infrarouge et lumière visible (sous
  3, c'est le jour) : le briquet à 5 cm restait ignoré, avec des rapports de
  2,5 à 3,6 (`+1351 mV / +373 mV`, `+1463 mV / +584 mV`), en plein dans la
  zone du soleil.

Quatre règles en deux jours : chacune réglait un cas et en cassait un autre.
En plein jour, une flamme proche et un retour du soleil se ressemblent pour ce
capteur nu, et l'information qui les distinguerait n'est pas dans le signal.
Une fonction de sécurité qui ignore parfois sa propre alarme est pire que pas
de fonction du tout.

Contrainte : rien à acheter. Les seuls autres capteurs disponibles sont un
module tactile TTP223 et un récepteur infrarouge VS1838B.

## Décision

L'alarme devient un **arrêt d'urgence tactile**, sur le module TTP223 (D32,
voir [`docs/wiring.md`](../wiring.md)) :

- un toucher le déclenche : LED, buzzer, moteur bloqué ;
- il reste **verrouillé** quand on retire le doigt ;
- seul un **appui long de 2 s sur le module**, sur place, le réarme. Aucune
  commande depuis le web ne peut le faire : on ne relance pas à distance une
  machine que personne ne voit.

La sortie numérique du module déclenche une **interruption matérielle** (front
montant) qui réveille la tâche d'alarme de la
[décision 0003](0003-alarme-tache-freertos.md). La tâche relit aussi l'entrée
toutes les 10 ms, pour mesurer l'appui long et en secours de l'interruption.

Le capteur de flamme est retiré du montage, avec le mode capture (touche `c`)
qui servait à le régler.

## Alternatives écartées

- **Persévérer avec le capteur de flamme** : une cinquième règle aurait réglé
  le briquet proche et cassé un autre cas. Il faudrait un filtre optique ou un
  second capteur infrarouge de référence, donc du matériel en plus.
- **Le garder comme simple mesure affichée** (« infrarouge ambiant »), sans
  alarme : une valeur qui suit surtout le soleil n'apporte rien à la page.
- **Le VS1838B** : c'est un récepteur de télécommande. Il ne réagit qu'à
  l'infrarouge clignotant à 38 kHz, et il est justement conçu pour ignorer le
  soleil et les flammes. Il pourrait servir plus tard à piloter la station
  avec la télécommande du kit.
- **Une alarme momentanée** (active tant qu'on touche, plus 3 s) : la machine
  repartirait seule. Ce n'est pas le comportement d'un arrêt d'urgence.
- **Un réarmement depuis le web** : écarté pour la raison donnée plus haut.

## Conséquences

- **Plus aucune fausse alarme liée à la lumière** : on touche le module ou on
  ne le touche pas.
- **Une vraie interruption matérielle**, que le capteur de flamme, analogique,
  rendait impossible. Mesuré sur 11 touchers, le firmware réagit en 54 µs
  (médiane, de 15 à 86 µs), y compris pendant une coupure réseau et avec
  `loop()` bloquée 500 ms à chaque tour : dans ce dernier cas, `loop()`
  n'a vu l'alarme que 388 ms plus tard. Le module lui-même met de 60 à 220 ms
  à reconnaître le doigt selon son mode (fiche technique du TTP223, non mesuré
  ici) : c'est lui qui domine.
- **Le travail des étapes 2 et 3 reste valable** : la tâche d'alarme
  (priorité 10, chien de garde), les sorties, le blocage du moteur, les
  événements MQTT, les commandes test et silence. Seul le déclencheur change.
- **Le protocole change** ([`docs/protocol.md`](../protocol.md)) :
  `flame_rise_mv` disparaît des mesures, `alarm_raised` porte
  `"cause": "touch"` et le temps de réaction `reaction_us`. Le site ne lisait
  encore rien (étape 4) : rien à adapter côté web.
- **À surveiller** : le TTP223 se calibre à la mise sous tension. Il ne faut
  pas le toucher au démarrage, et un objet posé dessus ou de l'humidité peut
  le déclencher.
- La décision 0004, le mode capture et le rejeu sur PC restent dans
  l'historique, au tag `v0.3-mqtt`, avec le
  [journal de l'étape 2](../journal/02-alarme-flamme.md).
