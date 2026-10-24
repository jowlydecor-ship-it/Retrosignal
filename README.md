# Retro Signal Chain — plugin OpenFX (réplique fonctionnelle du concept « FOUND / PWS-06 »)

Implémentation **originale** (aucun code ni asset de Paperwolf Studio). À ne pas distribuer sous le nom « FOUND ».

## Chaîne de signal (src/Engine.cpp, ordre strict)
1. **Lentille & capteur** : crosstalk spectral CCD, saturation/teinte, gain AGC (relève les ombres), flou optique,
   bloom, smear vertical CCD, compression douce des hautes lumières, contraste/noirs relevés. Mode intensificateur (night vision).
2. **Électronique** : pompage de luminance + bandes de scintillement, halo de renforcement de contours, incrustation date (gravée avant la bande).
3. **Bande / codec** : bruit capteur ∝ √gain, codec (sous-résolution + quantification DCT 8×8 4:2:0), bande passante luma/chroma,
   décalage chroma, jitter de lignes, tracking band, head-switching, stries, dropouts, générations successives (Copy of a Copy).
4. **Lecture / affichage** : OSD de visière, scanlines CRT + scintillement inter-trame + masque RGB (Best), grille LCD + 6 bits dithérés, vignettage.

## Paramètres
Look (15) · Intensity (Light/Standard/Heavy) · Quality (Fast/Balanced/Best) · Mix · Damage/Tape Wear · Tape Noise/Streaks ·
Max Gain (dB, absolu, actif avec « Override Look Gain ») · Saturation · Overlay (Auto/On/Off) · date/heure personnalisées · Seed.

## Compilation (macOS)
```
git clone --recursive https://github.com/AcademySoftwareFoundation/openfx
./build_macos.sh /chemin/vers/openfx --install
```
Produit `build/RetroSignalChain.ofx.bundle` (universal arm64+x86_64, signature ad hoc), installé dans `/Library/OFX/Plugins`.
(Le nom réel d'un bundle OpenFX est `Nom.ofx.bundle`, pas `.bundle.ofx`.)

## Compilation via GitHub Actions (sans Mac)
Pousser le projet sur GitHub ; `.github/workflows/build.yml` compile sur un runner macOS et publie `RetroSignalChain.ofx.bundle.zip` en artefact (ou en Release sur un tag `v1.0`).

## Test du moteur seul (sans SDK)
`g++ -O2 -std=c++17 -pthread tests/engine_test.cpp src/Engine.cpp -o engine_test && mkdir -p out && ./engine_test 960 540 1 1`

## Tests
`g++ -O3 -std=c++17 -pthread tests/regression_test.cpp src/Engine.cpp -o regtest && ./regtest`
(finitude des 135 combinaisons look/intensité/qualité, déterminisme, variation temporelle, monotonie du bruit, effet du gain, de la saturation, de l'overlay).

## État / limites
- Moteur testé (compilation, 9 tests de non-régression, rendu des 15 looks). **La couche OpenFX (src/RetroPlugin.cpp) n'a pas pu être
  compilée ici** (pas de SDK) : prévoir d'éventuels ajustements mineurs selon la version du SDK.
- Rendu **CPU multithread**, float RGBA, trame entière. Mesuré en 1080p : Fast ≈ 130 ms (pipeline à demi-résolution), Balanced/Best ≈ 380 ms.
  Convient au rendu et à la prévisualisation ; pas de lecture temps réel 24 i/s en pleine résolution.
- **Pas de backend Metal** : je n'ai ni compilateur Metal ni hôte Resolve pour le valider, et n'ai pas voulu livrer du code GPU jamais exécuté.
  Resolve sur macOS exécute très bien un plugin OpenFX CPU ; le port Metal serait une optimisation de vitesse (étages flous/DCT/bruit/affichage).
- « Copy of a Copy » est simulé par générations dans une même image (pas de récursion entre images, qui rendrait le rendu non reproductible).
- Les looks sont des interprétations visuelles des noms ; à affiner à l'œil sur tes plans.
