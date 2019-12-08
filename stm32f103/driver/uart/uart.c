#include <string.h>

#include "common.h"
#include "watchdog.h"
#include "uart.h"
#include "shell.h"
#include "log.h"

int uart_work_mode = TSHELL_MODE;

uint32_t uart_recv_buf_index = 0;
char uart_recv_buf[UART_IO_SIZE] = {0};

 /**
  * @brief  ����Ƕ�������жϿ�����NVIC
  * @param  ��
  * @retval ��
  */
static void NVIC_Configuration(void)
{
  NVIC_InitTypeDef NVIC_InitStructure;
  
  /* Ƕ�������жϿ�������ѡ�� */
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
  
  /* ����USARTΪ�ж�Դ */
  NVIC_InitStructure.NVIC_IRQChannel = DEBUG_USART_IRQ;
  /* �������ȼ�*/
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
  /* �����ȼ� */
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
  /* ʹ���ж� */
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  /* ��ʼ������NVIC */
  NVIC_Init(&NVIC_InitStructure);
}

 /**
  * @brief  USART GPIO ����,������������
  * @param  ��
  * @retval ��
  */
void USART_Config(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;

	// �򿪴���GPIO��ʱ��
	DEBUG_USART_GPIO_APBxClkCmd(DEBUG_USART_GPIO_CLK, ENABLE);
	
	// �򿪴��������ʱ��
	DEBUG_USART_APBxClkCmd(DEBUG_USART_CLK, ENABLE);

	// ��USART Tx��GPIO����Ϊ���츴��ģʽ
	GPIO_InitStructure.GPIO_Pin = DEBUG_USART_TX_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(DEBUG_USART_TX_GPIO_PORT, &GPIO_InitStructure);

  // ��USART Rx��GPIO����Ϊ��������ģʽ
	GPIO_InitStructure.GPIO_Pin = DEBUG_USART_RX_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(DEBUG_USART_RX_GPIO_PORT, &GPIO_InitStructure);
	
	// ���ô��ڵĹ�������
	// ���ò�����
	USART_InitStructure.USART_BaudRate = DEBUG_USART_BAUDRATE;
	// ���� �������ֳ�
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	// ����ֹͣλ
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	// ����У��λ
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	// ����Ӳ��������
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	// ���ù���ģʽ���շ�һ��
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	// ��ɴ��ڵĳ�ʼ������
	USART_Init(DEBUG_USARTx, &USART_InitStructure);
	
	// �����ж����ȼ�����
	NVIC_Configuration();
	
	// ʹ�ܴ��ڽ����ж�
	USART_ITConfig(DEBUG_USARTx, USART_IT_RXNE, ENABLE);	
	
	// ʹ�ܴ���
	USART_Cmd(DEBUG_USARTx, ENABLE);		

  // ���������ɱ�־
	//USART_ClearFlag(USART1, USART_FLAG_TC);     
}

/*****************  ����һ���ַ� **********************/
void Usart_SendByte( USART_TypeDef * pUSARTx, uint8_t ch)
{
	if (ch == '\n') {
		USART_SendData(pUSARTx, '\r');			
		while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET);
	}
	
	USART_SendData(pUSARTx,ch);
		
	/* �ȴ��������ݼĴ���Ϊ�� */
	while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET);	
}

/*****************  �����ַ��� **********************/
void Usart_SendString( USART_TypeDef * pUSARTx, char *str)
{
	unsigned int k=0;
  do 
  {
      Usart_SendByte( pUSARTx, *(str + k) );
      k++;
  } while(*(str + k)!='\0');
  
  /* �ȴ�������� */
  while(USART_GetFlagStatus(pUSARTx,USART_FLAG_TC)==RESET)
  {}
}

/*****************  ����һ��16λ�� **********************/
void Usart_SendHalfWord( USART_TypeDef * pUSARTx, uint16_t ch)
{
	uint8_t temp_h, temp_l;
	
	/* ȡ���߰�λ */
	temp_h = (ch&0XFF00)>>8;
	/* ȡ���Ͱ�λ */
	temp_l = ch&0XFF;
	
	/* ���͸߰�λ */
	USART_SendData(pUSARTx,temp_h);	
	while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET);
	
	/* ���͵Ͱ�λ */
	USART_SendData(pUSARTx,temp_l);	
	while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET);	
}

void DEBUG_USART_IRQHandler(void)
{
	static uint8_t i;
	static char magic_cmd[7] = {0};
	uint16_t ch;
	ch = (uint8_t)USART_ReceiveData(DEBUG_USARTx);

    for(i = 0; i < 5; i++) {
        magic_cmd[i] = magic_cmd[i + 1];
    }
    magic_cmd[5] = ch;

		if (strcmp(magic_cmd, "sreset") == 0) {
				uart_puts("magic_cmd: system reset!\n");
        watchdog_reset();
		}
		if (strcmp(magic_cmd, "tshell") == 0) {
			  uart_puts("magic_cmd: switch to thread shell!\n");        
				uart_work_mode = TSHELL_MODE;
		}
		if (strcmp(magic_cmd, "ishell") == 0) {
				uart_puts("magic_cmd: switch to int shell!\n");
				uart_work_mode = ISHELL_MODE;
		}

		if (ch == '\n') {   /* sscom will send '\r\n' we ignore the '\n' */
				return;
		}
		if (uart_recv_buf_index == (UART_IO_SIZE - 1) && ch != '\r') {
				uart_puts("cmd too long!\n");
				uart_recv_buf_index = 0;
				return;
		}

		if (ch == '\r') {
				uart_recv_buf[uart_recv_buf_index] = '\0';  /* terminate the string. */
				switch (uart_work_mode) {
					case (ISHELL_MODE):
						shell(uart_recv_buf);
						break;
					case (TSHELL_MODE):
						if (shell_cmd == NULL) {
							shell_cmd = uart_recv_buf;
						}
						break;
					default:
						break;
				}
				uart_recv_buf_index = 0;
				return;
		} else {
				uart_recv_buf[uart_recv_buf_index] = ch;
				uart_recv_buf_index++;
		}
		
		/* echo */
		printf("%c", ch);

}

void uart_putc(uint8_t byte)
{
	Usart_SendByte(DEBUG_USARTx, byte);
}

void uart_puts(char *s)
{
	Usart_SendString(DEBUG_USARTx, s);
}

void uart_init()
{
	USART_Config();
}