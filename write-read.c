#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

#define NUM_THREADS 2
#define LIST_SIZE 10000

int get_random(int low, int high) {
    return rand() % (high - low + 1) + low;
}

struct ascendente {
    bool (*compare)(int x, int y);
};

bool compare_ascendente(int x, int y) {
    return x <= y;
}

struct nodo {
    int valor;
    struct nodo* next;
    pthread_rwlock_t rw_lock;
};

typedef struct nodo nodo;

struct LE {
    nodo* head;
    nodo* tail;
    pthread_rwlock_t list_rwlock;
};

typedef struct LE LE;

LE* create_list() {
    LE* list = (LE*)malloc(sizeof(LE));
    list->head = (nodo*)malloc(sizeof(nodo));
    list->tail = (nodo*)malloc(sizeof(nodo));

    list->head->valor = -2147483648;
    list->head->next = list->tail;
    list->tail->valor = 2147483647;
    list->tail->next = NULL;

    pthread_rwlock_init(&list->list_rwlock, NULL);

    return list;
}

void print_list(LE* list) {
    nodo* current = list->head->next;
    printf("Head->");
    while (current != list->tail) {
        printf("%d->", current->valor);
        current = current->next;
    }
    printf("NULL\n");
}

bool find_element(LE* list, int v, nodo** pos) {
    struct ascendente comp;
    bool encontrado = false;
    *pos = list->head;

    pthread_rwlock_rdlock(&list->list_rwlock);
    for (nodo* p = list->head; p != list->tail && comp.compare(p->valor, v); p = p->next) {
        if (p->valor == v) {
            encontrado = true;
            break;
        }
        *pos = p;
    }
    pthread_rwlock_unlock(&list->list_rwlock);

    return encontrado;
}

bool contains(LE* list, int v) {
    nodo* pos_ant;
    return find_element(list, v, &pos_ant);
}

bool weak_search(LE* list, nodo** padre, nodo** hijo, int valor) {
    *padre = list->head;
    *hijo = (*padre)->next;
    while (*hijo && (*hijo)->valor < valor) {
        *padre = *hijo;
        *hijo = (*padre)->next;
    }
    return true;
}

bool strong_search(LE* list, nodo** padre, nodo** hijo, int valor) {
    weak_search(list, padre, hijo, valor);
    pthread_rwlock_rdlock(&(*padre)->rw_lock);
    *hijo = (*padre)->next;
    while (*hijo && (*hijo)->valor < valor) {
        pthread_rwlock_unlock(&(*padre)->rw_lock);
        *padre = *hijo;
        pthread_rwlock_rdlock(&(*padre)->rw_lock);
        *hijo = (*padre)->next;
    }
    return true;
}

bool insert_thread(LE* list, int valor) {
    nodo* padre;
    nodo* hijo;
    nodo* aux;
    strong_search(list, &padre, &hijo, valor);
    if (hijo && hijo->valor == valor) {
        hijo->valor = valor;
    } else {
        aux = (nodo*)malloc(sizeof(nodo));
        aux->valor = valor;
        pthread_rwlock_wrlock(&padre->rw_lock);
        padre->next = aux;
        aux->next = hijo;
        pthread_rwlock_unlock(&padre->rw_lock);
    }
    pthread_rwlock_unlock(&padre->rw_lock);
    return true;
}

bool delete_thread(LE* list, int valor) {
    nodo* padre;
    nodo* hijo;
    strong_search(list, &padre, &hijo, valor);
    if (hijo && hijo->valor == valor) {
        pthread_rwlock_wrlock(&padre->rw_lock);
        padre->next = hijo->next;
        pthread_rwlock_unlock(&padre->rw_lock);
        free(hijo);
    }
    pthread_rwlock_unlock(&padre->rw_lock);
    return true;
}

void* add_thread(void* arg) {
    LE* list = (LE*)arg;
    int min = 1;
    int max = LIST_SIZE;

    for (int i = 0; i < LIST_SIZE / 2; i++) {
        int x = get_random(min, max);
        insert_thread(list, x);
    }

    return NULL;
}

void* delete_thread_function(void* arg) {
    LE* list = (LE*)arg;
    int min = 1;
    int max = LIST_SIZE;

    for (int i = 0; i < LIST_SIZE / 20; i++) {
        int x = get_random(min, max);
        delete_thread(list, x);
    }

    return NULL;
}

int main() {
    srand(time(NULL));
    LE* lista = create_list();
    pthread_t threads[NUM_THREADS];

    clock_t t0, t1;
    t0 = clock();

    pthread_create(&threads[0], NULL, add_thread, lista);
    pthread_create(&threads[1], NULL, delete_thread_function, lista);

    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);

    print_list(lista);

    t1 = clock();
    double time = (double)(t1 - t0) / CLOCKS_PER_SEC;
    printf("Execution Time: %lf\n", time);

    free(lista->head);
    free(lista->tail);
    free(lista);
    return 0;
}

