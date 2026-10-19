# Instalar e atualizar BaetaMP

## Instalação de raiz

1. Descarrega o ZIP **BaetaMP-v1.3.0-RGDSPlus.zip** na Release.
2. Extrai o ZIP no computador.
3. Abre o cartão Games/TF2 e a pasta `Roms/APPS`.
4. Copia para lá o conteúdo da pasta `APPS` do ZIP: `BaetaMP.sh`, `BaetaMP/` e `Imgs/`.
5. Se já existe `Imgs`, junta o conteúdo; não substituas a pasta inteira de imagens de outras apps.
6. Abre BaetaMP no menu APPS da consola.

```text
Roms/APPS/
├── BaetaMP.sh
├── Imgs/
│   └── BaetaMP.png
└── BaetaMP/
    ├── bin/baetamp
    ├── fonts/
    ├── Filmes/
    └── IPTV/
```

`config/` é criada na primeira abertura. O ficheiro `Filmes/Sample_720p.mp4` é uma demonstração sintética com legendas externas.

## Atualizar BaetaMediaPlayer

Copia os novos ficheiros, substituindo apenas os ficheiros correspondentes. **Não apagues `BaetaMediaPlayer`, `MediaPlayer_RGDSPlus`, filmes, IPTV ou configurações antes da primeira abertura do BaetaMP.**

O lançador move os dados para `BaetaMP`. Se dois ficheiros têm o mesmo nome e o destino já existe, o ficheiro antigo fica na cópia `config/migracao-NOME-ANTIGO`, sem ser perdido. A preferência PT/EN, favoritos e pontos guardados mantêm o formato anterior. Os lançadores antigos reconhecidos saem do menu APPS e ficam arquivados dentro de `config`.

Se já usas BaetaMP, uma atualização futura apenas substitui os ficheiros de programa. O pacote não contém `config` nem dados pessoais.

## Utilização

Filmes e legendas com o mesmo nome podem ir em `BaetaMP/Filmes` (por exemplo, `Filme.mp4` e `Filme.srt`). As tuas listas `.m3u` e `.m3u8` podem ir em `BaetaMP/IPTV`. Também é possível navegar para outras pastas e introduzir URLs.

Log de diagnóstico: **`Roms/APPS/BaetaMP/config/Arranque.txt`**, substituído a cada arranque principal. O código de saída 42 depois de um vídeo é a reabertura prevista da interface.

Ao comunicar um problema, não envies `favoritos.txt`, cookies, URLs com tokens ou configurações pessoais. O modelo de reporte em Issues indica os dados úteis.
