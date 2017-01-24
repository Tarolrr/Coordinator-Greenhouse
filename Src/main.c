//V1.0.4:
//	-defines for debugging and using various versions of hardware.
//	-added fno-strict-aliasing param
//	-handled unimportant warnings
//  -relays disabled for debugging purposes
//V1.0.3:
//	-Fixed bug in SMS (SMS with pass was incorrectly handled (no reply, no param saving))
//	-Fixed bug in SMS status (relays not differentiating, relays N/A immediately after sending message to relay)
//V1.0.2:
//	-Storing SMS owner number and temperature parameters in flash
//V1.0.1:
//	-Reply SMS only to owner, and only to correct command
//V1.0.0 - Original version

#include "stm32f1xx_hal.h"
#include "SX1278Drv.h"
#include <string.h>
#include "main.h"
#include "cmsis_os.h"
#include "tm_stm32_hd44780.h"

#define MySTM_
#define DebugWithoutRelay

SPI_HandleTypeDef hspi1;
TIM_HandleTypeDef htim1;
UART_HandleTypeDef huart2;
osTimerId hRxTimer;
char GSMOwner[16] = "";
uint8_t sensorState[SensorCount];
uint32_t alertTimer;
float sensorLastValue[SensorCount];
SX1278Drv_LoRaConfiguration cfg;
ChannelFlag chFlags;
uint8_t sendedMessageCount;
osThreadId hLCDTask;
uint32_t statusTimer = 0;


uint32_t sensorConnectTimer[SensorCount];

#ifndef DebugWithoutRelay
//uint32_t relayConnectTimer[RelayCount];
bool relayNotConnected[RelayCount];
uint8_t lastRelayIdx;
#endif


void SystemClock_Config(void);
void Error_Handler(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
static void LCDTaskFxn(void const * argument);

#ifndef DebugWithoutRelay
static void RxTimerCallback(void const * argument);
#endif

volatile uint8_t __attribute__((aligned(1024),section(".text"))) flashVar[1024] = {
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

int main(void){
	ThresholdTempLow = DefaultThresholdTempLow;
	ThresholdTempHigh = DefaultThresholdTempHigh;

	char *tmpGSMOwner = (char *)flashVar;
	float *tmpThrshldLow = (float *)(flashVar+16);
	float *tmpThrshldHigh = (float *)(flashVar+20);

	if(strlen(tmpGSMOwner) == 12)
		memcpy(GSMOwner,tmpGSMOwner,13);

	if(*tmpThrshldLow != 0)
		ThresholdTempLow = *tmpThrshldLow;
	if(*tmpThrshldHigh != 0)
		ThresholdTempHigh = *tmpThrshldHigh;

	HAL_Init();

	SystemClock_Config();
	MX_GPIO_Init();
	MX_SPI1_Init();
	MX_USART2_UART_Init();
	NeowayM590_Init(&huart2);
	TM_HD44780_Init(16,2);
#ifndef DebugWithoutRelay
	osTimerDef(RxTimer, RxTimerCallback);
	hRxTimer = osTimerCreate(osTimer(RxTimer), osTimerOnce, NULL);
#endif
	osThreadDef(LCDTask, LCDTaskFxn, osPriorityNormal, 0, 512);
	hLCDTask = osThreadCreate(osThread(LCDTask), NULL);

	cfg.bw = SX1278Drv_RegLoRaModemConfig1_BW_125;
	cfg.cr = SX1278Drv_RegLoRaModemConfig1_CR_4_8;
	cfg.crc = SX1278Drv_RegLoRaModemConfig2_PayloadCrc_ON;
#ifdef MySTM
	cfg.frequency = 434e6;
#else
	cfg.frequency = 868e6;
#endif
	cfg.hdrMode = SX1278Drv_RegLoRaModemConfig1_HdrMode_Explicit;
	cfg.power = 17;
	cfg.preambleLength = 20;
	cfg.sf = SX1278Drv_RegLoRaModemConfig2_SF_12;
	cfg.spi = &hspi1;
#ifdef MySTM
	cfg.spi_css_pin = &SPICSMyPin;
#else
	cfg.spi_css_pin = &SPICSPin;
	cfg.rx_en = &LoRaRxEnPin;
	cfg.tx_en = &LoRaTxEnPin;
#endif
	cfg.sleepInIdle = true;

	SX1278Drv_Init(&cfg);
	SX1278Drv_SetAdresses(0, (uint16_t *)AddrSensors, SensorCount);
	SX1278Drv_SetAdresses(SensorCount, (uint16_t *)AddrRelays, RelayCount);

	osKernelStart();
	return 0;
}

void SystemClock_Config(void){
	RCC_OscInitTypeDef RCC_OscInitStruct;
	RCC_ClkInitTypeDef RCC_ClkInitStruct;
	RCC_PeriphCLKInitTypeDef PeriphClkInit;

	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSI;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
	RCC_OscInitStruct.LSIState = RCC_LSI_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL3;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
		Error_Handler();

	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
							  |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
		Error_Handler();

	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
	PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
		Error_Handler();

	HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);
	HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
	HAL_NVIC_SetPriority(SysTick_IRQn, 15, 0);
}

/* SPI1 init function */
static void MX_SPI1_Init(void){
	hspi1.Instance = SPI1;
	hspi1.Init.Mode = SPI_MODE_MASTER;
	hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi1.Init.NSS = SPI_NSS_SOFT;
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
	hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi1.Init.CRCPolynomial = 10;
	if (HAL_SPI_Init(&hspi1) != HAL_OK)
		Error_Handler();
	GPIO_PIN_SET(&SPICSPin);
}

static void MX_GPIO_Init(void){
	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();

	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

	GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_11 | GPIO_PIN_1;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

static void MX_USART2_UART_Init(void){
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
    Error_Handler();
}

void Error_Handler(void){
  while(1);
}

char StatusSMSStr[128];
static void LCDTaskFxn(void const * argument){
	uint8_t currDisplayedSensor = 0;
	while(1){
		if(HAL_GetTick() - statusTimer > 60000*StatusSMSPeriodInMinutes){

			uint8_t size = 0;
			uint8_t idx;

			if(chFlags & ChannelFlag_TooLongError)
				size += sprintf(StatusSMSStr+size,"T too long out of bounds\n");
			if((chFlags & ChannelFlag_TooMuchError) && (chFlags & ChannelFlag_Low))
				size += sprintf(StatusSMSStr+size,"T too low\n");
			if((chFlags & ChannelFlag_TooMuchError) && (chFlags & ChannelFlag_High))
				size += sprintf(StatusSMSStr+size,"T too high\n");

			bool someSensorsNoConnect = false;

			size += sprintf(StatusSMSStr+size,"S ");

			for(idx = 0; idx < SensorCount; idx++)
				if(HAL_GetTick() - sensorConnectTimer[idx] > 60000*ConnectTimeoutInMinutes){
					size += sprintf(StatusSMSStr+size,"%d,",AddrSensors[idx]);
					someSensorsNoConnect = true;
				}

			if(!someSensorsNoConnect)
				size -= 2;
			else
				size += sprintf(StatusSMSStr+size-1," N/A\n") - 1;

#ifndef DebugWithoutRelay
			bool someRelaysNoConnect = false;

			size += sprintf(StatusSMSStr+size,"R ");

			for(idx = 0; idx < RelayCount; idx++)
				if(relayNotConnected[idx]){
					size += sprintf(StatusSMSStr+size,"%d",AddrRelays[idx]&0x3fff);
					if(AddrRelays[idx]&LoRaRelayType_Hot)
						size += sprintf(StatusSMSStr+size,"H,");
					else
						size += sprintf(StatusSMSStr+size,"C,");

					someRelaysNoConnect = true;
				}

			if(!someRelaysNoConnect)
				size -= 2;
			else
				size += sprintf(StatusSMSStr+size-1," N/A\n") - 1;
#endif


			if((size > 0) && (GSMOwner[0] == '+')){
				StatusSMSStr[size-1] = 0;
				statusTimer = HAL_GetTick();
				NeowayM590_SMS sms2;
				memcpy(sms2.content, StatusSMSStr,size+1);
				memcpy(sms2.number, GSMOwner,13);
				NeowayM590_SendSMS(&sms2);
			}
		}
		if(currDisplayedSensor == SensorCount)
			currDisplayedSensor = 0;
		if(sensorLastValue[currDisplayedSensor]){
			char str[17];
			uint8_t size = sprintf(str,"S %d T %+.1f",AddrSensors[currDisplayedSensor],sensorLastValue[currDisplayedSensor]);
			TM_HD44780_Clear();
			taskENTER_CRITICAL();
			TM_HD44780_Puts(0,0,str);
			taskEXIT_CRITICAL();
			osDelay(LCDUpdatePeriodMs);
		}
		currDisplayedSensor++;

	}
}

bool pendingReply = false;
bool pendingRequest = false;
void UpdateChannelState(){
	uint8_t idx;

	bool chLow = false;
	bool chHigh = false;

	chFlags &= (ChannelFlag_High | ChannelFlag_Low);
	for(idx = 0; idx < SensorCount; idx++){
		if(HAL_GetTick() - sensorConnectTimer[idx] > 60000*ConnectTimeoutInMinutes)
			continue;

		if(sensorState[idx] & SensorState_Low)
			chLow = true;
		else if(sensorState[idx] & SensorState_High)
			chHigh = true;

		if(sensorState[idx] & SensorState_TooMuch)
			chFlags |= ChannelFlag_TooMuchError;
	}

	if(chLow)
		chFlags |= ChannelFlag_HotOn;
	else if(chFlags & ChannelFlag_Low)
			chFlags |= ChannelFlag_HotOff;

	if(chHigh)
		chFlags |= ChannelFlag_ColdOn;
	else if(chFlags & ChannelFlag_High)
			chFlags |= ChannelFlag_ColdOff;

	if((!chLow) && (!chHigh))
		alertTimer = HAL_GetTick();

	if(HAL_GetTick() - alertTimer > MaximumWorkingTimeInMinutes*60000)
		chFlags |= ChannelFlag_TooLongError;
}

#ifndef DebugWithoutRelay
static void RxTimerCallback(void const * argument){
	uint8_t idx;
	LoRa_Message msg2;
	msg2.payloadLength = 1;

	if(SX1278Drv_IsBusy())
		return;

	if(sendedMessageCount == RetryCount){
		relayNotConnected[lastRelayIdx] = true;
		osTimerStop(hRxTimer);
		pendingRequest = false;
		return;
	}

	if(chFlags & ChannelFlag_HotOn)
		chFlags |= ChannelFlag_Low;

	if(chFlags & ChannelFlag_ColdOn)
		chFlags |= ChannelFlag_High;

	if((chFlags & ChannelFlag_LowHighError) == ChannelFlag_LowHighError)
		chFlags = ChannelFlag_LowHighError | ChannelFlag_HotOff | ChannelFlag_ColdOff;

	if(chFlags & ChannelFlag_HotOff)
		chFlags &= ~ChannelFlag_Low;

	if(chFlags & ChannelFlag_ColdOff)
		chFlags &= ~ChannelFlag_High;//TODO correct

	for(idx = 0; idx < RelayCount; idx++){
		msg2.address = AddrRelays[idx];

		if(AddrRelays[idx] & LoRaRelayType_Hot){
			if(chFlags & ChannelFlag_HotOn)
				msg2.payload[0] = 1;
			else if(chFlags & ChannelFlag_HotOff)
				msg2.payload[0] = 0;
			else
				continue;
		}
		else{
			if(chFlags & ChannelFlag_ColdOn)
				msg2.payload[0] = 1;
			else if(chFlags & ChannelFlag_ColdOff)
				msg2.payload[0] = 0;
			else
				continue;
		}
		sendedMessageCount++; //TODO correct logic in relay requests (all relays requested at the same time)
		SX1278Drv_SendMessage(&msg2);
		relayNotConnected[idx] = false;
		return;
	}

	osTimerStart(hRxTimer,SX1278Drv_GetRandomDelay(MinimumRxTimeout,1000));
	//TODO handle situation when all sensor returned to normal state
}
#endif

void SX1278Drv_LoRaRxCallback(LoRa_Message *msg){
	uint8_t setAlert = 0;
	uint8_t idx;

	if(!(msg->address & LoRaDeviceType_Relay)){
		if(msg->payloadLength != 4)
			return;


		for(idx = 0; idx < SensorCount; idx++)
			if(msg->address == AddrSensors[idx])
				break;

		sensorConnectTimer[idx] = HAL_GetTick();

		float temp;
		temp = *((float *)msg->payload);

		LoRa_Message msg2;
		sensorLastValue[idx] = temp;
		if(temp > ThresholdTempHigh){
			if(temp > ThresholdTempHigh + ThresholdWindow)
				sensorState[idx] |= SensorState_TooMuch;
			sensorState[idx] |= SensorState_High;

			setAlert = 1;
		}
		else if(temp < ThresholdTempLow){
			if(temp < ThresholdTempLow - ThresholdWindow)
				sensorState[idx] |= SensorState_TooMuch;
			sensorState[idx] |= SensorState_Low;

			setAlert = 1;
		}
		else{
			sensorState[idx] = SensorState_Normal;
		}


		pendingReply = true;
		msg2.address = msg->address;
		msg2.payload[0] = setAlert;
		msg2.payloadLength = 1;
		SX1278Drv_SendMessage(&msg2);

		UpdateChannelState();
	}
#ifndef DebugWithoutRelay
	else{ // LoRaDeviceType_Relay

		for(idx = 0; idx < RelayCount; idx++)
			if(msg->address == AddrRelays[idx])
				break;

		//relayConnectTimer[idx] = HAL_GetTick();
		relayNotConnected[idx] = true;
		lastRelayIdx = idx;
		osTimerStop(hRxTimer);
		pendingRequest = false;
	}
#endif
}

#ifndef DebugWithoutRelay
void SX1278Drv_LoRaRxError(){
	if(pendingRequest){
		osTimerStop(hRxTimer);
		osTimerStart(hRxTimer,SX1278Drv_GetRandomDelay(MinimumRxTimeout,1000));
	}
}
#endif

void SX1278Drv_LoRaTxCallback(LoRa_Message *msg){
	if(pendingReply){

		pendingReply = false;
#ifndef DebugWithoutRelay
		sendedMessageCount = 0;
		pendingRequest = true;
#endif
	}
	else{

	}
#ifndef DebugWithoutRelay
	osTimerStop(hRxTimer);
	osTimerStart(hRxTimer,SX1278Drv_GetRandomDelay(MinimumRxTimeout,1000));
#endif
}

void NeowayM590_SMSReceivedCallback(NeowayM590_SMS *sms){
	char *tok;
	bool needToSave = false;
	bool needToRespond = false;

	if((tok = strtok(sms->content," ")) == NULL)
		return;

	//checking if SMS is for assigning new owner
	if(!strcmp(tok,"pass")){
		if((tok = strtok(NULL," ")) == NULL)
			return;
		if(!strcmp(tok,GSMModemPass)){
			memcpy(GSMOwner,sms->number,strlen(sms->number)+1);
			needToSave = true;
			needToRespond = true;
		}
	}
	//If sender of SMS isn't current owner
	if(strcmp(GSMOwner,sms->number))
		return;

	if(!strcmp(tok,"set")){
		if((tok = strtok(NULL," ")) == NULL)
			return;

		//setting new temperature thresholds. command format is "set temp %f %f"
		if(!strcmp(tok,"temp")){

			if((tok = strtok(NULL,"")) == NULL)
				return;

			float tempL, tempH;
			if(sscanf(tok, "%f %f", &tempL, &tempH) == 2){
				ThresholdTempLow = tempL;
				ThresholdTempHigh = tempH;
				needToSave = true;
				needToRespond = true;
			}
			else
				return;
		}
		else
			return;
	}
	else if(!strcmp(tok,"get")){
		needToRespond = true;
		/*if((tok = strtok(NULL," ")) == NULL)
			return;*/
	}
	else;

	if(needToSave){
		HAL_FLASH_Unlock();
		FLASH_PageErase((uint32_t)flashVar);
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,(uint32_t)flashVar,*(uint64_t *)GSMOwner);
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,(uint32_t)(flashVar+8),*(uint64_t *)(GSMOwner+8));
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,(uint32_t)(flashVar+16),*(uint64_t *)(&ThresholdTempLow));
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,(uint32_t)(flashVar+20),*(uint64_t *)(&ThresholdTempHigh));
		HAL_FLASH_Lock();
	}

	if(needToRespond){
		NeowayM590_SMS sms2;
		memcpy(sms2.number, GSMOwner, strlen(GSMOwner) + 1);
		memcpy(sms2.content, "OK", 3);
		NeowayM590_SendSMS(&sms2);
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
  if (htim->Instance == TIM1)
    HAL_IncTick();
}
