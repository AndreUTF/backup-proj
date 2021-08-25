#include <stdbool.h>
#include <stdio.h>
#include "cmsis_os2.h" // CMSIS-RTOS

// includes da biblioteca driverlib
#include "inc/hw_memmap.h"
#include "driverlib/gpio.h"
#include "driverlib/uart.h"
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"
#include "utils/uartstdio.h"
#include "system_TM4C1294.h"

typedef struct // STRUCT
{
    char string[30];
} message; //STRUCT

uint8_t floors_leftElevator[16] = {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t floors_centralElevator[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t floors_rightElevator[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

osThreadId_t LeftElevator_id, CentralElevator_id, RightElevator_id, MainTask_id, MainTask1_id;
osMessageQueueId_t LeftElevatorMessageQueue_id, CentralElevatorMessageQueue_id, RightElevatorMessageQueue_id;
osMutexId_t uart_id;
message LeftElevator_msg, CentralElevator_msg, RightElevator_msg;
bool LeftElevator_readUART = false;
bool CentralElevator_readUART = false;
bool RightElevator_readUART = false;
char buffer[30];

//----------
// UART definitions
extern void UARTStdioIntHandler(void);

void UARTInit(void){
  // Enable UART0
  SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
  while(!SysCtlPeripheralReady(SYSCTL_PERIPH_UART0));

  // Initialize the UART for console I/O.
  UARTStdioConfig(0, 115200, SystemCoreClock);

  // Enable the GPIO Peripheral used by the UART.
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
  while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOA));

  // Configure GPIO Pins for UART mode.
  GPIOPinConfigure(GPIO_PA0_U0RX);
  GPIOPinConfigure(GPIO_PA1_U0TX);
  GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
} // UARTInit

void UART0_Handler(void){
  UARTStdioIntHandler();
} // UART0_Handler

// osRtxIdleThread
__NO_RETURN void osRtxIdleThread(void *argument){
  (void)argument;
  
  while(1){
    //UARTprintf("Idle thread\n");
    asm("wfi");
  } // while
} // osRtxIdleThread

// osRtxTimerThread

void LeftElevator(void *arg){
  message msg;
  bool isAscending = false;
  bool isGoingDown = false;
  bool isWaitingCmd = false;
  bool isStopped = true;
  bool isExternalButtonPressed = false;
  bool isInternalButtonPressed = false;
  uint8_t level;
  uint8_t state;
  uint32_t tick;
  char string[30];
  char read_buffer[30];
  int currentFloor = 0;
  while(1){
    if(LeftElevator_readUART == true)
    {
      state = osMessageQueueGet(LeftElevatorMessageQueue_id, &msg, NULL, osWaitForever);
      //
      if(state == osOK)
      {
        strcpy(read_buffer,"");
        strcpy(string,"");
        strcpy(string, msg.string);
        if(isStopped == true)
        {
          UARTprintf("ef\r");
          if(string[0] == 'e' && string[1] == 'E')
          {
            level = (int) string[3];
          }
          if(level > currentFloor)
          {
            UARTprintf("es\r");
            isStopped = false;
            isAscending = true;
            isGoingDown = false;
          }
          if(level < currentFloor)
          {
            UARTprintf("ed\r");
            isStopped = false;
            isAscending = false;
            isGoingDown = true;
          }
        }
        
        if(isStopped == false)
        {
          if(isAscending == true && isGoingDown == false)
          {
            if((int) string[1] == level)
            {
              UARTprintf("ep\r");
              currentFloor = (int) string[1];
              UARTprintf("ed\r");
              //TODO - forma de deixar elevador proximo da altura ideal
			  osDelay(50);
              UARTprintf("ep\r");
			  // END TODO
              isStopped = true;
            }
          }
          if(isAscending == false && isGoingDown == true)
          {
            if((int) string[1] == level && isAscending == true)
            {
              UARTprintf("ep\r");
              isStopped = true;
			  //TODO - elevador descer para andar pelo botao externo.
            }
          }
        }
      }
      LeftElevator_readUART = false;
    }
    osThreadYield();
    //tick = osKernelGetTickCount();
    //osDelayUntil(tick+10);
  } // while
} // LeftElevator

//TODO - list
// implementar descida do elevador, abertura e fechamento das portas.

void CentralElevator(void *arg){
  message msg;
  uint32_t count = 0;
  bool doorStatus = false;
  bool isStopped = true;
  uint8_t level;
  char string[30];
  uint8_t state;
  while(1){
    if(CentralElevator_readUART == true)
    {
      state = osMessageQueueGet(CentralElevatorMessageQueue_id, &msg, NULL, osWaitForever);
      if(state == osOK){
        strcpy(string,"");
        strcpy(string, msg.string);
        if(isStopped == true){
          level = (int) string[3];
          UARTprintf("cf\r");
          UARTprintf("cs\r");
          isStopped = false;
        }
       if(isStopped == false){
         if((int) string[1] == level)
          {
              UARTprintf("cp\r");
              isStopped = true;
              //printf("stop\n");
          }
       }
        
      }
      CentralElevator_readUART = false;
    }
    osThreadYield();
  } // while
} // CentralElevator

void RightElevator(void *arg){
   message msg;
  uint32_t count = 0;
  bool doorStatus = false;
  bool isStopped = true;
  uint8_t level;
  char string[30];
  uint8_t state;
  while(1){
    if(RightElevator_readUART == true)
    {
      state = osMessageQueueGet(RightElevatorMessageQueue_id, &msg, NULL, osWaitForever);
      if(state == osOK){
        strcpy(string, "");
        strcpy(string, msg.string);
        if(isStopped == true){
          level = (int) msg.string[3];
          UARTprintf("df\r");
          UARTprintf("ds\r");
          isStopped = false;
        }
        if(isStopped == false){
          if((int) string[1] == level)
          {
              UARTprintf("dp\r");
              isStopped = true;
              //printf("stop\n");
          }
        }
        
      }
      RightElevator_readUART = false;
    }
    osThreadYield();
  } // while
} // RightElevator

void MainTask(void *arg){
  uint32_t count = 0;
  uint32_t tick;
  message msg;
  uint8_t state;
  while(1){
    //osMutexAcquire(uart_id, osWaitForever);
    strcpy(buffer, "");
    int size = UARTgets(buffer, 13);
    //osMutexRelease(uart_id);
    UARTFlushRx();
    //printf("%s\n",buffer);
    //UARTFlushTx(true);
    printf("%s\n",buffer);
    if(size != 0){
      if(buffer[0] == 'e'){
        LeftElevator_readUART = true;
        //osThreadFlagsSet(LeftElevator_id, 0x0001);
        strcpy(msg.string, buffer);
        state = osMessageQueuePut(LeftElevatorMessageQueue_id, &msg, NULL, osWaitForever);
      }
      else if(buffer[0] == 'c'){
        CentralElevator_readUART = true;
        //osThreadFlagsSet(CentralElevator_id, 0x0002);
        strcpy(msg.string, buffer);
        state = osMessageQueuePut(CentralElevatorMessageQueue_id, &msg, NULL, osWaitForever);
      }
      else if(buffer[0] == 'd'){
        RightElevator_readUART = true;
        //osThreadFlagsSet(RightElevator_id, 0x0003);
        strcpy(msg.string, buffer);
        state = osMessageQueuePut(RightElevatorMessageQueue_id, &msg, NULL, osWaitForever);
      }
    }
    //tick = osKernelGetTickCount();
    //osDelayUntil(tick + 20);
    //tick = osKernelGetSysTimerCount();
    //osDelayUntil(tick + 10);
  } // while
} //MainTask_id

void MainTask1(void *arg){
  uint32_t count = 0;
  uint32_t tick;
  message msg;
  uint8_t state;
  while(1){
    UARTprintf("er\r");
    UARTprintf("cr\r");
    UARTprintf("dr\r");
    strcpy(buffer, "");
    int size = UARTgets(buffer, 13);
    //printf("%s\n", buffer);
    
    if(buffer[0] == 'e'){
      printf("1\n");
      if(buffer[1] = 'E'){
        printf("4\n");
      }
      else{
        printf("5\n");
      }
    }
    else if(buffer[0] == 'c'){
      printf("2\n");
    }
    else if(buffer[0] == 'd'){
      printf("3\n");
    }
    /*
    UARTprintf("er\r");
    tick = osKernelGetTickCount();
    osDelayUntil(tick + 3000);
    UARTprintf("es\r");
    tick = osKernelGetTickCount();
    osDelayUntil(tick + 3000);
    UARTprintf("ef\r");
    tick = osKernelGetTickCount();
    osDelayUntil(tick + 3000);
    
    UARTprintf("er\r");
    tick = osKernelGetTickCount();
    osDelayUntil(tick + 3000);
    UARTprintf("ef\r");
    tick = osKernelGetTickCount();
    osDelayUntil(tick + 3000);
    UARTwrite("es\r", 5);
    tick = osKernelGetTickCount();
    osDelayUntil(tick + 3000);
    int level = (int) buffer[3];
    
    while(1){
      UARTgets(buffer, 13);
      //printf("%s\n", buffer);
      if(buffer[0] == 'e' && buffer[1] == '3'){
        UARTwrite("ep\r", 5);
      }
    }
*/
  } // while
} //MainTask_id

void main(void){
  UARTInit();
  if(osKernelGetState() == osKernelInactive)
     osKernelInitialize();
  LeftElevator_id = osThreadNew(LeftElevator, NULL, NULL);
  CentralElevator_id = osThreadNew(CentralElevator, NULL, NULL);
  RightElevator_id = osThreadNew(RightElevator, NULL, NULL);
  MainTask_id = osThreadNew(MainTask, NULL, NULL);
  //MainTask1_id = osThreadNew(MainTask1, NULL, NULL);
  LeftElevatorMessageQueue_id = osMessageQueueNew(1, sizeof(message), NULL);
  CentralElevatorMessageQueue_id = osMessageQueueNew(1, sizeof(message), NULL);
  RightElevatorMessageQueue_id = osMessageQueueNew(1, sizeof(message), NULL);
  uart_id = osMutexNew(NULL);
  if(osKernelGetState() == osKernelReady)
    osKernelStart();
  UARTprintf("er\r");
  UARTprintf("cr\r");
  UARTprintf("dr\r");
  UARTprintf("ef\r");
  UARTprintf("cf\r");
  UARTprintf("df\r");
  while(1);
} // main
