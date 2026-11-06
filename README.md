# Electrolux AC Remote para Flipper Zero

Aplicativos (`.fap`) de controle remoto infravermelho para **ar-condicionado Electrolux**
no [Flipper Zero](https://flipperzero.one/), mais os arquivos `.ir` dos sinais.

Dois controles foram decifrados por engenharia reversa a partir de gravações IR reais
(nenhuma informação oficial da fabricante foi usada):

| App | Controle | Pasta |
|-----|----------|-------|
| **Electrolux AC (branco)** | controle simples (botões de borracha: MUDO/DISPLAY/OSCILAR/VELOCID./TIMER/VENTILAR/ECO) | [`apps/electrolux-ac-white`](apps/electrolux-ac-white) |
| **Electrolux Black AC** | controle premium com display OLED (Oscilar, Ar Indireto, Auto Sense, Dormir Bem, Ventilar, Turbo…) | [`apps/electrolux-ac-black`](apps/electrolux-ac-black) |

> ⚠️ **Projeto independente, sem qualquer vínculo com a Electrolux.** "Electrolux" é marca
> de seus respectivos titulares; é citada apenas para indicar compatibilidade (uso nominativo).
> Este projeto não é oficial nem endossado pela Electrolux.

## Recursos

Ambos os apps mantêm o **estado do ar** (como o controle original) e, a cada tecla, enviam o
sinal IR completo. Interface vertical no estilo dos apps "AC Remote" (Midea/Hitachi):

- Liga/Desliga, temperatura **16–30 °C**, velocidade (incl. Turbo), Oscilar, Ventilar, Led, Mudo
- **Black**: Dormir Bem (sono), Timer **1–24 h**, e Ar Indireto / Auto Sense (replay dos sinais reais)

O protocolo é da família **Gree** (comum em ar-condicionado Electrolux no Brasil): um bloco "A"
próprio da Electrolux (56 bits, checksum `XOR(bytes)^0xAA`) seguido de quadros Gree
(checksum padrão Gree). Toda a lógica de geração do sinal está em `ac_frame.h` e foi validada
reproduzindo as gravações bit a bit.

## Instalar (.fap pré-compilado)

Os `.fap` foram compilados para **Unleashed** (API 88.19). Copie o `.fap` para o SD do Flipper em
`/ext/apps/Infrared/` (via qFlipper ou direto no cartão) e abra em **Apps → Infrared**.

- Black (Unleashed): [`apps/electrolux-ac-black/electrolux_black_unleashed.fap`](apps/electrolux-ac-black/electrolux_black_unleashed.fap)
- Branco (Unleashed): [`apps/electrolux-ac-white/electrolux_ac_unleashed.fap`](apps/electrolux-ac-white/electrolux_ac_unleashed.fap)
- Branco (firmware oficial): [`apps/electrolux-ac-white/electrolux_ac_oficial.fap`](apps/electrolux-ac-white/electrolux_ac_oficial.fap)

Para outros firmwares, compile do código com [ufbt](https://github.com/flipperdevices/flipperzero-ufbt):

```sh
cd apps/electrolux-ac-black
ufbt            # gera dist/electrolux_black.fap
```

## Arquivos `.ir`

Em [`ir/`](ir) estão os sinais em formato RAW do Flipper, caso você só queira importá-los
(sem o app). Códigos IR são dados funcionais de interoperabilidade (como os do
[Flipper-IRDB](https://github.com/Lucaslhm/Flipper-IRDB)).

## Créditos e licenças

Código, lógica de sinal IR (`ac_frame.h`) e scripts: **MIT** (veja [`LICENSE`](LICENSE)).

O **design da interface e os ícones** (pasta `assets/`) foram **copiados e adaptados** destes
projetos, todos MIT — seus textos de licença estão em [`LICENSES/`](LICENSES):

- [Midea AC Remote](https://github.com/xakep666/flipperzero-midea-ac-remote) — Vladimir Stoliarov
- [Mitsubishi AC Remote](https://github.com/achistyakov/flipperzero-mitsubishi-ac-remote) — Anton Chistyakov
- [Hitachi AC Remote](https://github.com/dogtopus/flipperzero-hitachi-ac-remote) — dogtopus
- [Samsung AC Remote](https://github.com/dappermint/samsung-ac-remote-flipper-app) — Lia Yoffe (só a hélice do botão FAN)

Veja `apps/*/CREDITOS_E_LICENCAS.txt` para o detalhamento de cada ícone.
