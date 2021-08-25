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

enum ELEVATOR_STATES{
	INITIAL = 0,
	STOPPED,
	GOING_UP,
	GOING_DOWN,
        WAITING
};

typedef struct // STRUCT
{
    char string[30];
} message; //STRUCT

uint8_t floorsInternal_leftElevator[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t floorsInternal_centralElevator[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t floorsInternal_rightElevator[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

uint8_t floorsExternal_leftElevator[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t floorsExternal_centralElevator[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t floorsExternal_rightElevator[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

osThreadId_t LeftElevator_id, CentralElevator_id, RightElevator_id, MainTask_id, MainTask1_id;
osMessageQueueId_t LeftElevatorMessageQueue_id, CentralElevatorMessageQueue_id, RightElevatorMessageQueue_id;
osMutexId_t uart_id;
message LeftElevator_msg, CentralElevator_msg, RightElevator_msg;
bool LeftElevator_readUART = false;
bool CentralElevator_readUART = false;
bool RightElevator_readUART = false;
char buffer[30];
int currentFloor_leftElevator = 0;
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
  uint8_t stateLeftElevator = STOPPED;
  uint8_t level;
  uint8_t level_internal;
  char string[30];
  char read_buffer[30];
  uint8_t state;
  int currentFloor = 0;
  while(1){
    if(LeftElevator_readUART == true)
    {
      state = osMessageQueueGet(LeftElevatorMessageQueue_id, &msg, NULL, osWaitForever);
      //
      if(state == osOK)
      {
        UARTprintf("ef\r");
        strcpy(read_buffer,"");
        strcpy(string,"");
        strcpy(string, msg.string);
        switch(stateLeftElevator)
        {
          case STOPPED:
            stateLeftElevator = STOPPED;
            if(string[0] == 'e' && string[1] == 'E')
            {
              level = (int) string[3] - 0x30;
            }
            if(string[0] == 'e' && string[1] == 'I')
            {
              level_internal = (int) string[3];
            }
            if(level > currentFloor)
            {
              UARTprintf("es\r");
              stateLeftElevator = GOING_UP;
            }
            if(level < currentFloor)
            {
              UARTprintf("ed\r");
              stateLeftElevator = GOING_DOWN;
            }
            break;
            
          case GOING_UP:
            stateLeftElevator = GOING_UP;
            if(string[0] == 'e')
            {
              currentFloor = (int) string[1] - 0x30; //Atualiza o andar do elevador
            }
            //if((int) string[1] == level)
            if(currentFloor == level)
            {
              UARTprintf("ep\r");
              UARTprintf("ed\r");
              osDelay(800);
              UARTprintf("ep\r");
              //osDelay(1000);
              //UARTprintf("ef\r");
              char str[12];
              strcpy(str, "");
              str[0] = 'e';
              str[1] = 'D';
              str[2] = (char) (97 + level);
              str[3] = '\r';
              UARTprintf("%s", str);
              stateLeftElevator = WAITING;
            }
            break;
            
          case GOING_DOWN:
            stateLeftElevator = GOING_DOWN;
            if(string[0] == 'e')
            {
              currentFloor = (int) string[1] - 0x30; //Atualiza o andar do elevador
            }
            if(currentFloor == level)
            {
              UARTprintf("ep\r");
              if(level != 0)
              {
                UARTprintf("es\r");
                osDelay(400);
                UARTprintf("ep\r");
              }
              char str[12];
              strcpy(str, "");
              str[0] = 'e';
              str[1] = 'D';
              str[2] = (char) (97 + level);
              str[3] = '\r';
              UARTprintf("%s", str);
              stateLeftElevator = WAITING;
            }
            break;
          
          case WAITING:
            if(string[0] == 'e' && (string[1] != 'a' || string[1] == 'A' || string[1] != 'f' || string[1] == 'F'))
            {
              //UARTprintf("ea\r");
              //osDelay(2000);
            }
            //UARTprintf("ea\r");
            //osDelay(2000);
            //UARTprintf("ef\r");
            if(string[0] == 'e' && string[1] == 'I')
            {
              char button = (string[2]);
              char str[10];
              strcpy(str, "");
              str[0] = 'e';
              str[1] = 'L';
              str[2] = button;
              str[3] = '\r';
              floorsInternal_leftElevator[(int)button - 97] = 1;
              UARTprintf("%s", str);
              level = ((int) button) - 97;
              if(level > currentFloor)
              {
                UARTprintf("es\r");
                stateLeftElevator = GOING_UP;
              }
              if(level == currentFloor)
              {
                stateLeftElevator = WAITING;
              }
              if(level < currentFloor)
              {
                UARTprintf("ed\r");
                stateLeftElevator = GOING_DOWN;
              }
            }
            if(string[0] == 'e' && string[1] == 'E')
            {
              level = (int) string[3] - 0x30;
              if(level > currentFloor)
              {
                UARTprintf("es\r");
                stateLeftElevator = GOING_UP;
              }
              if(level < currentFloor)
              {
                UARTprintf("ed\r");
                stateLeftElevator = GOING_DOWN;
              }
            }
            break;
        }
      }
      LeftElevator_readUART = false;
    }
    osThreadYield();
  } // while
} // LeftElevator

//TODO - list
// implementar descida do elevador, abertura e fechamento das portas.

void CentralElevator(void *arg){
  message msg;
  uint8_t stateCentralElevator = STOPPED;
  uint8_t level;
  uint8_t level_internal;
  char string[30];
  char read_buffer[30];
  uint8_t state;
  int currentFloor = 0;
  while(1){
    if(CentralElevator_readUART == true)
    {
      state = osMessageQueueGet(RightElevatorMessageQueue_id, &msg, NULL, osWaitForever);
      //
      if(state == osOK)
      {
        UARTprintf("cf\r");
        strcpy(read_buffer,"");
        strcpy(string,"");
        strcpy(string, msg.string);
        switch(stateCentralElevator)
        {
          case STOPPED:
            stateCentralElevator = STOPPED;
            if(string[0] == 'c' && string[1] == 'E')
            {
              level = (int) string[3] - 0x30;
            }
            if(string[0] == 'c' && string[1] == 'I')
            {
              level_internal = (int) string[3];
            }
            if(level > currentFloor)
            {
              UARTprintf("cs\r");
              stateCentralElevator = GOING_UP;
            }
            if(level < currentFloor)
            {
              UARTprintf("cd\r");
              stateCentralElevator = GOING_DOWN;
            }
            break;
            
          case GOING_UP:
            stateCentralElevator = GOING_UP;
            if(string[0] == 'c')
            {
              currentFloor = (int) string[1] - 0x30; //Atualiza o andar do elevador
            }
            //if((int) string[1] == level)
            if(currentFloor == level)
            {
              UARTprintf("cp\r");
              UARTprintf("cd\r");
              osDelay(800);
              UARTprintf("cp\r");
              //osDelay(1000);
              //UARTprintf("ef\r");
              char str[12];
              strcpy(str, "");
              str[0] = 'c';
              str[1] = 'D';
              str[2] = (char) (97 + level);
              str[3] = '\r';
              UARTprintf("%s", str);
              stateCentralElevator = WAITING;
            }
            break;
            
          case GOING_DOWN:
            stateCentralElevator = GOING_DOWN;
            if(string[0] == 'c')
            {
              currentFloor = (int) string[1] - 0x30; //Atualiza o andar do elevador
            }
            if(currentFloor == level)
            {
              UARTprintf("cp\r");
              if(level != 0)
              {
                UARTprintf("cs\r");
                osDelay(400);
                UARTprintf("cp\r");
              }
              char str[12];
              strcpy(str, "");
              str[0] = 'c';
              str[1] = 'D';
              str[2] = (char) (97 + level);
              str[3] = '\r';
              UARTprintf("%s", str);
              stateCentralElevator = WAITING;
            }
            break;
          
          case WAITING:
            if(string[0] == 'c' && (string[1] != 'a' || string[1] == 'A' || string[1] != 'f' || string[1] == 'F'))
            {
              //UARTprintf("ea\r");
              //osDelay(2000);
            }
            //UARTprintf("ea\r");
            //osDelay(2000);
            //UARTprintf("ef\r");
            if(string[0] == 'c' && string[1] == 'I')
            {
              char button = (string[2]);
              char str[10];
              strcpy(str, "");
              str[0] = 'c';
              str[1] = 'L';
              str[2] = button;
              str[3] = '\r';
              floorsInternal_leftElevator[(int)button - 97] = 1;
              UARTprintf("%s", str);
              level = ((int) button) - 97;
              if(level > currentFloor)
              {
                UARTprintf("cs\r");
                stateCentralElevator = GOING_UP;
              }
              if(level == currentFloor)
              {
                stateCentralElevator = WAITING;
              }
              if(level < currentFloor)
              {
                UARTprintf("cd\r");
                stateCentralElevator = GOING_DOWN;
              }
            }
            if(string[0] == 'c' && string[1] == 'E')
            {
              level = (int) string[3] - 0x30;
              if(level > currentFloor)
              {
                UARTprintf("cs\r");
                stateCentralElevator = GOING_UP;
              }
              if(level < currentFloor)
              {
                UARTprintf("cd\r");
                stateCentralElevator = GOING_DOWN;
              }
            }
            break;
        }
      }
      CentralElevator_readUART = false;
    }
    osThreadYield();
  } // while
} // CentralElevator

void RightElevator(void *arg){
  message msg;
  uint8_t stateRightElevator = STOPPED;
  uint8_t level;
  uint8_t level_internal;
  char string[30];
  char read_buffer[30];
  uint8_t state;
  int currentFloor = 0;
  while(1){
    if(RightElevator_readUART == true)
    {
      state = osMessageQueueGet(RightElevatorMessageQueue_id, &msg, NULL, osWaitForever);
      //
      if(state == osOK)
      {
        UARTprintf("df\r");
        strcpy(read_buffer,"");
        strcpy(string,"");
        strcpy(string, msg.string);
        switch(stateRightElevator)
        {
          case STOPPED:
            stateRightElevator = STOPPED;
            if(string[0] == 'd' && string[1] == 'E')
            {
              level = (int) string[3] - 0x30;
            }
            if(string[0] == 'd' && string[1] == 'I')
            {
              level_internal = (int) string[3];
            }
            if(level > currentFloor)
            {
              UARTprintf("ds\r");
              stateRightElevator = GOING_UP;
            }
            if(level < currentFloor)
            {
              UARTprintf("dd\r");
              stateRightElevator = GOING_DOWN;
            }
            break;
            
          case GOING_UP:
            stateRightElevator = GOING_UP;
            if(string[0] == 'd')
            {
              currentFloor = (int) string[1] - 0x30; //Atualiza o andar do elevador
            }
            //if((int) string[1] == level)
            if(currentFloor == level)
            {
              UARTprintf("dp\r");
              UARTprintf("dd\r");
              osDelay(800);
              UARTprintf("dp\r");
              //osDelay(1000);
              //UARTprintf("ef\r");
              char str[12];
              strcpy(str, "");
              str[0] = 'd';
              str[1] = 'D';
              str[2] = (char) (97 + level);
              str[3] = '\r';
              UARTprintf("%s", str);
              stateRightElevator = WAITING;
            }
            break;
            
          case GOING_DOWN:
            stateRightElevator = GOING_DOWN;
            if(string[0] == 'd')
            {
              currentFloor = (int) string[1] - 0x30; //Atualiza o andar do elevador
            }
            if(currentFloor == level)
            {
              UARTprintf("dp\r");
              if(level != 0)
              {
                UARTprintf("ds\r");
                osDelay(400);
                UARTprintf("dp\r");
              }
              char str[12];
              strcpy(str, "");
              str[0] = 'd';
              str[1] = 'D';
              str[2] = (char) (97 + level);
              str[3] = '\r';
              UARTprintf("%s", str);
              stateRightElevator = WAITING;
            }
            break;
          
          case WAITING:
            if(string[0] == 'd' && (string[1] != 'a' || string[1] == 'A' || string[1] != 'f' || string[1] == 'F'))
            {
              //UARTprintf("ea\r");
              //osDelay(2000);
            }
            //UARTprintf("ea\r");
            //osDelay(2000);
            //UARTprintf("ef\r");
            if(string[0] == 'd' && string[1] == 'I')
            {
              char button = (string[2]);
              char str[10];
              strcpy(str, "");
              str[0] = 'd';
              str[1] = 'L';
              str[2] = button;
              str[3] = '\r';
              floorsInternal_leftElevator[(int)button - 97] = 1;
              UARTprintf("%s", str);
              level = ((int) button) - 97;
              if(level > currentFloor)
              {
                UARTprintf("ds\r");
                stateRightElevator = GOING_UP;
              }
              if(level == currentFloor)
              {
                stateRightElevator = WAITING;
              }
              if(level < currentFloor)
              {
                UARTprintf("dd\r");
                stateRightElevator = GOING_DOWN;
              }
            }
            if(string[0] == 'd' && string[1] == 'E')
            {
              level = (int) string[3] - 0x30;
              if(level > currentFloor)
              {
                UARTprintf("ds\r");
                stateRightElevator = GOING_UP;
              }
              if(level < currentFloor)
              {
                UARTprintf("dd\r");
                stateRightElevator = GOING_DOWN;
              }
            }
            break;
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
      if(buffer[0] == 'c'){
        CentralElevator_readUART = true;
        //osThreadFlagsSet(CentralElevator_id, 0x0002);
        strcpy(msg.string, buffer);
        state = osMessageQueuePut(CentralElevatorMessageQueue_id, &msg, NULL, osWaitForever);
      }
      if(buffer[0] == 'd'){
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
