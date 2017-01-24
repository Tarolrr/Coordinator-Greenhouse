/*
 * NeowayM590_Driver.c
 *
 *  Created on: Dec 2, 2016
 *      Author: Tarolrr
 */

#include "NeowayM590_Driver.h"

char buf[256];
volatile bool connected;

char GSMRxStr[256];
bool GSMStrReceived = false;
bool GSMReceivingSMS = false;
volatile bool GSMSendingSMS = false;
NeowayM590_Message lastMsg;
bool retryMsg;
uint8_t retryCount;
NeowayM590_SMS GSMRecevedSMS;

static NeowayM590_ParseResult parseString(char *string);
static void GSMServiceTaskFxn(void const * argument);
static void GSMCheckTimerCallback(void const * argument);
static void GSMTimeoutTimerCallback(void const * argument);

void NeowayM590_Init(UART_HandleTypeDef *huart){

    SET_BIT(huart->Instance->CR1, USART_CR1_RXNEIE);

	osMutexDef(NeowayM590Mutex);
	hNeowayM590Mutex = osMutexCreate(osMutex(NeowayM590Mutex));

	osThreadDef(GSMServiceTask, GSMServiceTaskFxn, osPriorityNormal, 0, 512);
	hGSMServiceTask = osThreadCreate(osThread(GSMServiceTask), NULL);

	osTimerDef(GSMCheckTimer, GSMCheckTimerCallback);
	hGSMCheckTimer = osTimerCreate(osTimer(GSMCheckTimer), osTimerPeriodic, NULL);

	osTimerDef(GSMTimeoutTimer, GSMTimeoutTimerCallback);
	hGSMTimeoutTimer = osTimerCreate(osTimer(GSMTimeoutTimer), osTimerOnce, NULL);

	osMessageQDef(GSMSendQueue, 16, void *);
	hGSMSendQueue = osMessageCreate(osMessageQ(GSMSendQueue), NULL);

	hGSMuart = huart;
	pendingReset = true;

}

void NeowayM590_Reset(){
	__HAL_UART_CLEAR_FLAG(hGSMuart,UART_FLAG_RXNE);
	__HAL_UART_CLEAR_PEFLAG(hGSMuart);

	osTimerStop(hGSMTimeoutTimer);
	osTimerStop(hGSMCheckTimer);

	HAL_GPIO_WritePin(NeowayM590_ResetPin, GPIO_PIN_SET);
	HAL_Delay(1000);
	HAL_GPIO_WritePin(NeowayM590_ResetPin,GPIO_PIN_RESET);

	//HAL_GPIO_WritePin(NeowayM590_StatusLedPin, GPIO_PIN_RESET);
	retryMsg = false;
	retryCount = 0;
	pendingReset = false;
	connected = false;
	GSMSendingSMS = false;
	osMutexWait(hNeowayM590Mutex,0);

	osTimerStart(hGSMTimeoutTimer,NeowayM590_StartupTimeout);


	osEvent e = osMessageGet(hGSMSendQueue,0);
	while(e.status == osEventMessage){
		NeowayM590_Message *msg = (NeowayM590_Message *)e.value.p;
		vPortFree(msg);
		e = osMessageGet(hGSMSendQueue,0);
	}

	NeowayM590_SendCommand("ATE0");
	NeowayM590_SendCommand("AT+CMGF=1");
	NeowayM590_SendCommand("AT+CNMI=2,2,0,0,0");
	NeowayM590_SendCommand("AT+CSCS=\"GSM\"");
}

//should be null-terminated string
void NeowayM590_SendCommand(char *com){
	uint8_t len = strlen(com);
	NeowayM590_Message *msg;
	msg = pvPortMalloc(sizeof(NeowayM590_Message));
	memcpy(msg->content,com,len);
	msg->content[len] = '\r';
	msg->size = len+1;
	msg->isSMS = false;
	osMessagePut(hGSMSendQueue,(uint32_t)msg,0);
}

static NeowayM590_ParseResult parseString(char *string){
	if(GSMReceivingSMS)
		return NeowayM590_ParseResult_ReceivedSMSContent;

	if((string[0] == '>') && (strlen(string) == 1))
			return NeowayM590_ParseResult_SendingSMS;

	if(strstr((char *)string,"STARTUP"))
		return NeowayM590_ParseResult_Startup;
	if(strstr((char *)string,"+PBREADY"))
		return NeowayM590_ParseResult_Connected;
	if(strstr((char *)string,"RING"))
		return NeowayM590_ParseResult_Ring;
	else if(strstr((char *)string,"+CMT:")){
		return NeowayM590_ParseResult_ReceivedSMSHeader;
	}
	else if(strstr((char *)string,"+CREG:"))
		return NeowayM590_ParseResult_Check;
	else if(strstr((char *)string,"OK"))
		return NeowayM590_ParseResult_Ok;
	else if(strstr((char *)string,"ERROR"))
		return NeowayM590_ParseResult_Error;
	else
		return NeowayM590_ParseResult_Unknown;
}


static void GSMServiceTaskFxn(void const * argument){
	while(1){

		if(pendingReset)
			NeowayM590_Reset();

		if(retryMsg)
			osMutexRelease(hNeowayM590Mutex);

		osStatus s = osMutexWait(hNeowayM590Mutex,0);
		if( s == osOK){

			if(GSMSendingSMS){
				HAL_Delay(500);
				HAL_UART_Transmit(hGSMuart,(uint8_t *)lastMsg.content,lastMsg.size,-1);
				osTimerStart(hGSMTimeoutTimer,10000);
			}
			else{
				NeowayM590_Message *msg;
				osEvent e;
				e.status = osEventMessage;
				if(retryMsg){
					HAL_Delay(500);
					msg = &lastMsg;
					if(msg->isSMS){
						char str[32];
						uint8_t strSize = sprintf(str,"AT+CMGS=\"%s\"\r",(msg->number));
						HAL_UART_Transmit(hGSMuart,(uint8_t *)str,strSize,-1);
					}
					else
						HAL_UART_Transmit(hGSMuart,(uint8_t *)msg->content,msg->size,-1);
				}
				else{
					 e = osMessageGet(hGSMSendQueue,0);
					if(e.status == osEventMessage){
						HAL_Delay(500);
						msg = (NeowayM590_Message *)e.value.p;
						memcpy(&lastMsg,msg,sizeof(NeowayM590_Message));
						HAL_UART_Transmit(hGSMuart,(uint8_t *)msg->content,msg->size,-1);
						vPortFree(msg);
					}
					else
						osMutexRelease(hNeowayM590Mutex);
				}

				if(e.status == osEventMessage){



					osTimerStart(hGSMTimeoutTimer,NeowayM590_RxTimeout);
				}
			}

			retryMsg = false;

		}

		char *tok;

		if(GSMStrReceived){
			GSMStrReceived = false;
			NeowayM590_ParseResult parseResult = parseString(GSMRxStr);
			switch(parseResult){
				case NeowayM590_ParseResult_Startup:
					break;
				case NeowayM590_ParseResult_Connected:
					connected = true;
					//HAL_GPIO_WritePin(NeowayM590_StatusLedPin, GPIO_PIN_SET);
					osTimerStop(hGSMTimeoutTimer);
					osTimerStart(hGSMCheckTimer,NeowayM590_CheckPeriod);
					osMutexRelease(hNeowayM590Mutex);

					//HAL_Delay(2000);

					/*NeowayM590_SMS sms2;
					sprintf(sms2.content,"ready");
					sprintf(sms2.number,"+79600365483");
					NeowayM590_SendSMS(&sms2);*/
					break;
				case NeowayM590_ParseResult_Ring:
					break;
				case NeowayM590_ParseResult_RingEnded:
					break;
				case NeowayM590_ParseResult_ReceivedSMSHeader:

					//skipping the part "+CMT: ", changing opening \" to \0
					if(strtok(GSMRxStr,"\"") == NULL)
						break;

					//saving pointer to the beginning of sender
					if((tok = strtok(NULL,"\"")) == NULL)
						return;

					// if sender substring is longer than 12 symbols then smtng is wrong
					if(strlen(tok) > 12)
						break;

					// saving number
					memcpy(GSMRecevedSMS.number,tok,strlen(tok)+1);

					//skipping the part ",,"
					if(strtok(NULL,"\"") == NULL)
						break;

					//saving pointer to the beginning of timestamp
					if((tok = strtok(NULL,"\"")) == NULL)
						return;

					// if timestamp substring is longer than 30 symbols then smtng is wrong
					if(strlen(tok) > 30)
						break;

					// saving timestamp
					memcpy(GSMRecevedSMS.timeStr,tok,strlen(tok)+1);

					GSMReceivingSMS = true;

					break;
				case NeowayM590_ParseResult_ReceivedSMSContent:
					GSMReceivingSMS = false;
					memcpy(GSMRecevedSMS.content,GSMRxStr,strlen(GSMRxStr)+1);
					NeowayM590_SMSReceivedCallback(&GSMRecevedSMS);
					break;
				case NeowayM590_ParseResult_SendingSMS:
					GSMSendingSMS = true;
					osTimerStop(hGSMTimeoutTimer);
					osMutexRelease(hNeowayM590Mutex);
					break;
				case NeowayM590_ParseResult_Check:
					if(strstr((char *)buf,"+CREG: 0,1"))
						pendingReset = true;
					break;
				case NeowayM590_ParseResult_Ok:
					if(connected){
						if(GSMSendingSMS)
							GSMSendingSMS = false;

						osTimerStop(hGSMTimeoutTimer);
						osMutexRelease(hNeowayM590Mutex);
					}
					break;
				case NeowayM590_ParseResult_Error:
					GSMTimeoutTimerCallback(NULL);
					break;
				case NeowayM590_ParseResult_Unknown:
					break;
			}
		}
	}
}


static void GSMCheckTimerCallback(void const * argument){
	NeowayM590_SendCommand("AT+CREG?");
}

static void GSMTimeoutTimerCallback(void const * argument){
	if(!connected){
		pendingReset = true;
		return;
	}
	if(retryCount == NeowayM590_MaxRetryCount)
		pendingReset = true;
	else{
		retryCount++;
		retryMsg = true;
		GSMSendingSMS = false;
	}
}

char tmpStr[256];
uint8_t tmpStrSize = 0;

void GSM_UART_IRQHandler(){

	uint32_t isrflags   = READ_REG(hGSMuart->Instance->SR);
	uint32_t errorflags = 0x00U;

	__HAL_UART_CLEAR_FLAG(hGSMuart,UART_FLAG_RXNE);
	__HAL_UART_CLEAR_PEFLAG(hGSMuart);

	/* If no error occurs */
	errorflags = (isrflags & (uint32_t)(USART_SR_PE | USART_SR_FE | USART_SR_ORE | USART_SR_NE));
	if(errorflags != RESET)
	  return;

	tmpStr[tmpStrSize] = (uint8_t)(hGSMuart->Instance->DR & (uint8_t)0x00FFU);

	if(lastMsg.isSMS)
		if(tmpStr[0] == '>'){
			GSMStrReceived = true;
			GSMRxStr[0] = '>';
			GSMRxStr[1] = 0;
			return;
		}
	if(tmpStr[tmpStrSize] == '\n'){
		if(tmpStrSize == 0)
			return;

		tmpStr[tmpStrSize++] = 0;
		memcpy(GSMRxStr,tmpStr,tmpStrSize);
		tmpStrSize = 0;
		GSMStrReceived = true;
	}
	else if(tmpStr[tmpStrSize] != '\r')
		tmpStrSize++;
}

void NeowayM590_SendSMS(NeowayM590_SMS *sms){
	uint8_t len = strlen(sms->content);
	NeowayM590_Message *msg;
	msg = pvPortMalloc(sizeof(NeowayM590_Message));
	memcpy(msg->content,sms->content,len > 62 ? 62 : len);
	msg->content[len] = 0x1a;
	msg->size = len+1;
	msg->isSMS = true;
	sprintf(msg->number,"%s",sms->number);
	osMessagePut(hGSMSendQueue,(uint32_t)msg,0);
}

__weak void NeowayM590_SMSReceivedCallback(NeowayM590_SMS *sms){
  UNUSED(sms);
}
