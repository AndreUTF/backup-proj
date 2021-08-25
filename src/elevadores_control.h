#ifndef ELEVADORES_CONTROL_H_
#define ELEVADORES_CONTROL_H_

#include <stdint.h>
#include <stdbool.h>
#include "cmsis_os2.h" // CMSIS-RTOS

#define N_ANDARES 16
#define N_ELEVADORES  3

enum estados_elevadores {Inicial, Parado, Subindo, Descendo, Esperando};
enum estados_portas{Fechada, Aberta};

typedef struct
{
  int estado; // Inicial, parado, esperando, subindo ou descendo
  int andar_atual;
  int estado_anterior; 
  int prox_andar;
  int prox_sentido;
  int estado_porta; // aberto e fechado
  char comandos[8]; // comando a ser interpretado pelo simulador
  osMessageQueueId_t fila_comandos; // fila de mensagens para receber os comandos que vem do simulador
  bool andares_subindo[N_ANDARES]; // andares que estão chamando esse elevador para subrir
  bool andares_descendo[N_ANDARES]; // andares que estão chamando esse elevador para descer
}elevator;



void Thread_Elevador(void *arg);
void Gerenciador(void *arg);
void set_init_values(elevator *aux);
void set_values(int *state,int *previous_state, int *floor, int *actual_floor, elevator *aux);
void monta_comando(char elevador,char *comando, char* repositorio);
void adiciona_andar(elevator *aux_elevador, int andar);
void Acha_Proximo_Andar(elevator *aux);
void timer_portas(void *arg);

#endif //ELEVADORES_CONTROL_H_