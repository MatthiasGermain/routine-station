# 0002 : un pilote maison pour le moteur pas-à-pas

- **Date** : 2026-10-08
- **Étape** : 1

## Contexte

Le moteur 28BYJ-48 est piloté par un driver ULN2003 : le firmware doit allumer
ses quatre bobines dans le bon ordre, un pas toutes les quelques millisecondes.
Pendant ce temps, le programme doit continuer à lire les capteurs, et à
l'étape 2 l'alarme flamme doit pouvoir arrêter le moteur **immédiatement**.

## Décision

Un petit module `src/motor.cpp` d'une centaine de lignes, écrit pour le projet.
Il fait tourner le moteur en demi-pas, sans jamais bloquer : `motorMove()` fixe
un nombre de pas à faire, et `motorUpdate()`, appelée à chaque tour de
`loop()`, fait le pas suivant quand c'est l'heure. `motorStop()` coupe les
bobines tout de suite.

## Alternatives écartées

- **Bibliothèque `Stepper` d'Arduino** : `step()` bloque le programme jusqu'à
  la fin du mouvement. Pendant un tour (8 secondes), plus de mesures, et
  surtout aucun moyen de stopper le moteur en cours de route.
- **Bibliothèque AccelStepper** : non bloquante et complète (accélérations,
  positions), mais elle cache le fonctionnement du moteur derrière beaucoup de
  code. Le projet n'a besoin ni d'accélération ni de positionnement précis, et
  un pilote lisible montre ce qui se passe réellement sur les broches.

## Conséquences

- Aucune dépendance externe pour le moteur.
- La séquence des demi-pas est visible dans un tableau de huit lignes, facile à
  vérifier contre les LED A à D de la carte ULN2003.
- La régularité des pas dépend de la fréquence d'appel de `motorUpdate()` : si
  `loop()` prend du retard, le pas suivant arrive plus tard. L'alarme de
  l'étape 2 devra donc pouvoir arrêter le moteur sans attendre `loop()` : c'est
  l'objet de cette étape.
- Les bobines sont coupées à l'arrêt : le moteur ne chauffe pas, mais l'axe ne
  garde pas sa position sous un effort. Sans conséquence pour ce projet.
