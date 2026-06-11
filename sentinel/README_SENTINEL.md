# SENTINEL//OS — Instructions

## Installation

```bash
cd sentinel/
node server.js
```

## Utilisation

### Mode fenêtre unique (USE_EXTERNAL_SCREENS = false)
Ouvrir uniquement :
→ http://localhost:3000/SENTINEL_MAIN.html

La carte et les caméras s'affichent dans la colonne centrale.

---

### Mode multi-écrans (USE_EXTERNAL_SCREENS = true)
1. Modifier dans SENTINEL_MAIN.html :
   `const USE_EXTERNAL_SCREENS = true;`

2. Lancer le serveur :
   `node server.js`

3. Ouvrir les 3 fenêtres MANUELLEMENT (dans n'importe quel ordre) :
   - Écran 1 : http://localhost:3000/SENTINEL_MAIN.html  (console principale)
   - Écran 2 : http://localhost:3000/SENTINEL_MAP.html   (carte du monde)
   - Écran 3 : http://localhost:3000/SENTINEL_CAM.html   (caméras de surveillance)

4. Passer chaque fenêtre en plein écran (F11) sur son écran.

Les 3 fenêtres se connectent au serveur WebSocket automatiquement
et se synchronisent en temps réel pendant le jeu.

---

## Codes initiales — étape carte (étape 7)

| Ville         | Initiales acceptées |
|---------------|---------------------|
| Paris         | PA, PAR             |
| Berlin        | BE, BER             |
| Londres       | LO, LON             |
| Madrid        | MA, MAD             |
| Rome          | RO, ROM             |
| Moscou        | MO, MOS             |
| Istanbul      | IS, IST             |
| Amsterdam     | AM, AMS             |
| New York      | NY, NEW             |
| Chicago       | CH, CHI             |
| Los Angeles   | LA, LAX             |
| Toronto       | TO, TOR             |
| Miami         | MI, MIA             |
| São Paulo     | SP, SAO             |
| Tokyo         | TK, TOK, TKY        |
| Beijing       | BJ, PEK, BEI        |
| Shanghai      | SH, SHA             |
| Séoul         | SE, SEO, SEL        |
| Singapour     | SI, SIN, SG         |
| Mumbai        | MU, BOM, MUM        |
| Dubai         | DU, DXB, DUB        |
| Sydney        | SY, SYD             |
| Lagos         | LG, LAG, LOS        |
| Le Caire      | CA, CAI, LC         |
| Johannesburg  | JO, JHB, JNB        |
| Nairobi       | NA, NBO, NAI        |

Les initiales sont affichées directement sur la carte à côté de chaque ville.
