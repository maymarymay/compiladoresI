## USO_IA.md

Ferramenta: claude
Trecho: README.md
Finalidade: foi pedido para fazer um readme
O que fiz: adaptei e organizei melhor o readme do jeito que eu queria

Ferramenta: claude
Trecho: relatório
Finalidade: foi pedido para melhorar a escrita do relatório e manter uma boa gramática e estrutura
O que fiz: adicionei as mudanças que achei melhor 

Ferramenta: claude
Trecho: bugs no passo 1 e 2 e outras coisas que não estava entendendo
Finalidade: foi pedido para explicar o que era pra ser feito e o que foi sendo feito pedi para corrigir bugs
O que fiz: implementei o código e corrigi o que estava dando errado

Ferramenta: Claude Haiku 5.5
Trecho: Scanner (microc/scanner.c, microc/microc.flex)
Finalidade: foi pedido para estruturar o adaptador do scanner do TP1 e corrigir o que não estava funcionando
O que fiz: recebi a sugestão de estrutura do adaptador, a separação do YYSTYPE em microc_yylval.h e a troca das mensagens de erro por strdup. Compilei, testei e corrigi o que deu erro.

Ferramenta: Claude Haiku 5.5
Trecho: Gerador (src/codegen.c)
Finalidade: foi pedido para implementar a leitura do arquivo de tokens (opção --tokens) e para encontrar a causa do erro 'entrada' undeclared no main gerado
O que fiz: recebi a proposta da função gen_token_de_arquivo, que testei com um arquivo de tokens de exemplo. Ajustei o formato aceito pelo sscanf e o tratamento da vírgula antes de linha, depois de o teste apontar o problema. Também identifiquei que um parser.c editado à mão estava desatualizado e decidi regenerá-lo em vez de manter a edição manual.

Ferramenta: Claude Haiku 5.5
Trecho: Depuração (saída do valgrind e erros de compilação)
Finalidade: foi pedido para interpretar os blocos still reachable do valgrind e localizar a causa dos erros de compilação
O que fiz: executei os comandos, colei as saídas de volta e verifiquei que o soma.mc não tinha vazamentos. Concluí que os 16.930 bytes still reachable no caminho de erro léxico são buffers do flex que não foram liberados, não vazamento, e decidi não alterar o gerador só para zerá-los.
