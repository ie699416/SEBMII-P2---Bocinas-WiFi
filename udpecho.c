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

#include "fsl_dac.h"
#include "fsl_pit.h"

#define NS 60
#define CHARS 4

#define EVENT_PIT (1<<0)
#define EVENT_PING_PONG (1<<1)

#define PIT_FS_HANDLER PIT0_IRQHandler
#define PIT_IRQ_ID PIT0_IRQn
#define USEC_TO_COUNT(us, clockFreqInHz) (uint64_t)((uint64_t)us * clockFreqInHz / 1000000U)
#define PIT_SOURCE_CLOCK CLOCK_GetFreq(kCLOCK_BusClk)

EventGroupHandle_t pingPong_events;
QueueHandle_t pingPongA_buffer;
QueueHandle_t pingPongB_buffer;

volatile bool pitIsrFlag = false;

void PIT_FS_HANDLER(void) {
	/* Clear interrupt flag.*/
	PIT_ClearStatusFlags(PIT, 0, kPIT_TimerFlag);
	pitIsrFlag = true;

//	BaseType_t xHigherPriorityTaskWoken;
//	BaseType_t xResult;
//
//	xHigherPriorityTaskWoken = pdFALSE;
//	xResult = pdFAIL;
//	xResult = xEventGroupSetBits(pingPong_events,
//	EVENT_PIT, &xHigherPriorityTaskWoken);
//
//	if (pdFAIL != xResult) {
//		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
//	}

}

static void server_thread(void *arg) {
	struct netconn *conn;
	struct netbuf *buf;

	uint8_t *msg;
	uint8_t DAC_buffer[CHARS * NS] = { };

	uint16_t buffer_index = 0;
	uint16_t len;
	uint16_t outData_x;

	LWIP_UNUSED_ARG(arg);
	conn = netconn_new(NETCONN_UDP);
	netconn_bind(conn, IP_ADDR_ANY, 5005);
	//LWIP_ERROR("udpecho: invalid conn", (conn != NULL), return;);

	while (1) {

		netconn_recv(conn, &buf);

		if (ERR_OK == netbuf_data(buf, (void**) &msg, &len)) {

			for (buffer_index = 0; buffer_index < len; buffer_index++) {
				DAC_buffer[buffer_index] = *msg - '0';
				msg++;
			}

			for (uint8_t Ns_index = 0; Ns_index < (NS * CHARS); Ns_index += 4) {

				outData_x = (DAC_buffer[Ns_index + 0] * 1000
						+ DAC_buffer[Ns_index + 1] * 100
						+ DAC_buffer[Ns_index + 2] * 10
						+ DAC_buffer[Ns_index + 3] * 1);

				if (EVENT_PING_PONG & xEventGroupGetBits(pingPong_events)) {
					xQueueSendToBack(pingPongA_buffer, &outData_x, 10);
				} else {
					xQueueSendToBack(pingPongB_buffer, &outData_x, 10);
				}

			}

			if (EVENT_PING_PONG & xEventGroupGetBits(pingPong_events)) {
				xEventGroupClearBits(pingPong_events, EVENT_PING_PONG);
			} else {
				xEventGroupSetBits(pingPong_events, EVENT_PING_PONG);
			}
		}

buffer_index = 0;

		netbuf_delete(buf);

	}
}

void PIT_task(void * arg) {

	dac_config_t dacConfigStruct;
	pit_config_t pitConfig;
	uint16_t outData_x;

	pingPong_events = xEventGroupCreate();
	pingPongA_buffer = xQueueCreate(NS, sizeof(uint16_t));
	pingPongB_buffer = xQueueCreate(NS, sizeof(uint16_t));

	DAC_GetDefaultConfig(&dacConfigStruct);
	DAC_Init(DAC0, &dacConfigStruct);
	DAC_Enable(DAC0, true); /* Enable output. */

	PIT_GetDefaultConfig(&pitConfig);

	PIT_Init(PIT, &pitConfig);

	/* Enable timer interrupts for channel 0 */
	PIT_EnableInterrupts(PIT, kPIT_Chnl_0, kPIT_TimerInterruptEnable);

	/* Enable at the NVIC */
	EnableIRQ(PIT_IRQ_ID);

	PIT_SetTimerPeriod(PIT, 0, USEC_TO_COUNT(23U, PIT_SOURCE_CLOCK));

	PIT_StartTimer(PIT, kPIT_Chnl_0);

	for (;;) {
		if (true == pitIsrFlag) {
			pitIsrFlag = false;

			if (EVENT_PING_PONG & xEventGroupGetBits(pingPong_events)) {
				xQueueReceive(pingPongB_buffer, &outData_x, 10);
			} else {
				xQueueReceive(pingPongA_buffer, &outData_x, 10);
			}

			DAC_SetBufferValue(DAC0, 0U, (outData_x));
		}

	}
}

/*-----------------------------------------------------------------------------------*/
void udpecho_init(void) {
	sys_thread_new("server", server_thread, NULL, 300, 2);

	xTaskCreate(PIT_task, "PIT task", 200, NULL, configMAX_PRIORITIES,
	NULL);

}

#endif /* LWIP_NETCONN */
