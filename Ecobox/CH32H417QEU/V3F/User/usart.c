#include "usart.h"

/*
 * Screen communication is handled by status_uart.c on USART3:
 * PB10 = TX, PB11 = RX.
 *
 * USART2 on PD5/PD6 is handled by CH584_Comm.c for the CH584M link.
 */
