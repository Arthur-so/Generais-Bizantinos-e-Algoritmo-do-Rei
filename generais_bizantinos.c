#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/*
  Este código executa o Algoritmo dos Generais Bizantinos em diferentes cenários:
    - Cenário 1: 0 traidores, falha bizantina = 0, falha de quebra = 0
    - Cenário 2: 1 traidor (falha bizantina), falha de quebra = 0
    - Cenário 3: 1 traidor (falha de quebra), simulando com recv não bloqueante + timeout
*/
static const int TOTAL_GENERAIS = 4;

typedef struct {
    int numTraidores;
    int falhaBizantina;
    int falhaQuebra;
} Cenario;


int receberMensagem(
    int origem, 
    int tag, 
    MPI_Comm comm,
    int falhaDeQuebra,
    double timeoutSegundos
) {
    int valorRecebido = -1;
    if (!falhaDeQuebra) {
        // Recebimento normal (bloqueante)
        MPI_Recv(&valorRecebido, 1, MPI_INT, origem, tag, comm, MPI_STATUS_IGNORE);
    } else {
        // Exemplo simplificado de recepção não bloqueante com timeout
        MPI_Request req;
        MPI_Status status;
        int flag = 0;
        double inicio = MPI_Wtime();
        
        MPI_Irecv(&valorRecebido, 1, MPI_INT, origem, tag, comm, &req);

        while (!flag) {
            MPI_Test(&req, &flag, &status);
            double agora = MPI_Wtime();
            if ((agora - inicio) > timeoutSegundos) {
                // Falha de quebra simulada: não recebemos nada
                // neste exemplo, consideramos "valorRecebido = -1"
                // para indicar que o processo "quebrou" e não enviou.
                return -1;
            }
        }
    }
    return valorRecebido;
}

void rodarAlgoritmoGenerais(Cenario c) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size != TOTAL_GENERAIS) {
        if (rank == 0) {
            printf("[ERRO] Este código foi configurado para rodar com %d processos.\n",
                   TOTAL_GENERAIS);
        }
        return;
    }

    // Semente aleatória
    srand(time(NULL) + rank);

    int ehTraidor = 0;
    // Definimos, por exemplo, que os primeiros "c.numTraidores" ranks são traidores
    if (rank < c.numTraidores) {
        ehTraidor = 1;
    }

    // Decisão inicial: 0 = Recuar, 1 = Atacar
    int minhaDecisao = rand() % 2;

    // Se for traidor e falha bizantina, ele pode alterar sua decisão de forma maliciosa
    if (ehTraidor && c.falhaBizantina) {
        minhaDecisao = rand() % 2;
    }

    // Vetor para armazenar as mensagens recebidas
    int mensagensRecebidas[size];

    // Primeira rodada: enviar decisão para todos
    for (int i = 0; i < size; i++) {
        if (ehTraidor && c.falhaQuebra) {
            // Não envia nada (ou envia lixo)
            int lixo = -1;
            MPI_Send(&lixo, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
        } else {
            // Envia a decisão normal
            MPI_Send(&minhaDecisao, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
        }
    }

    // Recebe as decisões de todos
    for (int i = 0; i < size; i++) {
        mensagensRecebidas[i] = receberMensagem(i, 0, MPI_COMM_WORLD, c.falhaQuebra, 2.0);
        // Se valor for -1, significa que não recebemos nada (falha de quebra)
        if (mensagensRecebidas[i] == -1) {
            // Ex: se não recebemos nada, 0 (Recuar) como default
            mensagensRecebidas[i] = 0;
        }
    }

    // Segunda rodada: retransmitir tudo que foi recebido
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            // Se sou traidor e falha bizantina, posso enviar qualquer coisa
            // Exemplo: manda valor aleatório
            int valorParaEnviar = mensagensRecebidas[j];
            if (ehTraidor && c.falhaBizantina) {
                valorParaEnviar = rand() % 2;
            }

            // Se sou traidor e falha de quebra, opcionalmente não envio
            if (ehTraidor && c.falhaQuebra) {
                int lixo = -1;
                MPI_Send(&lixo, 1, MPI_INT, i, 1, MPI_COMM_WORLD);
            } else {
                MPI_Send(&valorParaEnviar, 1, MPI_INT, i, 1, MPI_COMM_WORLD);
            }
        }
    }

    // Recebe as retransmissões
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            int valor = receberMensagem(i, 1, MPI_COMM_WORLD, c.falhaQuebra, 2.0);
            if (valor == -1) {
                // Se não recebemos, considera recuo (0) por default
                valor = 0;
            }
            mensagensRecebidas[j] = valor;
        }
    }

    // Decisão final pela maioria
    int ataque = 0, recuo = 0;
    for (int i = 0; i < size; i++) {
        if (mensagensRecebidas[i] == 1) ataque++;
        else recuo++;
    }
    int decisaoFinal = (ataque > recuo) ? 1 : 0;

    // Impressão de resultado
    for (int r = 0; r < size; r++) {
        if (rank == r) {
            printf(" [Cenário] Traidores=%d FalhaBiz=%d FalhaQueb=%d | General %d => Decisão: %s\n",
                   c.numTraidores, c.falhaBizantina, c.falhaQuebra,
                   rank, (decisaoFinal ? "Atacar" : "Recuar"));
            fflush(stdout);
        }
        MPI_Barrier(MPI_COMM_WORLD); 
    }
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    Cenario cenarios[] = {
        {0, 0, 0},  // Cenário 1: 0 traidores, sem falha bizantina, sem falha de quebra
        {1, 1, 0},  // Cenário 2: 1 traidor, falha bizantina
        {1, 0, 1},  // Cenário 3: 1 traidor, falha de quebra
    };
    int numCenarios = sizeof(cenarios) / sizeof(Cenario);

    if (rank == 0) {
        printf("=== Algoritmo dos Generais Bizantinos ===\n");
        printf("=== Executando %d cenários em sequência ===\n", numCenarios);
        printf("    (Use 'mpirun -np 4 ./generais_bizantinos_simulacoes', por exemplo.)\n\n");
    }

    // Executa cada cenário sequencialmente
    for (int i = 0; i < numCenarios; i++) {
        // Sincroniza antes de iniciar cada cenário
        MPI_Barrier(MPI_COMM_WORLD);

        if (rank == 0) {
            printf("\n--- Iniciando Cenário %d ---\n", i+1);
        }
        MPI_Barrier(MPI_COMM_WORLD);

        rodarAlgoritmoGenerais(cenarios[i]);

        // Espera todos terminarem
        MPI_Barrier(MPI_COMM_WORLD);

        if (rank == 0) {
            printf("--- Fim do Cenário %d ---\n\n", i+1);
        }
    }

    if (rank == 0) {
        printf("=== Fim de todas as simulações (Generais Bizantinos) ===\n");
    }

    MPI_Finalize();
    return 0;
}
