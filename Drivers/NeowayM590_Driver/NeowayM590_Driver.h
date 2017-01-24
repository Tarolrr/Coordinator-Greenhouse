/*
 * NeowayM590_Driver.h
 *
 *  Created on: Dec 2, 2016
 *      Author: Tarolrr
 */

#ifndef NEOWAYM590_DRIVER_H_
#define NEOWAYM590_DRIVER_H_

#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <string.h>
#include "cmsis_os.h"


enum{
	NeowayM590_ParseResult_Startup,
	NeowayM590_ParseResult_Connected,
	NeowayM590_ParseResult_Ring,
	NeowayM590_ParseResult_RingEnded,
	NeowayM590_ParseResult_ReceivedSMSHeader,
	NeowayM590_ParseResult_SendingSMS,
	NeowayM590_ParseResult_ReceivedSMSContent,
	NeowayM590_ParseResult_Check,
	NeowayM590_ParseResult_Ok,
	NeowayM590_ParseResult_Error,
	NeowayM590_ParseResult_Unknown,
} typedef NeowayM590_ParseResult;

#define NeowayM590_ResetPin GPIOA, GPIO_PIN_1
//#define NeowayM590_StatusLedPin GPIOC, GPIO_PIN_13
#define NeowayM590_CheckPeriod 60000
#define NeowayM590_StartupTimeout 30000
#define NeowayM590_RxTimeout 500
#define NeowayM590_MaxRetryCount 10
UART_HandleTypeDef *hGSMuart;
uint32_t checkTimer;
volatile bool checkConfirmed;
volatile bool pendingReset;


osMutexId hNeowayM590Mutex; //TODO maybe change name
osThreadId hGSMServiceTask;
osTimerId hGSMCheckTimer;
osTimerId hGSMTimeoutTimer;
osMessageQId hGSMSendQueue;

struct{
	bool isSMS;	//if not then command
	char number[13];
	char content[64];
	uint8_t size;
} typedef NeowayM590_Message;


struct{
	char number[13];
	char content[160];
	char timeStr[31];
	uint32_t time;
} typedef NeowayM590_SMS;

void NeowayM590_Init(UART_HandleTypeDef *huart);
void NeowayM590_Reset();
void NeowayM590_SendCommand(char *com);
void NeowayM590_SendSMS(NeowayM590_SMS *sms);
void NeowayM590_SMSReceivedCallback(NeowayM590_SMS *sms);
#endif /* NEOWAYM590_DRIVER_H_ */
