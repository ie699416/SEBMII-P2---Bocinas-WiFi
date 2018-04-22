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
#include "semphr.h"

#include "fsl_dac.h"
#include "fsl_pit.h"

#define NS 10
#define CHARS 4

static void server_thread(void *arg) {
	struct netconn *conn;
	struct netbuf *buf;

	uint8_t *msg;
	uint8_t DAC_buffer[CHARS * NS] = { };

	uint16_t buffer_index = 0;
	uint16_t len;

	LWIP_UNUSED_ARG(arg);
	conn = netconn_new(NETCONN_UDP);
	netconn_bind(conn, IP_ADDR_ANY, 5005);
	//LWIP_ERROR("udpecho: invalid conn", (conn != NULL), return;);

	dac_config_t dacConfigStruct;
	uint16_t outData_x;
	
	SemaphoreHandle_t pingPong = xSemaphoreCreateBinary();

	QueueHandle_t pingPongA_buffer = xQueueCreate(NS, sizeof(uint16_t));
	QueueHandle_t pingPongB_buffer = xQueueCreate(NS, sizeof(uint16_t));

	DAC_GetDefaultConfig(&dacConfigStruct);
	DAC_Init(DAC0, &dacConfigStruct);
	DAC_Enable(DAC0, true); /* Enable output. */

	while (1) {
		netconn_recv(conn, &buf);

		if (ERR_OK == netbuf_data(buf, (void**) &msg, &len)) {
			for (buffer_index = 0; buffer_index < len; buffer_index++) {
				DAC_buffer[buffer_index] = *msg - '0';
				msg++;
			}

			buffer_index = 0;

		}

		for (uint8_t Ns_index = 0; Ns_index < (NS * CHARS); Ns_index += 4) {

			outData_x = (DAC_buffer[Ns_index + 0] * 1000
					+ DAC_buffer[Ns_index + 1] * 100
					+ DAC_buffer[Ns_index + 2] * 10
					+ DAC_buffer[Ns_index + 3] * 1);
			
			xQueueSendToBack(pingPongA_buffer, outData_x, 10);
		}

		DAC_SetBufferValue(DAC0, 0U, (outData_x));

		netbuf_delete(buf);

	}
}

/*-----------------------------------------------------------------------------------*/
static void client_thread(void *arg) {
	ip_addr_t dst_ip;
	struct netconn *conn;
	struct netbuf *buf;

	LWIP_UNUSED_ARG(arg);
	conn = netconn_new(NETCONN_UDP);
	//LWIP_ERROR("udpecho: invalid conn", (conn != NULL), return;);

	char *msg = "Hello loopback!";
	buf = netbuf_new();
	netbuf_ref(buf, msg, 10);

	IP4_ADDR(&dst_ip, 192, 168, 1, 100);

	while (1) {
		netconn_sendto(conn, buf, &dst_ip, 50005);
		vTaskDelay(1000);
	}
}
/*-----------------------------------------------------------------------------------*/
void udpecho_init(void) {
	sys_thread_new("client", client_thread, NULL, 300, 1);
	sys_thread_new("server", server_thread, NULL, 300, 2);

}

#endif /* LWIP_NETCONN */
