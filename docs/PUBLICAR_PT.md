# Atualizar BaetaMP no GitHub / Update BaetaMP on GitHub

## Português — versão 1.3.1

O repositório já existe: https://github.com/PolluxPT/BaetaMP
Este guia atualiza esse projeto e publica a versão com botões arredondados.

### Atualizar o repositório

1. Extrair `BaetaMP_v1.3.1_GitHub_Kit_PT_EN.zip` no computador.
2. Abrir a raiz do repositório, onde está `README.md`.
3. Escolher **Add file → Upload files**.
4. Arrastar TODO O CONTEÚDO de `01_Atualizar_repositorio`, incluindo as subpastas.
   Não arrastar a pasta exterior nem o ZIP do kit. Confirmar os caminhos
   `src/player.c`, `APPS/BaetaMP/bin/baetamp`, `VERSION`, etc.
5. Mensagem: `Release v1.3.1: rounded buttons throughout the UI`.
6. Confirmar o commit na branch principal. Se o GitHub exigir uma pull request,
   criar e integrar essa alteração antes de publicar a release.
7. Conservar os restantes ficheiros do repositório. A pasta fornecida inclui
   apenas as alterações necessárias; não é uma cópia completa do projeto.

### Publicar a release

1. Abrir https://github.com/PolluxPT/BaetaMP/releases/new
2. Criar a tag **v1.3.1** e selecionar a branch principal já atualizada.
3. Título: **BaetaMP v1.3.1 — Rounded buttons / Botões arredondados**.
4. Copiar o conteúdo de `03_Texto_da_release_PT_EN.md` para a descrição.
5. Anexar os DOIS ficheiros de `02_Release`:
   `BaetaMP-v1.3.1-RGDSPlus.zip` e `SHA256SUMS.txt`.
6. Deixar **This is a pre-release** desmarcado e marcar **Set as latest release**,
   se disponível. Confirmar os anexos e clicar em **Publish release**.
7. Abrir o botão de download no README; deverá apresentar a v1.3.1.

Manter a versão anterior no histórico. A nova versão Latest passa a ser o
download principal. Não reutilizar a tag antiga para ficheiros diferentes.
O ZIP do kit serve para preparar a publicação; não deve ser anexado à release.

O pacote instalável inclui APPS, código e licenças. Os utilizadores copiam
somente o conteúdo de APPS para `Roms/APPS` no cartão, conservando os seus dados.
Os ficheiros automáticos Source code do GitHub não substituem o ZIP instalável.
A alteração foi compilada e verificada localmente; falta testar na consola.

## English — version 1.3.1

Use the existing repository: https://github.com/PolluxPT/BaetaMP

### Update the repository

1. Extract `BaetaMP_v1.3.1_GitHub_Kit_PT_EN.zip` on your computer.
2. Open the repository root and select **Add file → Upload files**.
3. Drag the CONTENTS of `01_Atualizar_repositorio`, preserving subfolders.
   Do not upload the enclosing folder or the kit ZIP. Check paths such as
   `src/player.c`, `APPS/BaetaMP/bin/baetamp` and `VERSION`.
4. Commit message: `Release v1.3.1: rounded buttons throughout the UI`.
5. Commit to the main branch, or create and merge a pull request if required.
6. Keep unrelated repository files. The supplied folder is a set of updates,
   not a complete repository snapshot.

### Publish the release

1. Open https://github.com/PolluxPT/BaetaMP/releases/new
2. Create tag **v1.3.1**, targeting the updated main branch.
3. Title: **BaetaMP v1.3.1 — Rounded buttons / Botões arredondados**.
4. Paste `03_Texto_da_release_PT_EN.md` into the description.
5. Attach both files from `02_Release`: `BaetaMP-v1.3.1-RGDSPlus.zip` and
   `SHA256SUMS.txt`.
6. Leave **This is a pre-release** unchecked; select **Set as latest release**
   if available. Check the attachments and click **Publish release**.
7. Verify the README download button opens v1.3.1.

Keep the previous release as history. Do not reuse its tag. Do not attach the
publication kit ZIP to the release. Users install only the contents of APPS
into `Roms/APPS`, keeping personal files. Local build and checks passed;
physical console testing of this visual update is still pending.

## Referências / References

- https://docs.github.com/en/repositories/working-with-files/managing-files/adding-a-file-to-a-repository
- https://docs.github.com/en/repositories/releasing-projects-on-github/managing-releases-in-a-repository
