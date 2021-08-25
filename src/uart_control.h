#ifndef UART_CONTROL_H_
#define UART_CONTROL_H_

void UARTInit(void);
void UARTRx_Handler(void);
void UARTTx_Handler(void);
void Thread_Uart_TX(void *arg);
void Thread_Uart_RX(void *arg);
int Identifica_numero(char *aux);


#define UART0_FR_R              (*((volatile uint32_t *)0x4000C018))
#define UART_FR_RXFE            0x00000010  // UART Receive FIFO Empty


#endif //UART_CONTROL_H_