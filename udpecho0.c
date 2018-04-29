/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#include "udpecho.h"

#include "lwip/opt.h"

#if LWIP_NETCONN

#include "lwip/api.h"
#include "lwip/sys.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "semphr.h"

#include "fsl_dac.h"
#include "fsl_pit.h"
#include "fsl_gpio.h"
#include "fsl_common.h"
#include "fsl_port.h"

#include "MK64F12.h"

#define NS 300
#define BYTES 2

#define EVENT_PIT (1<<0)
#define EVENT_PING_PONG (1<<1)
#define EVENT_MAX_COUNT (1<<2)

#define PIT_FS_HANDLER PIT0_IRQHandler
#define PIT_IRQ_ID PIT0_IRQn
#define USEC_TO_COUNT(us, clockFreqInHz) (uint64_t)((uint64_t)us * clockFreqInHz / 1000000U)
#define PIT_SOURCE_CLOCK CLOCK_GetFreq(kCLOCK_BusClk)

EventGroupHandle_t pingPong_events;
QueueHandle_t pingPongA_buffer;
QueueHandle_t pingPongB_buffer;
SemaphoreHandle_t PIT_semaphore;

uint16_t pingpong_A[NS] = { };
uint16_t pingpong_B[NS] = { };
uint16_t outData_x;
uint16_t * pingpong_A_ptr = pingpong_A;
uint16_t * pingpong_B_ptr = pingpong_A;
uint16_t pingpongA_semaphore;
uint16_t pingpongB_semaphore;

void PIT_FS_HANDLER(void) {
	/* Clear interrupt flag.*/
	PIT_ClearStatusFlags(PIT, 0, kPIT_TimerFlag);
	BaseType_t xHigherPriorityTaskWoken;
	xHigherPriorityTaskWoken = pdFALSE;
	xSemaphoreGiveFromISR(PIT_semaphore, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

}

static void server_thread(void *arg) {
	struct netconn *conn;
	struct netbuf *buf;

	uint8_t *msg;
	uint8_t UDP_buffer[BYTES * NS] = { };
	uint16_t DAC_buffer[NS];
	uint16_t * DAC_value;

	uint16_t buffer_index = 0;
	uint16_t len;

	LWIP_UNUSED_ARG(arg);
	conn = netconn_new(NETCONN_UDP);
	netconn_bind(conn, IP_ADDR_ANY, 50005);
	//LWIP_ERROR("udpecho: invalid conn", (conn != NULL), return;);

	CLOCK_EnableClock(kCLOCK_PortA);

	port_pin_config_t config_switch = { kPORT_PullUp, kPORT_SlowSlewRate,
			kPORT_PassiveFilterDisable, kPORT_OpenDrainDisable,
			kPORT_LowDriveStrength, kPORT_MuxAsGpio, kPORT_UnlockRegister };

	PORT_SetPinConfig(PORTA, 1, &config_switch);

	gpio_pin_config_t switch_config_gpio = { kGPIO_DigitalOutput, 1 };

	GPIO_PinInit(GPIOA, 1, &switch_config_gpio);

	while (1) {

		netconn_recv(conn, &buf);

		err_enum_t err = netbuf_data(buf, (void**) &msg, &len);

		if (ERR_OK == err) {


			xEventGroupSetBits(pingPong_events, EVENT_MAX_COUNT);

			if (EVENT_PING_PONG & xEventGroupGetBits(pingPong_events)) {

				for (buffer_index = 0; buffer_index < NS; buffer_index++) {
					GPIO_PinWrite(GPIOA, 1, 0);
					DAC_value = msg;
					DAC_buffer[buffer_index] = *msg;
					xQueueSendToBack(pingPongA_buffer, DAC_value, 1);
					pingpongA_semaphore++;
					msg++;
					msg++;
					DAC_value++;
					GPIO_PinWrite(GPIOA, 1, 1);
				}

				xEventGroupClearBits(pingPong_events, EVENT_PING_PONG);

			} else {

				for (buffer_index = 0; buffer_index < NS; buffer_index++) {
					GPIO_PinWrite(GPIOA, 1, 0);
					DAC_value = msg;
					xQueueSendToBack(pingPongB_buffer, DAC_value, 1);
					pingpongB_semaphore++;
					msg++;
					msg++;
					DAC_value++;
					GPIO_PinWrite(GPIOA, 1, 1);
				}

				xEventGroupSetBits(pingPong_events, EVENT_PING_PONG);
			}

			buffer_index = 0;

			netbuf_delete(buf);


		}
	}
}

uint16_t outData_x;

void PIT_task(void * arg) {

	dac_config_t dacConfigStruct;
	pit_config_t pitConfig;

	pingPong_events = xEventGroupCreate();
	pingPongA_buffer = xQueueCreate(NS, sizeof(uint16_t));
	pingPongB_buffer = xQueueCreate(NS, sizeof(uint16_t));

	DAC_GetDefaultConfig(&dacConfigStruct);
	DAC_Init(DAC0, &dacConfigStruct);
	DAC_Enable(DAC0, true); /* Enable output. */

	PIT_GetDefaultConfig(&pitConfig);

	PIT_semaphore = xSemaphoreCreateBinary();

	PIT_Init(PIT, &pitConfig);

	/* Enable timer interrupts for channel 0 */
	PIT_EnableInterrupts(PIT, kPIT_Chnl_0, kPIT_TimerInterruptEnable);

	/* Enable at the NVIC */
	EnableIRQ(PIT_IRQ_ID);

	NVIC_SetPriority(PIT_IRQ_ID, 5);

	PIT_SetTimerPeriod(PIT, 0, USEC_TO_COUNT(22U, PIT_SOURCE_CLOCK));

	PIT_StartTimer(PIT, kPIT_Chnl_0);

	for (;;) {
		xSemaphoreTake(PIT_semaphore, portMAX_DELAY);

		if (EVENT_PING_PONG & xEventGroupGetBits(pingPong_events)) {

			if (0 == pingpongB_semaphore) {
				outData_x = 2048;
				DAC_SetBufferValue(DAC0, 0U, (outData_x));
				xEventGroupWaitBits(pingPong_events, EVENT_MAX_COUNT,
				pdTRUE, pdFALSE, portMAX_DELAY);

			} else {
				pingpongB_semaphore--;
				xQueueReceive(pingPongB_buffer, &outData_x, 1);

			}
		} else {
			if (0 == pingpongA_semaphore) {
				outData_x = 2048;
				DAC_SetBufferValue(DAC0, 0U, (outData_x));
				xEventGroupWaitBits(pingPong_events, EVENT_MAX_COUNT,
				pdTRUE, pdFALSE, portMAX_DELAY);

			} else {
				pingpongA_semaphore--;
				xQueueReceive(pingPongA_buffer, &outData_x, 1);

			}
		}

		DAC_SetBufferValue(DAC0, 0U, (outData_x));

	}
}

/*-----------------------------------------------------------------------------------*/
void udpecho_init(void) {
	sys_thread_new("server", server_thread, NULL, 600, 2);

	pingpongA_semaphore = 0;
	pingpongB_semaphore = 0;

	xTaskCreate(PIT_task, "PIT task", 200, NULL, configMAX_PRIORITIES,
	NULL);

}

#endif /* LWIP_NETCONN */
