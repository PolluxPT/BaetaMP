# BaetaMP v1.3.2 — English default and console volume restore

## Changes
- English is now the default for a fresh installation and for settings without a valid language preference.
- Existing saved Portuguese/English choices are preserved.
- The console volume is captured before the player starts and restored on exit, including error and signal cleanup.
- Internal UI restarts after playback retain the session volume.

## Installation / update
Extract BaetaMP-v1.3.2-RGDSPlus.zip and merge the contents of APPS into Roms/APPS on the Games/TF2 card, replacing matching program files. Keep your config, movies and playlists.

## Validation
Native and ARM64 (QEMU) unit/track tests, migration and mocked launcher volume tests passed. Release ZIP integrity and reproducibility were verified. Hardware validation on RGDS+ remains necessary.

## Verificação na consola
1. Colocar o volume da consola num nível conhecido.
2. Abrir BaetaMP, alterar o volume, reproduzir e terminar um vídeo, e sair da aplicação.
3. Confirmar que o volume da consola regressou ao nível inicial.
4. Para verificar EN de raiz, guardar uma cópia de config/settings.txt e retirar temporariamente esse ficheiro; abrir BaetaMP e confirmar inglês.
5. Escolher PT nas opções e voltar a abrir para confirmar persistência.

## Publicar no GitHub
1. Testar o pacote na consola.
2. Atualizar no repositório os ficheiros de programa, código, testes e documentação incluídos neste ZIP, preservando os restantes.
3. Criar uma release com a tag v1.3.2 e o título BaetaMP v1.3.2.
4. Copiar as secções Changes e Installation / update para a descrição.
5. Anexar BaetaMP-v1.3.2-RGDSPlus.zip e SHA256SUMS.txt.
