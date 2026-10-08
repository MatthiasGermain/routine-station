# Câblage

Branchement broche par broche de la station, avec les points d'attention
(tensions, résistances). Les numéros de GPIO de ce document et ceux de
[`include/pins.h`](../include/pins.h) doivent toujours être identiques.

## Carte

ESP32 DevKit V1, 30 broches (module ESP32-WROOM-32). Les noms ci-dessous sont
ceux sérigraphiés sur la carte. **TX2** et **RX2** sont les GPIO17 et GPIO16 :
ces noms viennent d'un second port série inutilisé, on s'en sert comme broches
normales.

La carte est à cheval sur deux breadboards : une colonne de broches sur chacune,
pour garder des trous libres des deux côtés.

## Règles valables pour tout le montage

- **Logique en 3,3 V.** Les GPIO de l'ESP32 ne tolèrent pas le 5 V : aucun
  signal à 5 V ne doit arriver directement sur une broche.
- **Capteurs analogiques sur ADC1** (GPIO 32 à 39). L'ADC2 est inutilisable
  quand le Wi-Fi est actif.
- **GPIO 34 à 39 : entrées uniquement**, sans résistance de tirage interne.
- **Broches à éviter** : GPIO 0, 5, 12 et 15 (broches de démarrage : leur état
  au démarrage compte, et certaines envoient un signal à ce moment-là), 6 à 11
  (mémoire flash), 1 et 3 (port série USB). GPIO2 est aussi une broche de
  démarrage, mais elle ne porte que la LED intégrée.

## Alimentation

| Depuis | Vers | Rôle |
|--------|------|------|
| VIN | rail rouge | **5 V** (celui de l'USB, environ 4,7 V après la diode de la carte) |
| GND | rail bleu | masse commune |
| 3V3 | fils directs | photorésistance et capteur de flamme |

- Le 3,3 V n'a pas de rail, exprès : avec un seul rail rouge, impossible de
  brancher par erreur un capteur 3,3 V sur le 5 V.
- Sur la grande breadboard, les rails sont **coupés au milieu** : deux ponts
  relient les deux moitiés (rouge avec rouge, bleu avec bleu).

## Étape 1 : capteurs et moteur

### Température : LM35

| Broche du LM35 | Vers | Remarque |
|----------------|------|----------|
| +Vs | rail 5 V | il lui faut au moins 4 V |
| Vout | **GPIO34** (D34) | 10 mV par °C, ne dépasse pas 1,5 V : sans danger |
| GND | rail GND | |

Sens : face plate vers soi, pattes en bas, de gauche à droite : +Vs, Vout, GND.
Monté à l'envers, il chauffe vite.

### Lumière : photorésistance

```
3V3 ── photorésistance ──┬── 10 kΩ ── GND
                         │
                    GPIO35 (D35)
```

Pont diviseur : plus il y a de lumière, plus la tension sur GPIO35 monte. La
résistance de 10 kΩ est de l'ordre de celle de la photorésistance dans une pièce
éclairée, c'est là que le montage est le plus sensible.

### Flamme : récepteur infrarouge

Le capteur du kit est un récepteur infrarouge nu, en forme de LED noire à
2 pattes, sans module ni sortie numérique.

```
3V3 ── patte courte  capteur  patte longue ──┬── 10 kΩ ── GND
                                             │
                                        GPIO32 (D32)
```

Plus il reçoit d'infrarouge, plus la tension sur GPIO32 monte. Monté dans
l'autre sens, la valeur reste bloquée, sans risque pour la carte. Le soleil et
les lampes halogènes émettent aussi de l'infrarouge.

### Moteur : 28BYJ-48 et driver ULN2003

| Broche ESP32 | ULN2003 |
|--------------|---------|
| D19 (GPIO19) | IN1 |
| D18 (GPIO18) | IN2 |
| TX2 (GPIO17) | IN3 |
| RX2 (GPIO16) | IN4 |
| rail 5 V | + (5-12V) |
| rail GND | − |

- **D5 reste libre** même si elle est entre D18 et TX2 : elle envoie un signal
  au démarrage qui ferait tressauter le moteur.
- **Diagnostic avec les LED A à D** de la carte ULN2003 (A = IN1 … D = IN4) :
  pendant les pauses du moteur, le programme met les 4 entrées à 0, donc les
  4 LED doivent être éteintes. Une LED qui reste allumée veut dire que son fil
  n'est pas sur la bonne broche de l'ESP32. Le moteur vibre alors sans tourner.
- L'ULN2003 accepte les signaux 3,3 V de l'ESP32 et ne renvoie jamais de 5 V
  vers elles.
- Le moteur se branche sur le connecteur blanc de la carte ULN2003 (il ne rentre
  que dans un sens).
- Le moteur consomme environ 200 mA sur le 5 V de l'USB. Si la carte redémarre
  quand il démarre, l'alimenter par le module d'alimentation 3,3 V/5 V du kit,
  masse commune avec l'ESP32.

### Récapitulatif

| Élément | GPIO | Nom sur la carte |
|---------|------|------------------|
| LED intégrée | 2 | — |
| LM35 | 34 | D34 |
| Photorésistance | 35 | D35 |
| Capteur de flamme | 32 | D32 |
| ULN2003 IN1 | 19 | D19 |
| ULN2003 IN2 | 18 | D18 |
| ULN2003 IN3 | 17 | TX2 |
| ULN2003 IN4 | 16 | RX2 |

## Étape 2 et suivantes

_À venir : LED et buzzer de l'alarme._
