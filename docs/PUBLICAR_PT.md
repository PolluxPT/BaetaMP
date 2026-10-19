# Publicar BaetaMP no GitHub — sem programar

Este pacote está preparado, mas **ainda não foi publicado na tua conta**. Não precisas de executar comandos para seguir este guia.

## 1. Extrair o pacote

Extrai `BaetaMP_GitHub_Pronto.zip`. Encontrarás:

- **REPOSITORIO**: ficheiros que vão para o repositório GitHub.
- **PARA_RELEASE**: ZIP instalável, checksum e texto da Release.
- **COMECA_AQUI.txt**: resumo dos passos.

Não carregues o ZIP completo como se fosse o código do projeto: extrai primeiro.

## 2. Criar o repositório

1. Entra na tua conta e abre [github.com/new](https://github.com/new?name=BaetaMP&description=Media+Player+for+Anbernic+RG+DS%2B+%E2%80%94+films%2C+subtitles+and+IPTV&visibility=public).
2. Nome: **BaetaMP**. Descrição: **Media Player for Anbernic RG DS+ — films, subtitles and IPTV.**
3. Escolhe **Public** para permitir o acesso público.
4. Deixa desmarcadas as opções para criar README, .gitignore ou licença: os ficheiros completos já estão no pacote.
5. Clica em **Create repository**.

## 3. Carregar os ficheiros

1. Na página do repositório vazio, escolhe **uploading an existing file**; num repositório com ficheiros, usa **Add file → Upload files**.
2. Abre a pasta **REPOSITORIO** no computador.
3. Arrasta **o conteúdo dessa pasta**, incluindo as subpastas, para a zona de carregamento. A raiz no GitHub deve mostrar `README.md`, `src`, `APPS`, `tests`, `docs`, `Makefile`, etc.; não deve aparecer uma pasta única chamada REPOSITORIO.
4. Inclui também **`.github`**, **`.gitignore`** e **`.gitattributes`**. Se o teu explorador os ocultar, ativa a visualização de ficheiros ocultos. A pasta `.github` contém o teste automático.
5. Mensagem de commit sugerida: **Initial BaetaMP v1.3.0 release**.
6. Confirma em **Commit changes**. Se o navegador limitar a quantidade de ficheiros, repete o carregamento por pastas, mantendo os mesmos caminhos na raiz.

**Não carregues `PARA_RELEASE`, dados da tua consola, favoritos, listas pessoais ou logs para a raiz do repositório.** O pacote REPOSITORIO já está limpo. Não substituas a licença fornecida por MIT: este projeto mantém GPL-3.0-or-later.

## 4. Conferir o resultado

O README deve apresentar o logótipo. Confirma que existe **APPS/Imgs/BaetaMP.png**, que a versão em `VERSION` é 1.3.0 e que `APPS/BaetaMP/bin/baetamp` foi carregado como ficheiro binário.

A aba **Actions** deverá executar **Build and test** quando o workflow estiver carregado. O pacote foi testado localmente, mas o workflow só será validado pelo serviço depois deste primeiro carregamento. Se falhar, envia o nome da etapa e a mensagem; não apagues o código-fonte.

Se o carregamento de `.github` pelo navegador não estiver disponível na tua conta, mantém os restantes ficheiros e cria o workflow pela interface **Add file → Create new file**, com o caminho `.github/workflows/check.yml`, copiando o conteúdo do ficheiro fornecido.

## 5. Criar a Release instalável

1. Abre **Releases → Draft a new release**.
2. Em **Choose a tag**, escreve **v1.3.0**, cria essa tag e escolhe a branch que contém os ficheiros (normalmente `main`).
3. Título: **BaetaMP v1.3.0 — for RGDS+**.
4. Copia o texto de **PARA_RELEASE/NOTAS_RELEASE.md** para a descrição.
5. Na zona dos ficheiros anexos, coloca **BaetaMP-v1.3.0-RGDSPlus.zip** e **SHA256SUMS.txt**, ambos em PARA_RELEASE.
6. Confirma que os dois anexos estão presentes. Podes guardar em **Save draft** para rever. Quando estiver tudo certo, usa **Publish release**.

O utilizador final descarrega o ZIP anexado, extrai-o e copia apenas o conteúdo de APPS para o cartão. Os ficheiros **Source code (zip)** que o GitHub cria automaticamente representam o código do repositório; não são o anexo de instalação que preparámos.

## Atualizações futuras

Mantém o mesmo repositório. Uma nova versão deve atualizar código, binário, VERSION e notas; cria uma nova tag e uma nova Release, anexando o respetivo ZIP/checksum. Não alteres apenas a versão escrita na página.

## Documentação oficial consultada

- [Criar repositório](https://docs.github.com/en/repositories/creating-and-managing-repositories/creating-a-new-repository)
- [Carregar ficheiros](https://docs.github.com/en/repositories/working-with-files/managing-files/adding-a-file-to-a-repository)
- [Gerir Releases](https://docs.github.com/en/repositories/releasing-projects-on-github/managing-releases-in-a-repository)
