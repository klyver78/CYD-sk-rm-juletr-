# CYD Juletræ

Et animeret juletræ til **ESP32-2432S028R CYD** med 320 × 240 touchskærm i landscape.

## Hvad programmet gør

- Viser et juletræ på mørkeblå baggrund. Træets tegning står stille, mens de røde, blå og gule lys blinker med individuelle, tilfældige intervaller på 2–6 sekunder.
- Stjernen forbliver gul og skifter blødt i lysstyrke.
- Efter 30 sekunder dæmpes skærmens bagbelysning fra 100 % til cirka 30 %. Et tryk gendanner 100 % og starter 30-sekundersperioden igen.
- Når lysstyrken er 100 %, viser et tryk “Klyver” og “JUL” i grønt på et rødt hjerte i tre sekunder. Derefter vender træet tilbage.
- Touch, lysanimation og bagbelysning håndteres lokalt. Der kræves ikke Wi-Fi.

## Hardwarekrav

- ESP32-2432S028R CYD (2,8″, 320 × 240, landscape).
- USB-datakabel til installation.
- Standardmodellen har ingen højttaler; programmet afspiller derfor ingen lyd.

Skærm- og touchopsætningen følger [CYD-test](https://github.com/klyver78/CYD-test), herunder TFT_eSPI, ILI9341, XPT2046 og touchkalibrering. CYD-kloner kan have andre touchkoordinater.

## Webinstaller

**[Åbn CYD Juletræ Web Installer](https://klyver78.github.io/CYD-sk-rm-juletr-/)**. Webinstallatøren virker i Chrome eller Edge på en computer via Web Serial.

Installationen overskriver firmwaren på enheden. Tilslut CYD’en med et USB-datakabel, tryk på installer, vælg enhedens serielle port, og bekræft.

## Byg og upload manuelt

1. Installer [Visual Studio Code](https://code.visualstudio.com/) og PlatformIO IDE.
2. Klon repositoryet, eller hent projektfilerne.
3. Åbn projektmappen i VS Code.
4. Tilslut CYD’en med USB-datakabel.
5. Kør PlatformIO-miljøet `cyd` med **Build** og derefter **Upload**.

Alternativt med PlatformIO Core:

```sh
pio run -e cyd
pio run -e cyd -t upload
```

Firmwaren bygges til ESP32 Arduino. Skærmen bruger GPIO 21 til bagbelysning; enkelte hardwarevarianter kan afvige.

## Kendte begrænsninger

- Standard-CYD har ingen højttaler.
- Dæmpningen regulerer bagbelysningen, ikke LCD-pixels; billedet kan derfor fortsat ses svagt.
- Touchkalibrering og USB-serieldriver afhænger af CYD-variant og computer.
