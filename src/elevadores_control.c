#include "elevadores_control.h"

#include "system_tm4c1294.h" // CMSIS-Core
#include "cmsis_os2.h" // CMSIS-RTOS
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/pin_map.h"
#include "driverlib/gpio.h"
#include "driverlib/uart.h"

extern char letras[4];

extern elevator elevador[N_ELEVADORES];
extern osThreadId_t Elevador_id[N_ELEVADORES];
extern osThreadId_t Gerencia_Elevador_id[N_ELEVADORES];
extern osMutexId_t function_mutex_id; // parametro que define qual mutex está sendo usado, nesse caso só tem 1
extern osMessageQueueId_t fila_uart_tx_id;
extern osTimerId_t timer_das_portas_id[N_ELEVADORES]; // não foi usado timer

char vetor_botao_interno[] = {'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p'};
char vetor_botao_externo[] = {'0','1','2','3','4','5','6','7','8','9'};

void Thread_Elevador(void *arg)
{
  int n;
  n = (int)arg;
  set_init_values(&elevador[n]);
  elevador[n].andares_subindo[0] = 1;
  char order[8];
  while(1)
  {   
    switch(elevador[n].estado)
    {
    case Inicial:
      osThreadFlagsWait(0x0001, osFlagsWaitAny, osWaitForever); // bloqueia a thread até receber a flag 0x0001 (no primeiro ciclo vai estar bloqueado)
      elevador[n].andares_subindo[0] = 0;
      monta_comando(letras[n], "r", order); // monta o comndo que reseta o elevador (deveria mandar para o 0 e abrir a porta)
      osMessageQueuePut(fila_uart_tx_id, order ,4 ,osWaitForever); // coloca esse comando na fila de mensagens que sai dos elevadores
      elevador[n].estado = Parado;
      break;
    case Parado:
      osThreadFlagsWait(0x0001, osFlagsWaitAny, osWaitForever); // bloqueia a thread até receber a flag 0x0001 (no primeiro ciclo vai estar bloqueado)
      if(elevador[n].andar_atual < elevador[n].prox_andar  && elevador[n].prox_andar  != (-1)) // (-1) usado´para indicar que não tem próximo andar 
      {
        monta_comando(letras[n], "f", order);                        // monta ocomando para fechar portas
        osMessageQueuePut(fila_uart_tx_id, order ,4 ,osWaitForever); // envia para fila de mensagens que sai dos elevadores
        while(elevador[n].estado_porta != Fechada);                  // Só funciona pq tem Round Robin por tras (espera fechar a porta);
        monta_comando(letras[n], "s", order);                        // monta comando para subir
        osMessageQueuePut(fila_uart_tx_id, order ,4 ,osWaitForever); // envia para fila de mensagens que sai dos elevadores
        elevador[n].estado = Subindo;
      }
      else if(elevador[n].andar_atual > elevador[n].prox_andar && elevador[n].prox_andar  != (-1))
      {
        monta_comando(letras[n], "f", order);                        // monta ocomando para fechar portas
        osMessageQueuePut(fila_uart_tx_id, order ,4 ,osWaitForever); // envia para fila de mensagens que sai dos elevadores
        while(elevador[n].estado_porta != Fechada);                  // Só funciona pq tem Round Robin por tras (espera fechar a porta);
        monta_comando(letras[n], "d", order);                        // monta comando para descer
        osMessageQueuePut(fila_uart_tx_id, order ,4 ,osWaitForever); // envia para fila de mensagens que sai dos elevadores
        elevador[n].estado_anterior = elevador[n].estado;
        elevador[n].estado = Descendo;
      }
      else
      {
        elevador[n].estado = Parado; // se não houver proxima requisição o elevador vai para Parado
      }
      break;
    case Subindo:
      if ((elevador[n].andar_atual == elevador[n].prox_andar) && elevador[n].prox_andar  != (-1)) // está subindo e chegou no destino
      {
        osDelay(120);                                                // delay para chegar na altura certa para abrir a porta (valor empírico)
        monta_comando(letras[n], "p", order);                        // monta comando para parar
        osMessageQueuePut(fila_uart_tx_id, order ,5 ,osWaitForever); // envia para fila de mensagens que sai dos elevadores
        if(elevador[n].prox_andar != (-1))                           // se ainda houver proximo andar                      
        {
          sprintf(order,"%cD%c", letras[n], vetor_botao_interno[elevador[n].prox_andar]); // monta comando para desligar o botão interno
          osMessageQueuePut(fila_uart_tx_id, order ,4 ,osWaitForever);                    // envia para fila de mensagens que sai dos elevadores
        }
        elevador[n].estado_anterior = elevador[n].estado;
        elevador[n].estado = Esperando;
        elevador[n].andares_subindo[elevador[n].prox_andar] = 0;  // retira o andar atingido tanto da fila de andares subindo
        elevador[n].andares_descendo[elevador[n].prox_andar] = 0; // quanto de descendo, assumindo que todos entraram, independente do sentido
      }
      else if (elevador[n].prox_andar == (-1)) // se não houver próxima requisição
      {
        elevador[n].estado = Parado;
      }
      break;
    case Descendo:
      if ((elevador[n].andar_atual == elevador[n].prox_andar) && elevador[n].prox_andar != (-1)) // está descendo e chegou no destino
      {
        monta_comando(letras[n], "p", order);                        // monta comando para parar
        osMessageQueuePut(fila_uart_tx_id, order ,4 ,osWaitForever); // envia para fila de mensagens que sai dos elevadores
        if(elevador[n].prox_andar != (-1))                           // se ainda houver proximo andar                      
        {
          sprintf(order,"%cD%c", letras[n], vetor_botao_interno[elevador[n].prox_andar]); // monta comando para desligar o botão interno
          osMessageQueuePut(fila_uart_tx_id, order ,4 ,osWaitForever);                    // envia para fila de mensagens que sai dos elevadores
          elevador[n].andares_subindo[elevador[n].prox_andar] = 0;                        // retira o andar atingido tanto da fila de andares subindo
          elevador[n].andares_descendo[elevador[n].prox_andar] = 0;                       // quanto de descendo, assumindo que todos entraram, independente do sentido
        }
        elevador[n].estado_anterior = elevador[n].estado;
        elevador[n].estado = Esperando;
      }
      else if (elevador[n].prox_andar == (-1)) // se não houver próxima requisição
      {
        elevador[n].estado = Parado;
      }
      break;
    case Esperando:
        monta_comando(letras[n], "a", order);                         // monta o comando para abrir porta
        osMessageQueuePut(fila_uart_tx_id, order ,4 ,osWaitForever);  // envia para fila de mensagens que sai dos elevadores 
        osDelay(5000);                                                // mantem aberta por 5 segundos
        osMutexAcquire(function_mutex_id, osWaitForever);             // protege a função de achar próximo andar                
        Acha_Proximo_Andar(&elevador[n]);                             // função que define qual será o próximo andar do elevador
        osMutexRelease(function_mutex_id);                            // encerra o uso do mutex
        if (elevador[n].prox_andar == (-1))                           // se não houver próximo andar
        {
          elevador[n].estado_anterior = elevador[n].estado;
          elevador[n].estado = Parado;
        }
        else if (elevador[n].andar_atual > elevador[n].prox_andar)   // se o próximo andar estiver abaixo do atual
        {
          elevador[n].estado_anterior = elevador[n].estado;
          elevador[n].estado = Descendo;
          monta_comando(letras[n], "f", order);                        // monta o comando para fechar a porta
          osMessageQueuePut(fila_uart_tx_id, order ,4 ,osWaitForever); // envia para fila de mensagens que sai dos elevadores 
          while(elevador[n].estado_porta != Fechada);                  // espera a porta fechar
          monta_comando(letras[n], "d", order);                        // monta o comando para o elevador descer
          osMessageQueuePut(fila_uart_tx_id, order ,4 ,osWaitForever); // envia para fila de mensagens que sai dos elevadores
        }
        else if (elevador[n].andar_atual < elevador[n].prox_andar)     // se o próximo andar está acima do atual
        {
          elevador[n].estado_anterior = elevador[n].estado;
          elevador[n].estado = Subindo;
          monta_comando(letras[n], "f", order);                        // monta o comando para fechar a porta
          osMessageQueuePut(fila_uart_tx_id, order ,4 ,osWaitForever); // envia para fila de mensagens que sai dos elevadores
          while(elevador[n].estado_porta != Fechada);                  // espera a porta fechar
          monta_comando(letras[n], "s", order);                        // monta o comando para o elevador descer
          osMessageQueuePut(fila_uart_tx_id, order ,4 ,osWaitForever); // envia para fila de mensagens que sai dos elevadores
        }
        else
        {
          elevador[n].estado_anterior = elevador[n].estado;
          elevador[n].estado = Parado;
        }
      break;
    }
    osMutexAcquire(function_mutex_id, osWaitForever); // protege a função de achar próximo andar                
    Acha_Proximo_Andar(&elevador[n]);                 // função que define qual será o próximo andar do elevador
    osMutexRelease(function_mutex_id);                // encerra o uso do mutex
  }
} // Thread_Elevador



void Gerenciador(void *arg)
{ 
  int n = 0, i;
  char  comand[8];
  int aux_num;
  uint8_t prioridade_elevador;
  i = (int)arg; // saber qual gerenciador está em uso
  while(1)
  {
    if(osMessageQueueGet(elevador[i].fila_comandos, &elevador[i].comandos, &prioridade_elevador, 1000) == osOK) // função para bloquear a thread caso não haja alteração, desbloqueando novamente em 1s
    {
      aux_num = (-1);
      n = 0;
      if(elevador[i].comandos[1] == 'A'){elevador[i].estado_porta = Aberta;}
      else if(elevador[i].comandos[1] == 'F'){elevador[i].estado_porta = Fechada;}
      else if(elevador[i].comandos[1] == 'I') // I = interno
      {
      while((elevador[i].comandos[2] != vetor_botao_interno[n]) && n < N_ANDARES) // descobre para qual andar a pessoa quer ir
        {
          n++;
        }
        if(n<N_ANDARES)
        {
          if(elevador[i].andar_atual != n) // se não está no andar desejado
          {
            adiciona_andar(&elevador[i], n); // coloca 1 na posição do andar desejado no vetor andares_subindo se for acima ou andares_descendo se for abaixo
            sprintf(comand,"%cL%c", letras[i], vetor_botao_interno[n]); // escreve o comando que acende a luz do botão interno do elevador
            osMessageQueuePut(fila_uart_tx_id, comand ,4 ,osWaitForever); // adiciona o comando na fila de mensagens da uart
          }
        }
      }
      else if(elevador[i].comandos[1] == 'E') // botão externo
      {
        sscanf(elevador[i].comandos,"%*c%*c%d%*c", &aux_num); // verifica apenas os dois dígitos do andar para saber que andar chamou
        if(aux_num>=0 && aux_num<=15)
        {
          if      (elevador[i].comandos[4] == 's' ){elevador[i].andares_subindo[aux_num] = 1;}
          else if (elevador[i].comandos[4] == 'd' ){elevador[i].andares_descendo[aux_num] = 1;}
        }
      }
      else
      {
        sscanf(elevador[i].comandos,"%*c%d", &aux_num);
        if(aux_num<=15 && aux_num >= 0){elevador[i].andar_atual = aux_num;} // atualiza o valor do antar em que o elevador se encontra
      }
    }
    
    if(elevador[i].estado == Parado || elevador[i].estado == Inicial)
    {
      osThreadFlagsSet(Elevador_id[i], 0x0001); // desbloqueia thread
    } 
  }
} // Thread_Gerencia_Elevador

void set_init_values(elevator *aux) // seta valores iniciais
{
  int i;
  aux->estado = Inicial;
  aux->estado_anterior = Parado;
  aux->andar_atual = 0;
  aux->prox_andar = (-1);
  aux->estado_porta = Fechada;
  aux->prox_sentido = Parado;
  for(i=0;i<N_ANDARES;i++)
  {
    aux->andares_subindo[i] = 0;
    aux->andares_descendo[i] = 0;
  }
}

void adiciona_andar(elevator *aux_elevador, int andar)
{
    if (aux_elevador->andar_atual < andar)
    {
      aux_elevador->andares_subindo[andar] = 1;
    }
    else if (aux_elevador->andar_atual > andar)
    {
      aux_elevador->andares_descendo[andar] = 1;
    }
}

void monta_comando(char elevador,char *comando, char* repositorio)
{
  int i = 0;
  repositorio[0] = elevador;
  while(comando[i])
  {
    repositorio[1+i] = comando[i];
    i++;
  }
}

void Acha_Proximo_Andar(elevator *aux)
{
  int i = -1, i2 = N_ANDARES-1;
  if (aux->estado == Inicial)
  {
    i = 0;
  }
  else if(aux->estado == Subindo)                                     // se está subindo
  {
    i = aux->andar_atual;
    if(aux->prox_sentido == Subindo)                                  // e o próximo sentido vai ser subindo também
    {
      while(aux->andares_subindo[i] != 1 && i <= N_ANDARES-1){i++;}   // verifica se tem um andar acima do atual que vai subir
    }
    else if(aux->prox_sentido == Descendo)                            // se próximo sentido for descendo
    {
      while(aux->andares_descendo[i] != 1 < N_ANDARES){i++;}          // verifica se tem um andar acima do atual que vai descer
    }
    else
    {
      i = (-1);                                                       
    }
  }
  else if (aux->estado == Descendo)                            // se está decsendo
  {
    i = aux->andar_atual;
    if(aux->prox_sentido == Subindo)                           // e o próximo sentido vai ser subindo
    {
      while(aux->andares_subindo[i] != 1 && i >= (-1)){i--;}   // verifica se tem um andar abaixo do atual que vai subir
    }
    else if(aux->prox_sentido == Descendo)                     // se próximo sentido for descendo
    {
      while(aux->andares_descendo[i] != 1 && i >= (-1)){i--;}  // verifica se tem um andar abaixo do atual que vai descer
    }
    else
    {
      i = (-1);
    }
  }
  else if(aux->estado == Esperando)                                     // se está esperando
  {
    i = aux->andar_atual;
    if(aux->prox_sentido == Subindo)                                    // e o próximo sentido vai ser subindo
    {
      while(aux->andares_subindo[i] != 1 && i <= (N_ANDARES-1)){i++;}   // verifica se tem um andar acima do atual que vai subir
    }
    else if(aux->prox_sentido == Descendo)                              // se próximo sentido for descendo
    {
      while(aux->andares_descendo[i] != 1 && i >= 0){i--;}              // verifica se tem um andar abaixo do atual que vai descer
    }
    else
    {
      i = (-1);
    }
  }
  else if (aux->estado == Parado)                                      // se etá parado
  {
    i = 0;
    i2 = N_ANDARES-1;
    while(aux->andares_subindo[i]  != 1 && i <= (N_ANDARES-1)){i++;}   // verifica se algum andar vai subir
    while(aux->andares_descendo[i2] != 1 && i2 >= 0){i2--;}            // verifica se algum andar vai descer
    if(i2 > (-1) && i < N_ANDARES)
    {
      if(((aux->andar_atual) - i2) < (i - (aux->andar_atual))) // se a proxima requisição para descer for mais perto de que a de subir
      {
        i = i2;
        aux->prox_sentido = Descendo;
      }
      else                                                     // caso contrário
      {
        aux->prox_sentido = Subindo;
      }
    }
    else if(i2 > (-1) && i >= N_ANDARES) // se só houver requisição para descer
    {
      i = i2;
      aux->prox_sentido = Descendo;
    }
    else if(i2 <=(-1) && i < N_ANDARES)  // se só houver requisição para subir
    {
      aux->prox_sentido = Subindo;
    }
    else
    {
      i = (-1);
    }
  }
  if(i<0 || i>=N_ANDARES)
  {
    i = (-1);
  }
  aux->prox_andar = i; // seta o próximo andar na variável do elevador
}

//
//void timer_portas(void *arg)
//{
//  int aux = (int)arg;
//  osThreadFlagsSet(Elevador_id[aux], 0x0003);
//} // callback

 