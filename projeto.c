#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>


#define NUM_FOGAO 1
#define NUM_PANELA 1
#define NUM_FORNO 1
#define NUM_BANCADA 1
#define NUM_GRILL 1
#define NUM_RECEITAS 6
#define NUM_COZINHEIROS 4


// Semáforos para os recursos
sem_t sem_fogao;
sem_t sem_panela;
sem_t sem_forno;
sem_t sem_bancada;
sem_t sem_grill;
sem_t sem_cozinheiros;


pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t recurso_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cozinheiros_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cozinheiros_disponiveis_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cozinheiro_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

pthread_t cozinheiro_tid[NUM_COZINHEIROS];

// Vetor para rastrear IDs de cozinheiros
int id_disponiveis[NUM_COZINHEIROS];


typedef struct {
    sem_t** semaphores;          // Array de ponteiros para semáforos
    char** resource_names;       // Array de nomes dos recursos
    pthread_mutex_t* mutexes;    // Array de mutexes para cada recurso
    pthread_cond_t* conds;       // Array de variáveis de condição para cada recurso
    int size;                    // Tamanho do array de semáforos
} SemaphoreArray;

typedef struct {
    char* key;               // Chave (por exemplo, string)
    SemaphoreArray value;    // Valor (array de semáforos)
} DictionaryEntry;

typedef struct {
    DictionaryEntry* entries; // Array de entradas do dicionário
    int size;                 // Número de entradas no dicionário
} Dictionary;

typedef struct {
    int id;              // ID do cozinheiro
    char* receita;       // Receita que o cozinheiro vai preparar
} CozinheiroInfo;

typedef struct Node {
    int id;
    struct Node* next;
} Node;

Dictionary receitas;
Node* head = NULL;
Node* tail = NULL;

void initDictionary(Dictionary* dict, int initialSize) {
    dict->entries = (DictionaryEntry*)malloc(initialSize * sizeof(DictionaryEntry));
    dict->size = 0;
}

int countSemaphores(sem_t** semaphoreArray) {
    int count = 0;
    while (semaphoreArray[count] != NULL) {
        count++;
    }
    return count;
}

void addSemaphoreArray(Dictionary* dict, const char* key, sem_t** semaphoreArray, char** resourceNames, int size) {
    dict->entries[dict->size].key = strdup(key);
    dict->entries[dict->size].value.semaphores = semaphoreArray;
    dict->entries[dict->size].value.resource_names = resourceNames;
    dict->entries[dict->size].value.size = size;

    // Alocando espaço para os mutexes e variáveis de condição
    dict->entries[dict->size].value.mutexes = malloc(size * sizeof(pthread_mutex_t));
    dict->entries[dict->size].value.conds = malloc(size * sizeof(pthread_cond_t));

    // Inicializando mutexes e variáveis de condição
    for (int i = 0; i < size; i++) {
        pthread_mutex_init(&dict->entries[dict->size].value.mutexes[i], NULL);
        pthread_cond_init(&dict->entries[dict->size].value.conds[i], NULL);
    }

    dict->size++;
}

SemaphoreArray* getSemaphoreArray(Dictionary* dict, const char* key) {
    for (int i = 0; i < dict->size; i++) {
        if (strcmp(dict->entries[i].key, key) == 0) {
            return &dict->entries[i].value;
        }
    }
    return NULL; // Se a chave não for encontrada
}

void freeDictionary(Dictionary* dict) {
    for (int i = 0; i < dict->size; i++) {
        free(dict->entries[i].key);
        free(dict->entries[i].value.semaphores);

        // Destruindo mutexes e variáveis de condição
        for (int j = 0; j < dict->entries[i].value.size; j++) {
            pthread_mutex_destroy(&dict->entries[i].value.mutexes[j]);
            pthread_cond_destroy(&dict->entries[i].value.conds[j]);
        }

        free(dict->entries[i].value.mutexes);
        free(dict->entries[i].value.conds);
    }
    free(dict->entries);
}

void mostrarCozinheirosDisponiveis() {
    pthread_mutex_lock(&cozinheiros_disponiveis_lock); // Protege o acesso ao array de IDs disponíveis
        printf("Cozinheiros disponíveis: ");
        int disponiveis = 0;
        for (int i = 0; i < NUM_COZINHEIROS; i++) {
            if (id_disponiveis[i] == 1) {
                printf("%d ", i + 1);  // IDs dos cozinheiros começam em 1
                disponiveis++;
            }
        }
        
        if (disponiveis == 0) {
            printf("Nenhum cozinheiro disponível.");
        }
        
        printf("\n");

    pthread_mutex_unlock(&cozinheiros_disponiveis_lock); // Libera o mutex
}


void enqueue(int id) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->id = id;
    newNode->next = NULL;

    pthread_mutex_lock(&queue_lock);
        if (tail == NULL) {
            head = tail = newNode;
        } else {
            tail->next = newNode;
            tail = newNode;
        }
        pthread_cond_signal(&queue_cond);
    pthread_mutex_unlock(&queue_lock);
}

int dequeue() {
    pthread_mutex_lock(&queue_lock);
        while (head == NULL) {
            pthread_cond_wait(&queue_cond, &queue_lock);
        }
        Node* temp = head;
        int id = temp->id;
        head = head->next;
        if (head == NULL) {
            tail = NULL;
        }
        free(temp);
    pthread_mutex_unlock(&queue_lock);
    return id;
}


void* cozinheiro(void* arg) {
    CozinheiroInfo* info = (CozinheiroInfo*)arg;
    char* receita = info->receita;
    int id_cozinheiro = info->id;
    SemaphoreArray* recursos = getSemaphoreArray(&receitas, receita);

    printf("Cozinheiro %d começando %s \n", id_cozinheiro, receita);

    if (recursos != NULL) {
        for (int i = 0; i < recursos->size; i++) {
            sem_t* recurso = recursos->semaphores[i];
            char* nome_recurso = recursos->resource_names[i];
            pthread_mutex_t* mutex = &recursos->mutexes[i];
            pthread_cond_t* cond = &recursos->conds[i];

            printf("Cozinheiro %d necessita usar o recurso %s para fazer %s.\n", id_cozinheiro, nome_recurso, receita);

            pthread_mutex_lock(mutex);
                while (1) {
                    int recurso_disponivel;
                    sem_getvalue(recurso, &recurso_disponivel);

                    if (recurso_disponivel == 0) {
                        printf("%s está indisponível para o cozinheiro %d fazer %s \n", nome_recurso, id_cozinheiro, receita);
                        pthread_cond_wait(cond, mutex);  // Espera até que o recurso esteja disponível
                    
                    } else {
                        sem_wait(recurso);
                            printf("Cozinheiro %d está usando o recurso %s para fazer %s.\n", id_cozinheiro, nome_recurso, receita);

                            int tempo = rand() % 10 + 5;
                            sleep(tempo);

                            printf("Cozinheiro %d finalizou o uso de %s para fazer %s.\n", id_cozinheiro, nome_recurso, receita);

                        sem_post(recurso);
                        
                        pthread_cond_broadcast(cond);  // Sinaliza que o recurso está disponível novamente
                        break;
                    }
                }
            pthread_mutex_unlock(mutex);
        }

        printf("Receita %s finalizada pelo Cozinheiro %d.\n", receita, id_cozinheiro);
    }

    pthread_mutex_lock(&cozinheiros_lock);
        sem_post(&sem_cozinheiros);
        id_disponiveis[id_cozinheiro - 1] = 1;  // Marca o ID como disponível novamente
        pthread_cond_signal(&cozinheiro_cond);
    pthread_mutex_unlock(&cozinheiros_lock);

    free(info); // Liberar a memória alocada para a estrutura
    pthread_exit(0);
}

int main() {
    initDictionary(&receitas, NUM_RECEITAS);

    // Inicializando os recursos das receitas em forma de semáforo
    sem_init(&sem_fogao, 0, NUM_FOGAO);
    sem_init(&sem_panela, 0, NUM_PANELA);
    sem_init(&sem_forno, 0, NUM_FORNO);
    sem_init(&sem_bancada, 0, NUM_BANCADA);
    sem_init(&sem_grill, 0, NUM_GRILL);
    sem_init(&sem_cozinheiros, 0, NUM_COZINHEIROS);

    const char* nomes_recursos_file[] = { "Fogão", "Panela", "Bancada" };
    const char* nomes_recursos_lasanha[] = { "Fogão", "Panela", "Forno", "Bancada" };
    const char* nomes_recursos_risoto[] = { "Fogão", "Panela", "Bancada"};
    const char* nomes_recursos_salmao[] = { "Grill", "Bancada" };
    const char* nomes_recursos_pizza[] = { "Forno", "Bancada" };
    const char* nomes_recursos_costela[] = { "Bancada", "Panela","Forno","Grill"};

    // Criando arrays de ponteiros para os semáforos e terminando com NULL
    sem_t* recursos_file[] = { &sem_fogao, &sem_panela, &sem_bancada, NULL };
    sem_t* recursos_lasanha[] = { &sem_fogao, &sem_panela, &sem_forno, &sem_bancada, NULL };
    sem_t* recursos_risoto[] = { &sem_fogao, &sem_panela, &sem_bancada, NULL };
    sem_t* recursos_salmao[] = { &sem_grill, &sem_bancada, NULL };
    sem_t* recursos_pizza[] = { &sem_forno, &sem_bancada, NULL };
    sem_t* recursos_costela[] = { &sem_bancada, &sem_panela,&sem_forno,&sem_grill, NULL };

    addSemaphoreArray(&receitas, "file", recursos_file, nomes_recursos_file, 3);
    addSemaphoreArray(&receitas, "lasanha", recursos_lasanha, nomes_recursos_lasanha, 4);
    addSemaphoreArray(&receitas, "risoto", recursos_risoto, nomes_recursos_risoto, 3);
    addSemaphoreArray(&receitas, "salmao", recursos_salmao, nomes_recursos_salmao, 2);
    addSemaphoreArray(&receitas, "pizza", recursos_pizza, nomes_recursos_pizza, 2);
    addSemaphoreArray(&receitas, "costela", recursos_costela, nomes_recursos_costela, 4);

    char* menu[NUM_RECEITAS] = {"file", "lasanha", "risoto", "salmao", "pizza", "costela"};

    // Inicializar todos os IDs como disponíveis
    for (int i = 0; i < NUM_COZINHEIROS; i++) {
        id_disponiveis[i] = 1;  // 1 significa disponível
    }

    printf("Restaurante abriu!\n");

    int quant_loop = 0;
    while (quant_loop < 10) {
        pthread_mutex_lock(&cozinheiros_lock);
            int quant_cozinheiros;
            sem_getvalue(&sem_cozinheiros,&quant_cozinheiros);
            
            printf("Quantidade de cozinheiros disponiveis: %d\n", quant_cozinheiros);
            
            while (quant_cozinheiros == 0) {
                pthread_cond_wait(&cozinheiro_cond, &cozinheiros_lock);
                sem_getvalue(&sem_cozinheiros,&quant_cozinheiros);
            }
            
            // Procurar um ID disponível
            int id_disponivel = -1;
            for (int i = 0; i < NUM_COZINHEIROS; i++) {
                if (id_disponiveis[i] == 1) {
                    id_disponivel = i;
                    enqueue(i + 1);  // Adiciona o ID à fila FIFO
                    id_disponiveis[i] = 0;  // Marcar o ID como em uso
                    break;
                }
            }

            //mostrarCozinheirosDisponiveis();

            // Esperar até que seja a vez do próximo cozinheiro
            if (id_disponivel != -1) {
                id_disponivel = dequeue();  // Remove da fila FIFO
                sem_wait(&sem_cozinheiros);
            }
        pthread_mutex_unlock(&cozinheiros_lock);

        if (id_disponivel != -1) {
            int randomIndex = rand() % NUM_RECEITAS;
            char* randomReceita = menu[randomIndex];

            // Alocar memória para a estrutura CozinheiroInfo e atribuir valores
            CozinheiroInfo* info = (CozinheiroInfo*)malloc(sizeof(CozinheiroInfo));
            info->id = id_disponivel;  // Atribuir ID disponível
            info->receita = randomReceita;

            pthread_create(&cozinheiro_tid[id_disponivel - 1], NULL, cozinheiro, (void*)info);
        }

        quant_loop++;
    }

    // Esperar todas as threads terminarem
    for (int i = 0; i < NUM_COZINHEIROS; i++) {
        pthread_join(cozinheiro_tid[i], NULL);
    }


    freeDictionary(&receitas);
    sem_destroy(&sem_fogao);
    sem_destroy(&sem_panela);
    sem_destroy(&sem_forno);
    sem_destroy(&sem_bancada);
    sem_destroy(&sem_grill);
    sem_destroy(&sem_cozinheiros);
    pthread_mutex_destroy(&print_lock);
    pthread_mutex_destroy(&recurso_lock);
    pthread_mutex_destroy(&cozinheiros_lock);
    pthread_cond_destroy(&cozinheiro_cond);


    return 0;
}