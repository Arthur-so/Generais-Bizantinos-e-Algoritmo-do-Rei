#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/*
  Este código executa o Algoritmo do Rei em vários cenários:
   - Cenário 1: 0 traidores
   - Cenário 2: 1 traidor (falha bizantina)
   - Cenário 3: 1 traidor (falha de quebra)
*/

static const int TOTAL_GENERAIS = 5;

typedef struct {
    int numTraidores;
    int falhaBizantina;
    int falhaQuebra;
} Cenario;

int receberMensagem(
    int origem,
    int tag,
    MPI_Comm comm,
    int falhaQuebra,
    double timeoutSegundos
) {
    int valorRecebido = -1;
    if (!falhaQuebra) {
        MPI_Recv(&valorRecebido, 1, MPI_INT, origem, tag, comm, MPI_STATUS_IGNORE);
    } else {
        MPI_Request req;
        MPI_Status status;
        int flag = 0;
        double inicio = MPI_Wtime();

        MPI_Irecv(&valorRecebido, 1, MPI_INT, origem, tag, comm, &req);

        while (!flag) {
            MPI_Test(&req, &flag, &status);
            if (MPI_Wtime() - inicio > timeoutSegundos) {
                return -1; // não chegou nada
            }
        }
    }
    return valorRecebido;
}

void rodarAlgoritmoRei(Cenario c) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size != TOTAL_GENERAIS) {
        if (rank == 0) {
            printf("[ERRO] Código configurado para %d processos!\n", TOTAL_GENERAIS);
        }
        return;
    }

    srand(time(NULL) + rank);

    int ehTraidor = (rank < c.numTraidores) ? 1 : 0;

    // Decisão inicial: 0 = Recuar, 1 = Atacar
    int minhaDecisao = rand() % 2;

    // Se for traidor e falha bizantina, pode mudar aleatoriamente
    if (ehTraidor && c.falhaBizantina) {
        minhaDecisao = rand() % 2;
    }

    // Array para receber mensagens
    int mensagens[size];

    // Primeira rodada: enviar decisão para todos
    for (int i = 0; i < size; i++) {
        if (ehTraidor && c.falhaQuebra) {
            // Se for falha de quebra, não envia ou envia lixo
            int lixo = -1;
            MPI_Send(&lixo, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
        } else {
            MPI_Send(&minhaDecisao, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
        }
    }

    // Recebe as decisões
    for (int i = 0; i < size; i++) {
        int val = receberMensagem(i, 0, MPI_COMM_WORLD, c.falhaQuebra, 2.0);
        if (val == -1) {
            // Não chegou -> default
            val = 0; 
        }
        mensagens[i] = val;
    }

    // Segunda rodada: retransmitir mensagens recebidas
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            int valorParaEnviar = mensagens[j];
            // Se sou traidor e falha bizantina, posso alterar
            if (ehTraidor && c.falhaBizantina) {
                valorParaEnviar = rand() % 2;
            }
            if (ehTraidor && c.falhaQuebra) {
                int lixo = -1;
                MPI_Send(&lixo, 1, MPI_INT, i, 1, MPI_COMM_WORLD);
            } else {
                MPI_Send(&valorParaEnviar, 1, MPI_INT, i, 1, MPI_COMM_WORLD);
            }
        }
    }

    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            int val = receberMensagem(i, 1, MPI_COMM_WORLD, c.falhaQuebra, 2.0);
            if (val == -1) {
                val = 0;
            }
            mensagens[j] = val;
        }
    }

    // Decisão final pela maioria
    int ataque = 0, recuo = 0;
    for (int i = 0; i < size; i++) {
        if (mensagens[i] == 1) ataque++;
        else recuo++;
    }
    int decisaoFinal = (ataque > recuo) ? 1 : 0;

    // Algoritmo do Rei: desempate
    int rei = size - 1; // último processo é o Rei
    if (ataque == recuo) {
        if (rank == rei) {
            // Se sou o Rei e houve empate, escolho arbitrariamente
            decisaoFinal = rand() % 2;
        }
        // Broadcast para todos
        MPI_Bcast(&decisaoFinal, 1, MPI_INT, rei, MPI_COMM_WORLD);
    }

    for (int r = 0; r < size; r++) {
        if (rank == r) {
            printf(" [Rei] Traidores=%d Biz=%d Queb=%d | Rank=%d (Rei=%d) => Decisão: %s\n",
                   c.numTraidores, c.falhaBizantina, c.falhaQuebra,
                   rank, (rank == (size - 1)), (decisaoFinal ? "Atacar" : "Recuar"));
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
        {0, 0, 0},  // Cenário 1: 0 traidores
        {1, 1, 0},  // Cenário 2: 1 traidor, falha bizantina
        {1, 0, 1},  // Cenário 3: 1 traidor, falha de quebra
    };
    int numCenarios = sizeof(cenarios) / sizeof(Cenario);

    if (rank == 0) {
        printf("=== Algoritmo do Rei ===\n");
        printf("=== Executando %d cenários em sequência ===\n", numCenarios);
        printf("    (Use 'mpirun -np 5 ./algoritmo_rei_simulacoes', por exemplo.)\n\n");
    }

    for (int i = 0; i < numCenarios; i++) {
        MPI_Barrier(MPI_COMM_WORLD);

        if (rank == 0) {
            printf("\n--- Iniciando Cenário %d ---\n", i+1);
        }
        MPI_Barrier(MPI_COMM_WORLD);

        rodarAlgoritmoRei(cenarios[i]);

        MPI_Barrier(MPI_COMM_WORLD);
        if (rank == 0) {
            printf("--- Fim do Cenário %d ---\n\n", i+1);
        }
    }

    if (rank == 0) {
        printf("=== Fim de todas as simulações (Algoritmo do Rei) ===\n");
    }

    MPI_Finalize();
    return 0;
}
