/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"

#include "cmd.h"

#define SPP_TAG "SPP_INITIATOR"
#define EXCAMPLE_DEVICE_NAME "ESP_SPP_INITIATOR"

static const esp_spp_mode_t esp_spp_mode = ESP_SPP_MODE_CB;
static const esp_spp_sec_t sec_mask = ESP_SPP_SEC_AUTHENTICATE;
static const esp_spp_role_t role_master = ESP_SPP_ROLE_MASTER;

static esp_bd_addr_t peer_bd_addr;
static uint8_t peer_bdname_len;
static char peer_bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
static const char remote_device_name[] = "ESP_SPP_ACCEPTOR";
static const esp_bt_inq_mode_t inq_mode = ESP_BT_INQ_MODE_GENERAL_INQUIRY;
static const uint8_t inq_len = 30;
static const uint8_t inq_num_rsps = 0;

#define SPP_DATA_LEN 20
static uint8_t spp_data[SPP_DATA_LEN];

QueueHandle_t xQueueCmd;

#define CONFIG_STACK 0
#define CONFIG_STICKC 1
#define CONFIG_STICK 0

#if CONFIG_STACK
#include "ili9340.h"
#include "fontx.h"
#endif

#if CONFIG_STICKC
#include "axp192.h"
#include "st7735s.h"
#include "fontx.h"
#endif

#if CONFIG_STICK
#include "sh1107.h"
#include "font8x8_basic.h"
#endif

#if CONFIG_STACK
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define CS_GPIO 14
#define DC_GPIO 27
#define RESET_GPIO 33
#define BL_GPIO 32
#define FONT_WIDTH 12
#define FONT_HEIGHT 24
#define MAX_LINE 8
#define DISPLAY_LENGTH 26
#define GPIO_INPUT_A GPIO_NUM_39
#define GPIO_INPUT_B GPIO_NUM_38
#define GPIO_INPUT_C GPIO_NUM_37
#endif

#if CONFIG_STICKC
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 160
#define FONT_WIDTH 8
#define FONT_HEIGHT	16
#define MAX_LINE 8
#define DISPLAY_LENGTH 10
#define GPIO_INPUT GPIO_NUM_37
#endif

#if CONFIG_STICK
#define MAX_LINE 14
#define DISPLAY_LENGTH 8
#define GPIO_INPUT GPIO_NUM_35
#define GPIO_BUZZER	GPIO_NUM_26
#endif


static bool get_name_from_eir(uint8_t *eir, char *bdname, uint8_t *bdname_len)
{
	uint8_t *rmt_bdname = NULL;
	uint8_t rmt_bdname_len = 0;

	if (!eir) {
		return false;
	}

	rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &rmt_bdname_len);
	if (!rmt_bdname) {
		rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &rmt_bdname_len);
	}

	if (rmt_bdname) {
		if (rmt_bdname_len > ESP_BT_GAP_MAX_BDNAME_LEN) {
			rmt_bdname_len = ESP_BT_GAP_MAX_BDNAME_LEN;
		}

		if (bdname) {
			memcpy(bdname, rmt_bdname, rmt_bdname_len);
			bdname[rmt_bdname_len] = '\0';
		}
		if (bdname_len) {
			*bdname_len = rmt_bdname_len;
		}
		return true;
	}

	return false;
}

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
	CMD_t cmdBuf;
	switch (event) {
	case ESP_SPP_INIT_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_INIT_EVT");
		esp_bt_dev_set_device_name(EXCAMPLE_DEVICE_NAME);
		esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
		esp_bt_gap_start_discovery(inq_mode, inq_len, inq_num_rsps);
		break;
	case ESP_SPP_DISCOVERY_COMP_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_DISCOVERY_COMP_EVT status=%d scn_num=%d",param->disc_comp.status, param->disc_comp.scn_num);
		if (param->disc_comp.status == ESP_SPP_SUCCESS) {
			esp_spp_connect(sec_mask, role_master, param->disc_comp.scn[0], peer_bd_addr);
		}
		break;
	case ESP_SPP_OPEN_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_OPEN_EVT");
		//esp_spp_write(param->srv_open.handle, SPP_DATA_LEN, spp_data);
		cmdBuf.sppHandle = param->srv_open.handle;
		cmdBuf.command = CMD_OPEN;
		xQueueSend(xQueueCmd, &cmdBuf, 0);
		break;
	case ESP_SPP_CLOSE_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_CLOSE_EVT");
		cmdBuf.command = CMD_CLOSE;
		xQueueSend(xQueueCmd, &cmdBuf, 0);
		break;
	case ESP_SPP_START_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_START_EVT");
		break;
	case ESP_SPP_CL_INIT_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_CL_INIT_EVT");
		break;
	case ESP_SPP_DATA_IND_EVT:
		//ESP_LOGI(SPP_TAG, "ESP_SPP_DATA_IND_EVT");
		ESP_LOGI(SPP_TAG, "ESP_SPP_DATA_IND_EVT len=%d handle=%d",
				 param->data_ind.len, param->data_ind.handle);
		esp_log_buffer_hex("",param->data_ind.data,param->data_ind.len);
		break;
	case ESP_SPP_CONG_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_CONG_EVT cong=%d", param->cong.cong);
		if (param->cong.cong == 0) {
			esp_spp_write(param->cong.handle, SPP_DATA_LEN, spp_data);
		}
		break;
	case ESP_SPP_WRITE_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_WRITE_EVT len=%d cong=%d", param->write.len , param->write.cong);
		break;
	case ESP_SPP_SRV_OPEN_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_OPEN_EVT");
		break;
	default:
		break;
	}
}

static void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
	switch(event){
	case ESP_BT_GAP_DISC_RES_EVT:
		ESP_LOGI(SPP_TAG, "ESP_BT_GAP_DISC_RES_EVT");
		esp_log_buffer_hex(SPP_TAG, param->disc_res.bda, ESP_BD_ADDR_LEN);
		ESP_LOGI(SPP_TAG, "ESP_BT_GAP_DISC_RES_EVT param->disc_res.num_prop=%d", param->disc_res.num_prop);
		for (int i = 0; i < param->disc_res.num_prop; i++){
			if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_EIR
				&& get_name_from_eir(param->disc_res.prop[i].val, peer_bdname, &peer_bdname_len)){
				esp_log_buffer_char(SPP_TAG, peer_bdname, peer_bdname_len);
				if (strlen(remote_device_name) == peer_bdname_len
					&& strncmp(peer_bdname, remote_device_name, peer_bdname_len) == 0) {
					memcpy(peer_bd_addr, param->disc_res.bda, ESP_BD_ADDR_LEN);
					esp_spp_start_discovery(peer_bd_addr);
					esp_bt_gap_cancel_discovery();
				}
			}
		}
		break;
	case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
		ESP_LOGI(SPP_TAG, "ESP_BT_GAP_DISC_STATE_CHANGED_EVT");
		break;
	case ESP_BT_GAP_RMT_SRVCS_EVT:
		ESP_LOGI(SPP_TAG, "ESP_BT_GAP_RMT_SRVCS_EVT");
		break;
	case ESP_BT_GAP_RMT_SRVC_REC_EVT:
		ESP_LOGI(SPP_TAG, "ESP_BT_GAP_RMT_SRVC_REC_EVT");
		break;
	case ESP_BT_GAP_AUTH_CMPL_EVT:{
		if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
			ESP_LOGI(SPP_TAG, "authentication success: %s", param->auth_cmpl.device_name);
			esp_log_buffer_hex(SPP_TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
		} else {
			ESP_LOGE(SPP_TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
		}
		break;
	}
	case ESP_BT_GAP_PIN_REQ_EVT:{
		ESP_LOGI(SPP_TAG, "ESP_BT_GAP_PIN_REQ_EVT min_16_digit:%d", param->pin_req.min_16_digit);
		if (param->pin_req.min_16_digit) {
			ESP_LOGI(SPP_TAG, "Input pin code: 0000 0000 0000 0000");
			esp_bt_pin_code_t pin_code = {0};
			esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
		} else {
			ESP_LOGI(SPP_TAG, "Input pin code: 1234");
			esp_bt_pin_code_t pin_code;
			pin_code[0] = '1';
			pin_code[1] = '2';
			pin_code[2] = '3';
			pin_code[3] = '4';
			esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
		}
		break;
	}

#if (CONFIG_BT_SSP_ENABLED == true)
	case ESP_BT_GAP_CFM_REQ_EVT:
		ESP_LOGI(SPP_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %d", param->cfm_req.num_val);
		esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
		break;
	case ESP_BT_GAP_KEY_NOTIF_EVT:
		ESP_LOGI(SPP_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey:%d", param->key_notif.passkey);
		break;
	case ESP_BT_GAP_KEY_REQ_EVT:
		ESP_LOGI(SPP_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
		break;
#endif

	default:
		break;
	}
}

#if CONFIG_STICK || CONFIG_STICKC
void buttonStick(void *pvParameters)
{
	ESP_LOGI(pcTaskGetName(0), "Start");
	CMD_t cmdBuf;
	cmdBuf.taskHandle = xTaskGetCurrentTaskHandle();

	// set the GPIO as a input
	gpio_reset_pin(GPIO_INPUT);
	gpio_set_direction(GPIO_INPUT, GPIO_MODE_DEF_INPUT);

	while(1) {
		int level = gpio_get_level(GPIO_INPUT);
		if (level == 0) {
			ESP_LOGI(pcTaskGetName(0), "Push Button");
			cmdBuf.command = CMD_START;
			TickType_t startTick = xTaskGetTickCount();
			while(1) {
				level = gpio_get_level(GPIO_INPUT);
				if (level == 1) break;
				vTaskDelay(1);
			}
			TickType_t endTick = xTaskGetTickCount();
			TickType_t diffTick = endTick-startTick;
			ESP_LOGI(pcTaskGetName(0),"diffTick=%d",diffTick);
			cmdBuf.command = CMD_START;
			if (diffTick > 200) cmdBuf.command = CMD_STOP;
			xQueueSend(xQueueCmd, &cmdBuf, 0);
		}
		vTaskDelay(1);
	}
}
#endif


#if CONFIG_STACK
void buttonA(void *pvParameters)
{
	ESP_LOGI(pcTaskGetName(0), "Start");
	CMD_t cmdBuf;
	cmdBuf.command = CMD_SEND;
	strcpy((char *)cmdBuf.payload, "abcdefghijk");
	cmdBuf.length = strlen((char *)cmdBuf.payload);
	cmdBuf.taskHandle = xTaskGetCurrentTaskHandle();

	// set the GPIO as a input
	gpio_reset_pin(GPIO_INPUT_A);
	gpio_set_direction(GPIO_INPUT_A, GPIO_MODE_DEF_INPUT);

	while(1) {
		int level = gpio_get_level(GPIO_INPUT_A);
		if (level == 0) {
			ESP_LOGI(pcTaskGetName(0), "Push Button");
			while(1) {
				level = gpio_get_level(GPIO_INPUT_A);
				if (level == 1) break;
				vTaskDelay(1);
			}
			xQueueSend(xQueueCmd, &cmdBuf, 0);
		}
		vTaskDelay(1);
	}
}

void buttonB(void *pvParameters)
{
	ESP_LOGI(pcTaskGetName(0), "Start");
	CMD_t cmdBuf;
	cmdBuf.command = CMD_SEND;
	strcpy((char *)cmdBuf.payload, "01234567890");
	cmdBuf.length = strlen((char *)cmdBuf.payload);
	cmdBuf.taskHandle = xTaskGetCurrentTaskHandle();

	// set the GPIO as a input
	gpio_reset_pin(GPIO_INPUT_B);
	gpio_set_direction(GPIO_INPUT_B, GPIO_MODE_DEF_INPUT);

	while(1) {
		int level = gpio_get_level(GPIO_INPUT_B);
		if (level == 0) {
			ESP_LOGI(pcTaskGetName(0), "Push Button");
			while(1) {
				level = gpio_get_level(GPIO_INPUT_B);
				if (level == 1) break;
				vTaskDelay(1);
			}
			xQueueSend(xQueueCmd, &cmdBuf, 0);
		}
		vTaskDelay(1);
	}
}

void buttonC(void *pvParameters)
{
	ESP_LOGI(pcTaskGetName(0), "Start");
	CMD_t cmdBuf;
	cmdBuf.command = CMD_SEND;
	strcpy((char *)cmdBuf.payload, "ABCDEFGHIJK");
	cmdBuf.length = strlen((char *)cmdBuf.payload);
	cmdBuf.taskHandle = xTaskGetCurrentTaskHandle();

	// set the GPIO as a input
	gpio_reset_pin(GPIO_INPUT_C);
	gpio_set_direction(GPIO_INPUT_C, GPIO_MODE_DEF_INPUT);

	while(1) {
		int level = gpio_get_level(GPIO_INPUT_C);
		if (level == 0) {
			ESP_LOGI(pcTaskGetName(0), "Push Button");
			while(1) {
				level = gpio_get_level(GPIO_INPUT_C);
				if (level == 1) break;
				vTaskDelay(1);
			}
			xQueueSend(xQueueCmd, &cmdBuf, 0);
		}
		vTaskDelay(1);
	}
}

#endif


#if CONFIG_STACK
void tft(void *pvParameters)
{
	ESP_LOGI(pcTaskGetName(0), "Start");
	// set font file
	FontxFile fxG[2];
	InitFontx(fxG,"/spiffs/ILGH24XB.FNT",""); // 12x24Dot Gothic
	FontxFile fxM[2];
	InitFontx(fxM,"/spiffs/ILMH24XB.FNT",""); // 12x24Dot Mincyo

	// get font width & height
	uint8_t buffer[FontxGlyphBufSize];
	uint8_t fontWidth;
	uint8_t fontHeight;
	GetFontx(fxG, 0, buffer, &fontWidth, &fontHeight);
	ESP_LOGD(pcTaskGetName(0), "fontWidth=%d fontHeight=%d",fontWidth,fontHeight);

	// Setup Screen
	TFT_t dev;
	spi_master_init(&dev, CS_GPIO, DC_GPIO, RESET_GPIO, BL_GPIO);
	lcdInit(&dev, 0x9341, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0);
	ESP_LOGI(pcTaskGetName(0), "Setup Screen done");

	// Initial Screen
	uint8_t ascii[DISPLAY_LENGTH+1];
	lcdFillScreen(&dev, BLACK);
	lcdSetFontDirection(&dev, 0);
	strcpy((char *)ascii, "SPP INITIATOR");
	lcdDrawString(&dev, fxG, 0, FONT_HEIGHT-1, ascii, RED);
	strcpy((char *)ascii, "Not Connect");
	uint16_t xstatus = 15*FONT_WIDTH;
	lcdDrawString(&dev, fxG, xstatus, FONT_HEIGHT-1, ascii, RED);
	uint16_t ypos = (FONT_HEIGHT*2) - 1;
	uint16_t color = CYAN;
	uint32_t sppHandle = 0;
	bool clearScreen = false;

	CMD_t cmdBuf;
	while(1) {
		xQueueReceive(xQueueCmd, &cmdBuf, portMAX_DELAY);
		ESP_LOGI(pcTaskGetName(0),"cmdBuf.command=%d", cmdBuf.command);
		if (cmdBuf.command == CMD_OPEN) {
			sppHandle = cmdBuf.sppHandle;
			strcpy((char *)ascii, "Connect");
			lcdDrawFillRect(&dev, xstatus, 0, SCREEN_WIDTH-1, FONT_HEIGHT-1, BLACK);
			lcdDrawString(&dev, fxG, xstatus, FONT_HEIGHT-1, ascii, RED);

		} else if (cmdBuf.command == CMD_CLOSE) {
			sppHandle = 0;
			strcpy((char *)ascii, "Not Connect");
			lcdDrawFillRect(&dev, xstatus, 0, SCREEN_WIDTH-1, FONT_HEIGHT-1, BLACK);
			lcdDrawString(&dev, fxG, xstatus, FONT_HEIGHT-1, ascii, RED);

		} else if (cmdBuf.command == CMD_SEND) {
			if (sppHandle == 0) continue;
			esp_spp_write(sppHandle, cmdBuf.length, cmdBuf.payload);
			if (clearScreen) lcdDrawFillRect(&dev, 0, FONT_HEIGHT-1, SCREEN_WIDTH-1, SCREEN_HEIGHT-1, BLACK);
			lcdDrawString(&dev, fxM, 0, ypos, cmdBuf.payload, color);
			ypos = ypos + FONT_HEIGHT;
			clearScreen = false;
			if (ypos >= SCREEN_HEIGHT) {
				ypos = (FONT_HEIGHT*2) - 1;
				clearScreen = true;
			}
		}
	}

	// nerver reach
	while (1) {
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
}
#endif

#if CONFIG_STICKC
static void timer_cb(TimerHandle_t arg)
{
	static uint32_t counter = 0;
	CMD_t cmdBuf;
	cmdBuf.command = CMD_SEND;
	sprintf((char *)cmdBuf.payload, "This is M5StickC:%d", counter);
	cmdBuf.length = strlen((char *)cmdBuf.payload);
	xQueueSend(xQueueCmd, &cmdBuf, 0);
	counter++;
}
#endif


#if CONFIG_STICKC
void tft(void *pvParameters)
{
	ESP_LOGI(pcTaskGetName(0), "Start");
	// set font file
	FontxFile fxG[2];
	InitFontx(fxG,"/spiffs/ILGH16XB.FNT",""); // 8x16Dot Gothic
	FontxFile fxM[2];
	InitFontx(fxM,"/spiffs/ILMH16XB.FNT",""); // 8x16Dot Mincyo

	// get font width & height
	uint8_t buffer[FontxGlyphBufSize];
	uint8_t fontWidth;
	uint8_t fontHeight;
	GetFontx(fxG, 0, buffer, &fontWidth, &fontHeight);
	ESP_LOGD(pcTaskGetName(0), "fontWidth=%d fontHeight=%d",fontWidth,fontHeight);

	// Setup Screen
	ST7735_t dev;
	spi_master_init(&dev);
	lcdInit(&dev, SCREEN_WIDTH, SCREEN_HEIGHT);
	ESP_LOGI(pcTaskGetName(0), "Setup Screen done");

	// Initial Screen
	uint8_t ascii[DISPLAY_LENGTH+1];
	lcdFillScreen(&dev, BLACK);
	lcdSetFontDirection(&dev, 0);
	strcpy((char *)ascii, "SPP");
	lcdDrawString(&dev, fxG, 0, (FONT_HEIGHT*1)-1, ascii, YELLOW);
	strcpy((char *)ascii, "INITIATOR");
	lcdDrawString(&dev, fxG, 0, (FONT_HEIGHT*2)-1, ascii, YELLOW);
	strcpy((char *)ascii, "DisConnect");
	lcdDrawString(&dev, fxG, 0, (FONT_HEIGHT*4)-1, ascii, RED);

	uint32_t sppHandle = 0;
	bool sendStatus = false;
	CMD_t cmdBuf;

	while(1) {
		xQueueReceive(xQueueCmd, &cmdBuf, portMAX_DELAY);
		ESP_LOGI(pcTaskGetName(0),"cmdBuf.command=%d", cmdBuf.command);
		if (cmdBuf.command == CMD_OPEN) {
			sppHandle = cmdBuf.sppHandle;
			strcpy((char *)ascii, "Connect");
			lcdDrawFillRect(&dev, 0, (FONT_HEIGHT*3), SCREEN_WIDTH-1, (FONT_HEIGHT*4)-1, BLACK);
			lcdDrawString(&dev, fxG, 0, (FONT_HEIGHT*4)-1, ascii, CYAN);
			strcpy((char *)ascii, "Stop");
			lcdDrawFillRect(&dev, 0, (FONT_HEIGHT*4), SCREEN_WIDTH-1, (FONT_HEIGHT*5)-1, BLACK);
			lcdDrawString(&dev, fxG, 0, (FONT_HEIGHT*5)-1, ascii, RED);

		} else if (cmdBuf.command == CMD_CLOSE) {
			sppHandle = 0;
			strcpy((char *)ascii, "DisConnect");
			lcdDrawFillRect(&dev, 0, (FONT_HEIGHT*3), SCREEN_WIDTH-1, (FONT_HEIGHT*4)-1, BLACK);
			lcdDrawString(&dev, fxG, 0, (FONT_HEIGHT*4)-1, ascii, RED);
			//strcpy((char *)ascii, "Stop");
			lcdDrawFillRect(&dev, 0, (FONT_HEIGHT*4), SCREEN_WIDTH-1, (FONT_HEIGHT*5)-1, BLACK);
			//lcdDrawString(&dev, fxG, 0, (FONT_HEIGHT*5)-1, ascii, RED);

		} else if (cmdBuf.command == CMD_START) {
			if (sppHandle == 0) continue;
			strcpy((char *)ascii, "Start");
			lcdDrawFillRect(&dev, 0, (FONT_HEIGHT*4), SCREEN_WIDTH-1, (FONT_HEIGHT*5)-1, BLACK);
			lcdDrawString(&dev, fxG, 0, (FONT_HEIGHT*5)-1, ascii, CYAN);
			sendStatus = true;

		} else if (cmdBuf.command == CMD_STOP) {
			if (sppHandle == 0) continue;
			strcpy((char *)ascii, "Stop");
			lcdDrawFillRect(&dev, 0, (FONT_HEIGHT*4), SCREEN_WIDTH-1, (FONT_HEIGHT*5)-1, BLACK);
			lcdDrawString(&dev, fxG, 0, (FONT_HEIGHT*5)-1, ascii, RED);
			sendStatus = false;

		} else if (cmdBuf.command == CMD_SEND) {
			if (sppHandle == 0) continue;
			if (!sendStatus) continue;
			esp_spp_write(sppHandle, cmdBuf.length, cmdBuf.payload);
		}
	}

	// nerver reach
	while (1) {
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
}
#endif

#if CONFIG_STICK
static void timer_cb(TimerHandle_t arg)
{
	static uint32_t counter = 0;
	CMD_t cmdBuf;
	cmdBuf.command = CMD_SEND;
	sprintf((char *)cmdBuf.payload, "This is M5Stick:%d", counter);
	cmdBuf.length = strlen((char *)cmdBuf.payload);
	xQueueSend(xQueueCmd, &cmdBuf, 0);
	counter++;
}
#endif


#if CONFIG_STICK
void tft(void *pvParameters)
{
	ESP_LOGI(pcTaskGetName(0), "Start");

	// Setup Screen
	SH1107_t dev;
	spi_master_init(&dev);
	spi_init(&dev, 64, 128);
	ESP_LOGI(pcTaskGetName(0), "Setup Screen done");

	// Initial Screen
	clear_screen(&dev, false);
	display_contrast(&dev, 0xff);
	char ascii[DISPLAY_LENGTH+1];
	strcpy(ascii, "SPP	   ");
	display_text(&dev, 0, ascii, 8, false);
	strcpy(ascii, "INITIATE");
	display_text(&dev, 1, ascii, 8, false);

	uint32_t sppHandle = 0;
	bool sendStatus = false;
	CMD_t cmdBuf;

	while(1) {
		xQueueReceive(xQueueCmd, &cmdBuf, portMAX_DELAY);
		ESP_LOGI(pcTaskGetName(0),"cmdBuf.command=%d", cmdBuf.command);
		if (cmdBuf.command == CMD_OPEN) {
			sppHandle = cmdBuf.sppHandle;
			strcpy((char *)ascii, "Connect ");
			display_text(&dev, 3, ascii, 8, false);
			strcpy((char *)ascii, "Stop    ");
			display_text(&dev, 5, ascii, 8, false);

		} else if (cmdBuf.command == CMD_CLOSE) {
			sppHandle = 0;
			strcpy((char *)ascii, "		   ");
			display_text(&dev, 3, ascii, 8, false);
			strcpy((char *)ascii, "		   ");
			display_text(&dev, 5, ascii, 8, false);

		} else if (cmdBuf.command == CMD_START) {
			if (sppHandle == 0) continue;
			strcpy((char *)ascii, "Start   ");
			display_text(&dev, 5, ascii, 8, false);
			sendStatus = true;

		} else if (cmdBuf.command == CMD_STOP) {
			if (sppHandle == 0) continue;
			strcpy((char *)ascii, "Stop    ");
			display_text(&dev, 5, ascii, 8, false);
			sendStatus = false;

		} else if (cmdBuf.command == CMD_SEND) {
			if (sppHandle == 0) continue;
			if (!sendStatus) continue;
			esp_spp_write(sppHandle, cmdBuf.length, cmdBuf.payload);
		}
	}

	// nerver reach
	while (1) {
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
}
#endif



#if CONFIG_STICKC || CONFIG_STACK
static void SPIFFS_Directory(char * path) {
	DIR* dir = opendir(path);
	assert(dir != NULL);
	while (true) {
		struct dirent*pe = readdir(dir);
		if (!pe) break;
		ESP_LOGI(SPP_TAG,"d_name=%s d_ino=%d d_type=%x", pe->d_name,pe->d_ino, pe->d_type);
	}
	closedir(dir);
}
#endif

void app_main()
{
	for (int i = 0; i < SPP_DATA_LEN; ++i) {
		spp_data[i] = i;
	}

	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK( ret );

	ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
		ESP_LOGE(SPP_TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}
	ESP_LOGI(SPP_TAG, "esp_bt_controller_init");

	if ((ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
		ESP_LOGE(SPP_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}
	ESP_LOGI(SPP_TAG, "esp_bt_controller_enable");

	if ((ret = esp_bluedroid_init()) != ESP_OK) {
		ESP_LOGE(SPP_TAG, "%s initialize bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}
	ESP_LOGI(SPP_TAG, "esp_bluedroid_init");

	if ((ret = esp_bluedroid_enable()) != ESP_OK) {
		ESP_LOGE(SPP_TAG, "%s enable bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}
	ESP_LOGI(SPP_TAG, "esp_bluedroid_enable");

	if ((ret = esp_bt_gap_register_callback(esp_bt_gap_cb)) != ESP_OK) {
		ESP_LOGE(SPP_TAG, "%s gap register failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}
	ESP_LOGI(SPP_TAG, "esp_bt_gap_register_callback");

	if ((ret = esp_spp_register_callback(esp_spp_cb)) != ESP_OK) {
		ESP_LOGE(SPP_TAG, "%s spp register failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}
	ESP_LOGI(SPP_TAG, "esp_spp_register_callback");

	if ((ret = esp_spp_init(esp_spp_mode)) != ESP_OK) {
		ESP_LOGE(SPP_TAG, "%s spp init failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}
	ESP_LOGI(SPP_TAG, "esp_spp_init");

#if (CONFIG_BT_SSP_ENABLED == true)
	/* Set default parameters for Secure Simple Pairing */
	esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
	esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
	esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
	ESP_LOGI(SPP_TAG, "esp_bt_gap_set_security_param");
#endif

	/*
	 * Set default parameters for Legacy Pairing
	 * Use variable pin, input pin code when pairing
	 */
	esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
	esp_bt_pin_code_t pin_code;
	esp_bt_gap_set_pin(pin_type, 0, pin_code);
	ESP_LOGI(SPP_TAG, "esp_bt_gap_set_pin");


#if CONFIG_STICKC || CONFIG_STACK
	ESP_LOGI(SPP_TAG, "Initializing SPIFFS");
	esp_vfs_spiffs_conf_t conf = {
		.base_path = "/spiffs",
		.partition_label = NULL,
		.max_files = 6,
		.format_if_mount_failed =true
	};

	// Use settings defined above toinitialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is anall-in-one convenience function.
	ret =esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret ==ESP_FAIL) {
			ESP_LOGE(SPP_TAG, "Failed to mount or format filesystem");
		} else if (ret== ESP_ERR_NOT_FOUND) {
			ESP_LOGE(SPP_TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(SPP_TAG, "Failed to initialize SPIFFS (%s)",esp_err_to_name(ret));
		}
		return;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(NULL, &total, &used);
	if (ret != ESP_OK) {
		ESP_LOGE(SPP_TAG,"Failed to get SPIFFS partition information (%s)",esp_err_to_name(ret));
	} else {
		ESP_LOGI(SPP_TAG,"Partition size: total: %d, used: %d", total, used);
	}

	SPIFFS_Directory("/spiffs");
#endif

	/* Create Queue */
	xQueueCmd = xQueueCreate( 10, sizeof(CMD_t) );
	configASSERT( xQueueCmd );

#if CONFIG_STICKC
	// power on
	i2c_master_init();
	AXP192_PowerOn();
#endif

	xTaskCreate(tft, "TFT", 1024*4, NULL, 2, NULL);

#if CONFIG_STACK
	xTaskCreate(buttonA, "ButtonA", 1024*4, NULL, 2, NULL);
	xTaskCreate(buttonB, "ButtonB", 1024*4, NULL, 2, NULL);
	xTaskCreate(buttonC, "ButtonC", 1024*4, NULL, 2, NULL);
#endif


#if CONFIG_STICK || CONFIG_STICKC
	TimerHandle_t timer = xTimerCreate("send_timer", 5000 / portTICK_PERIOD_MS, true, NULL, timer_cb);
	xTimerStart(timer, 0);
	xTaskCreate(buttonStick, "BUTTON", 1024*4, NULL, 2, NULL);
#endif
}


