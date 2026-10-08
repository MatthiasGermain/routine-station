# 0004 : distinguer une flamme de la lumière du jour

- **Date** : 2026-10-08
- **Étape** : 2

## Contexte

Le capteur de flamme du kit est un récepteur infrarouge nu, sans filtre ni
comparateur. Il voit aussi l'infrarouge de la lumière du jour, et la pièce est
éclairée par une fenêtre. Mesures relevées :

- au repos, le capteur donne de 750 à 2000 mV selon l'heure et la météo ;
- une ombre ou une main le fait varier de plusieurs centaines de millivolts ;
- un briquet pointé vers lui à 20 cm ajoute de +160 à +680 mV selon la flamme.

Un seuil fixe ne peut donc pas marcher. Un seuil relatif à un niveau ambiant
qui suit lentement le capteur non plus : il a donné une fausse alarme à chaque
retour de la lumière après une ombre, et une alarme bloquée quand le jour
augmentait pendant une alarme.

## Décision

La **photorésistance sert de référence**. Quand le jour change, la lumière
visible change avec l'infrarouge ; un briquet, lui, ajoute de l'infrarouge sans
presque changer la lumière visible d'une pièce claire (0 à 33 mV mesurés).

Dans une pièce claire (photorésistance au-dessus de 50 %) :

- si la lumière visible bouge d'au moins 100 mV, c'est le jour qui change :
  pendant 500 ms, les niveaux de référence suivent les deux capteurs et aucune
  alarme ne peut partir ;
- sinon, une hausse d'infrarouge d'au moins 100 mV au-dessus de la référence,
  moyennée sur 100 ms et tenue pendant 20 ms, déclenche l'alarme.

Les réglages ont été choisis en enregistrant les signaux bruts (mode capture,
5 s de mesures toutes les 2 ms) puis en rejouant ces captures sur l'algorithme,
réécrit à l'identique sur PC, avec différents réglages. Le détail est dans le
[journal de l'étape 2](../journal/02-alarme-flamme.md).

## Alternatives écartées

- **Seuil fixe** : selon l'heure, la lumière du jour seule dépasse le niveau
  d'une flamme.
- **Niveau ambiant seul** (moyenne lente du capteur) : fausse alarme à chaque
  retour de la lumière après une ombre, alarme bloquée quand le jour augmente.
- **Tube autour du capteur** pour limiter la lumière ambiante : les reflets à
  l'intérieur du tube ont aggravé le problème.
- **Détection du vacillement** : une flamme vacille environ 7 fois par seconde,
  la lumière du jour non. Très net sur une flamme franche, mais trop faible sur
  une petite flamme, et plus complexe à régler. Reste une piste d'amélioration.

## Conséquences

- Aucune fausse alarme sur les ombres et les mains testées ; un briquet est
  détecté 70 à 90 ms après l'apparition de la flamme.
- **Portée limitée** : environ 20 cm en plein jour, briquet pointé vers le
  capteur. Une petite flamme à 30 cm reste sous le seuil.
- **Pendant les 500 ms qui suivent un changement de lumière**, une flamme qui
  apparaît est absorbée par la référence et peut passer inaperçue.
- **Dans la pénombre** (photorésistance sous 50 %), la lumière visible n'est
  pas vérifiée, sinon un briquet, qui éclaire la pièce, se masquerait lui-même.
  Allumer une ampoule à incandescence ou halogène peut alors déclencher
  l'alarme. Ce cas n'a pas été testé dans le noir complet.
- Les réglages dépendent de ce capteur et de cette pièce : le mode capture
  (touche `c`) permet de refaire les mesures ailleurs.
