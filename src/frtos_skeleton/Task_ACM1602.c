/***********************************************************************************************************************
* DISCLAIMER
* This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No
* other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
* applicable laws, including copyright laws.
* THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
* THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM
* EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES
* SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS
* SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
* Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of
* this software. By using this software, you agree to the additional terms and conditions found by accessing the
* following link:
* http://www.renesas.com/disclaimer
*
* Copyright (C) 2019 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
#include "task_function.h"
#include "r_smc_entry.h"
#include "platform.h"
#include "r_sci_iic_rx_if.h"
#include "semphr.h"


#define USE_LED
//#define DEBUG_PRINT

#define ACM1602_I2C_CH					( 3 )
#define ACM1602_SLAVE_ADDRESS			( 0x50 )
#define ACM1602_WAIT_TIMEOUT			( 3000 )		// ms

sci_iic_info_t				g_tSciI2cInfo;								// 簡易I2Cハンドル
uint8_t						g_SlaveAddr = ACM1602_SLAVE_ADDRESS;		// ACM1602スレーブアドレス
SemaphoreHandle_t			g_pCallbackSemaphorHandle;					// コールバック同期用セマフォ

// プロトタイプ宣言
void main(void);
void ACM1602_Abort(void);
sci_iic_return_t ACM1602_Init(void);
void ACM1602_Callback(void);
sci_iic_return_t master_send(sci_iic_info_t *p_sci_iic_info);

sci_iic_return_t ACM1602_ClearDisplay(void);
sci_iic_return_t ACM1602_ReturnHome(void);
sci_iic_return_t ACM1602_SetEntryMode(uint8_t CursorMoving, uint8_t EnableShift);
sci_iic_return_t ACM1602_DisplayControl(uint8_t Display, uint8_t Cursor, uint8_t Blink);
sci_iic_return_t ACM1602_SetFunction(uint8_t DataLength, uint8_t DisplayLine, uint8_t DisplayFont);
sci_iic_return_t ACM1602_SetDdramAddress(uint8_t Addr);
sci_iic_return_t ACM1602_WriteDataRam(uint8_t Data);
sci_iic_return_t ACM1602_WriteData(const uint8_t *pszData, uint8_t DataSize);


//***********************************************************************************
// ACM1602タスク
//***********************************************************************************
void Task_ACM1602(void * pvParameters)
{
	sci_iic_return_t		eSciI2cReturn = SCI_IIC_SUCCESS;
	uint8_t					ch = 0x20;

#ifdef USE_LED
	PORTA.PDR.BIT.B1 = 1;
	PORTA.PODR.BIT.B1 = 0;
#endif	// #ifdef USE_LED

	// コールバック同期用セマフォ生成
	g_pCallbackSemaphorHandle = xSemaphoreCreateBinary();
	if (g_pCallbackSemaphorHandle == NULL)
	{
		printf("xSemaphoreCreateBinary Error.\n");
		vTaskDelete(NULL);
		return;
	}

	// 簡易I2Cモード初期化
	eSciI2cReturn = ACM1602_Init();
	if (eSciI2cReturn != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINT
		printf("R_SCI_ICC_Open Error! [eSciI2cReturn:%d]\n", eSciI2cReturn);
#endif // #ifdef DEBUG_PRINT
		ACM1602_Abort();
	}

	// ディスプレイクリア
	eSciI2cReturn = ACM1602_ClearDisplay();
	if (eSciI2cReturn != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINT
		printf("ACM1602_ClearDisplay Error! [eSciI2cReturn:%d]\n", eSciI2cReturn);
#endif // #ifdef DEBUG_PRINT
		ACM1602_Abort();
	}

	// ファンクション設定
	eSciI2cReturn = ACM1602_SetFunction(1,1,0);
	if (eSciI2cReturn != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINT
		printf("ACM1602_SetFunction Error! [eSciI2cReturn:%d]\n", eSciI2cReturn);
#endif // #ifdef DEBUG_PRINT
		ACM1602_Abort();
	}

	// ディスプレイ制御
	eSciI2cReturn = ACM1602_DisplayControl(1,1,1);
	if (eSciI2cReturn != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINT
		printf("ACM1602_DisplayControl Error! [eSciI2cReturn:%d]\n", eSciI2cReturn);
#endif // #ifdef DEBUG_PRINT
		ACM1602_Abort();
	}


	// エントリモード設定
	eSciI2cReturn = ACM1602_SetEntryMode(1,0);
	if (eSciI2cReturn != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINT
		printf("ACM1602_SetEntryMode Error! [eSciI2cReturn:%d]\n", eSciI2cReturn);
#endif // #ifdef DEBUG_PRINT
		ACM1602_Abort();
	}

	// DDRAMアドレス設定
	eSciI2cReturn = ACM1602_SetDdramAddress(0x00);
	if (eSciI2cReturn != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINT
		printf("ACM1602_SetEntryMode Error! [eSciI2cReturn:%d]\n", eSciI2cReturn);
#endif // #ifdef DEBUG_PRINT
		ACM1602_Abort();
	}

	while(1)
	{

		eSciI2cReturn = ACM1602_WriteDataRam(ch);
		if (eSciI2cReturn != SCI_IIC_SUCCESS)
		{
#ifdef DEBUG_PRINT
			printf("ACM1602_WriteDataRam Error! [eSciI2cReturn:%d]\n", eSciI2cReturn);
#endif // #ifdef DEBUG_PRINT
			ACM1602_Abort();
		}

		ch = ch + 1;
		if ((ch >= 0x80) && (ch <= 0xA0))
		{
			ch = 0xA1;
		}
		else if (ch == 0x00)
		{
			ch = 0x20;
		}
	}
}

//*******************************************************
// 異常終了
//*******************************************************
void ACM1602_Abort(void)
{
#ifdef DEBUG_PRINT
	printf("*** ACM1602_Abort! ***\n");
#endif // #ifdef DEBUG_PRINT

	while(1)
	{
#ifdef USE_LED
		PORTA.PODR.BIT.B1 = 1;
		vTaskDelay(100);
		PORTA.PODR.BIT.B1 = 0;
		vTaskDelay(100);
		PORTA.PODR.BIT.B1 = 1;
		vTaskDelay(100);
		PORTA.PODR.BIT.B1 = 0;
		vTaskDelay(100);
		PORTA.PODR.BIT.B1 = 1;
		vTaskDelay(100);
		PORTA.PODR.BIT.B1 = 0;
		vTaskDelay(500);
#endif	// #ifdef USE_LED
	}
}

//*******************************************************
// 簡易I2Cモード初期化
//*******************************************************
sci_iic_return_t ACM1602_Init(void)
{
	sci_iic_return_t		eSciI2cReturn = SCI_IIC_SUCCESS;

	// 簡易I2Cモード初期化
	g_tSciI2cInfo.ch_no = ACM1602_I2C_CH;
	g_tSciI2cInfo.p_slv_adr = &g_SlaveAddr;
	g_tSciI2cInfo.dev_sts = SCI_IIC_NO_INIT;
	g_tSciI2cInfo.p_data1st = NULL;
	g_tSciI2cInfo.cnt1st = 0;
	g_tSciI2cInfo.p_data2nd = NULL;
	g_tSciI2cInfo.cnt2nd = 0;
	g_tSciI2cInfo.callbackfunc = ACM1602_Callback;
	eSciI2cReturn = R_SCI_IIC_Open(&g_tSciI2cInfo);
	if (eSciI2cReturn != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINT
		printf("R_SCI_ICC_Open Error! [eSciI2cReturn:%d]\n", eSciI2cReturn);
#endif // #ifdef DEBUG_PRINT
	}

	return eSciI2cReturn;
}


//*******************************************************
// 簡易I2Cモード初期化
//*******************************************************
sci_iic_return_t ACM1602_ReInit(void)
{
	sci_iic_return_t		eSciI2cReturn = SCI_IIC_SUCCESS;

	// クローズ
	eSciI2cReturn = R_SCI_IIC_Close(&g_tSciI2cInfo);
	if (eSciI2cReturn != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINT
		printf("R_SCI_IIC_Close Error! [eSciI2cReturn:%d]\n", eSciI2cReturn);
#endif // #ifdef DEBUG_PRINT
		ACM1602_Abort();
	}

	// オープン
	eSciI2cReturn = R_SCI_IIC_Open(&g_tSciI2cInfo);
	if (eSciI2cReturn != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINT
		printf("R_SCI_IIC_Open Error! [eSciI2cReturn:%d]\n", eSciI2cReturn);
#endif // #ifdef DEBUG_PRINT
		ACM1602_Abort();
	}

	return eSciI2cReturn;
}



//*******************************************************
// コールバック処理
//*******************************************************
void ACM1602_Callback(void)
{
	BaseType_t 							bHigherPriorityTaskWoken = pdFALSE;

#ifdef DEBUG_PRINT
	printf("+++ ACM1602_Callback +++\n");
#endif // #ifdef DEBUG_PRINT

	xSemaphoreGiveFromISR(g_pCallbackSemaphorHandle,&bHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(bHigherPriorityTaskWoken);
}


//*******************************************************
// マスター送信処理
//*******************************************************
sci_iic_return_t master_send(sci_iic_info_t *p_sci_iic_info)
{
	sci_iic_return_t		eSciI2cReturn = SCI_IIC_SUCCESS;
	sci_iic_info_t			tSciI2cInfo;
	sci_iic_mcu_status_t	tSciI2cStatus;
	uint8_t					RetryCount = 0;
	uint8_t					Loop = 1;
	BaseType_t 				bRet = pdFALSE;


	while (Loop)
	{
		if (RetryCount >= 3)
		{
			printf( "Retry Count Error!\n");
			return SCI_IIC_ERR_OTHER;
		}

		// マスター送信
#ifdef DEBUG_PRINT
		printf( "Command[0]:0x%02X, Command[1]:0x%02X\n", *p_sci_iic_info->p_data1st, *p_sci_iic_info->p_data2nd);
#endif // #ifdef DEBUG_PRINT
		eSciI2cReturn = R_SCI_IIC_MasterSend(p_sci_iic_info);
		if (eSciI2cReturn != SCI_IIC_SUCCESS)
		{
#ifdef DEBUG_PRINT
			printf("R_SCI_IIC_MasterSend Error! [eSciI2cReturn:%d]\n", eSciI2cReturn);
#endif // #ifdef DEBUG_PRINT
			return eSciI2cReturn;
		}

		// コールバック応答待ち
		bRet = xSemaphoreTake(g_pCallbackSemaphorHandle, ACM1602_WAIT_TIMEOUT);
		if (bRet == pdFALSE)
		{
#ifdef DEBUG_PRINT
			printf("xSemaphoreTake Error.\n");
#endif // #ifdef DEBUG_PRINT
			return SCI_IIC_ERR_OTHER;
		}

		// 状態取得
		tSciI2cInfo.ch_no = ACM1602_I2C_CH;
		eSciI2cReturn = R_SCI_IIC_GetStatus(&tSciI2cInfo,&tSciI2cStatus);
		if (eSciI2cReturn != SCI_IIC_SUCCESS)
		{
#ifdef DEBUG_PRINT
			printf("R_SCI_IIC_GetStatus Error! [eSciI2cReturn:%d]\n", eSciI2cReturn);
#endif // #ifdef DEBUG_PRINT
			ACM1602_ReInit();
			R_BSP_SoftwareDelay(100,BSP_DELAY_MILLISECS);
			RetryCount++;
			continue;
		}
		// NACK?
		if (tSciI2cStatus.BIT.NACK == 1)
		{
#ifdef DEBUG_PRINT
			printf("[NACK]\n");
#endif // #ifdef DEBUG_PRINT
			ACM1602_ReInit();
			R_BSP_SoftwareDelay(100,BSP_DELAY_MILLISECS);
			RetryCount++;
			continue;
		}
		else
		{
			switch(p_sci_iic_info->dev_sts){
			case SCI_IIC_FINISH:
				eSciI2cReturn = SCI_IIC_SUCCESS;
				Loop = 0;
				break;
			case SCI_IIC_NO_INIT:
			case SCI_IIC_IDLE:
			case SCI_IIC_NACK:
			case SCI_IIC_COMMUNICATION:
			case SCI_IIC_ERROR:
			default:
				eSciI2cReturn = SCI_IIC_ERR_OTHER;
#ifdef DEBUG_PRINT
				printf("Status Error! [p_sci_iic_info->dev_sts:%d]\n",p_sci_iic_info->dev_sts);
#endif // #ifdef DEBUG_PRINT
				ACM1602_ReInit();
				R_BSP_SoftwareDelay(100,BSP_DELAY_MILLISECS);
				RetryCount++;
				continue;
			}
		}
	}

	return eSciI2cReturn;
}


//*******************************************************
// ディスプレイクリア
//*******************************************************
sci_iic_return_t ACM1602_ClearDisplay(void)
{
	sci_iic_return_t		eSciI2cReturn = SCI_IIC_SUCCESS;
	uint8_t					Command[2] = { 0x00, 0x01 };

	// マスター送信
	g_tSciI2cInfo.p_data1st = &Command[0];
	g_tSciI2cInfo.cnt1st = 1;
	g_tSciI2cInfo.p_data2nd = &Command[1];
	g_tSciI2cInfo.cnt2nd = 1;
	eSciI2cReturn = master_send(&g_tSciI2cInfo);
	if (eSciI2cReturn != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINT
		printf("master_send Error! [eSciI2cReturn:%d]\n", eSciI2cReturn);
#endif // #ifdef DEBUG_PRINT
		return eSciI2cReturn;
	}

	return eSciI2cReturn;
}

//*******************************************************
// カーソルリセット
//*******************************************************
sci_iic_return_t ACM1602_ReturnHome(void)
{
	sci_iic_return_t		eSciI2cReturn = SCI_IIC_SUCCESS;
	uint8_t					Command[2] = { 0x00, 0x02 };

	// マスター送信
	g_tSciI2cInfo.p_data1st = &Command[0];
	g_tSciI2cInfo.cnt1st = 1;
	g_tSciI2cInfo.p_data2nd = &Command[1];
	g_tSciI2cInfo.cnt2nd = 1;
	eSciI2cReturn = master_send(&g_tSciI2cInfo);
	if (eSciI2cReturn != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINT
		printf("master_send Error! [eSciI2cReturn:%d]\n", eSciI2cReturn);
#endif // #ifdef DEBUG_PRINT
		return eSciI2cReturn;
	}

	return eSciI2cReturn;
}


//*******************************************************
// エントリモード設定
//*******************************************************
sci_iic_return_t ACM1602_SetEntryMode(uint8_t CursorMoving, uint8_t EnableShift)
{
	sci_iic_return_t		eSciI2cReturn = SCI_IIC_SUCCESS;
	uint8_t					Command[2] = { 0x00, 0x04 };

	// CursorMoving(0:OFF, 1:ON)
	Command[1] |= ((CursorMoving != 0) ? 0x02 : 0x00);

	// EnableShift(0:OFF, 1:ON)
	Command[1] |= ((EnableShift != 0) ? 0x01 : 0x00);

	// マスター送信
	g_tSciI2cInfo.p_data1st = &Command[0];
	g_tSciI2cInfo.cnt1st = 1;
	g_tSciI2cInfo.p_data2nd = &Command[1];
	g_tSciI2cInfo.cnt2nd = 1;
	eSciI2cReturn = master_send(&g_tSciI2cInfo);
	if (eSciI2cReturn != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINT
		printf("master_send Error! [eSciI2cReturn:%d]\n", eSciI2cReturn);
#endif // #ifdef DEBUG_PRINT
		return eSciI2cReturn;
	}

	return eSciI2cReturn;
}


//*******************************************************
// ディスプレイ制御
//*******************************************************
sci_iic_return_t ACM1602_DisplayControl(uint8_t Display, uint8_t Cursor, uint8_t Blink)
{
	sci_iic_return_t		eSciI2cReturn = SCI_IIC_SUCCESS;
	uint8_t					Command[2] = { 0x00, 0x08 };

	// Display(0:OFF, 1:ON)
	Command[1] |= ((Display != 0) ? 0x04 : 0x00);

	// Cursor(0:OFF, 1:ON)
	Command[1] |= ((Cursor != 0) ? 0x02 : 0x00);

	// Blink(0:OFF, 1:ON)
	Command[1] |= ((Blink != 0) ? 0x01 : 0x00);

	// マスター送信
	g_tSciI2cInfo.p_data1st = &Command[0];
	g_tSciI2cInfo.cnt1st = 1;
	g_tSciI2cInfo.p_data2nd = &Command[1];
	g_tSciI2cInfo.cnt2nd = 1;
	eSciI2cReturn = master_send(&g_tSciI2cInfo);
	if (eSciI2cReturn != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINT
		printf("master_send Error! [eSciI2cReturn:%d]\n", eSciI2cReturn);
#endif // #ifdef DEBUG_PRINT
		return eSciI2cReturn;
	}

	return eSciI2cReturn;
}


//*******************************************************
// ファンクション設定
//*******************************************************
sci_iic_return_t ACM1602_SetFunction(uint8_t DataLength, uint8_t DisplayLine, uint8_t DisplayFont)
{
	sci_iic_return_t		eSciI2cReturn = SCI_IIC_SUCCESS;
	uint8_t					Command[2] = { 0x00, 0x20 };

	// DataLength(0:4-bit, 1:8-bit)
	Command[1] |= ((DataLength != 0) ? 0x10 : 0x00);

	// DisplayLine(0:1-Line, 1:2-Line)
	Command[1] |= ((DisplayLine != 0) ? 0x08 : 0x00);

	// DisplayFont(0:5x8dots, 1:5x10dots)
	Command[1] |= ((DisplayFont != 0) ? 0x04 : 0x00);

	// マスター送信
	g_tSciI2cInfo.p_data1st = &Command[0];
	g_tSciI2cInfo.cnt1st = 1;
	g_tSciI2cInfo.p_data2nd = &Command[1];
	g_tSciI2cInfo.cnt2nd = 1;
	eSciI2cReturn = master_send(&g_tSciI2cInfo);
	if (eSciI2cReturn != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINT
		printf("master_send Error! [eSciI2cReturn:%d]\n", eSciI2cReturn);
#endif // #ifdef DEBUG_PRINT
		return eSciI2cReturn;
	}

	return eSciI2cReturn;
}


//*******************************************************
// DDRAMアドレス設定
//*******************************************************
sci_iic_return_t ACM1602_SetDdramAddress(uint8_t Addr)
{
	sci_iic_return_t		eSciI2cReturn = SCI_IIC_SUCCESS;
	uint8_t					Command[2] = { 0x00, 0x80 };


	// 引数チェック
	if (!(((Addr >= 0x40) && (Addr <= 0x4F)) || (Addr <= 0x0F)))
	{
		return SCI_IIC_ERR_INVALID_ARG;
	}
	Command[1] |= (0x7F & Addr);

	// マスター送信
	g_tSciI2cInfo.p_data1st = &Command[0];
	g_tSciI2cInfo.cnt1st = 1;
	g_tSciI2cInfo.p_data2nd = &Command[1];
	g_tSciI2cInfo.cnt2nd = 1;
	eSciI2cReturn = master_send(&g_tSciI2cInfo);
	if (eSciI2cReturn != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINT
		printf("master_send Error! [eSciI2cReturn:%d]\n", eSciI2cReturn);
#endif // #ifdef DEBUG_PRINT
		return eSciI2cReturn;
	}

	return eSciI2cReturn;
}


//*******************************************************
// データ書込み
//*******************************************************
sci_iic_return_t ACM1602_WriteDataRam(uint8_t Data)
{
	sci_iic_return_t		eSciI2cReturn = SCI_IIC_SUCCESS;
	uint8_t					Command[2] = { 0x80, 0x00 };


	Command[1] = Data;

	// マスター送信
	g_tSciI2cInfo.p_data1st = &Command[0];
	g_tSciI2cInfo.cnt1st = 1;
	g_tSciI2cInfo.p_data2nd = &Command[1];
	g_tSciI2cInfo.cnt2nd = 1;
	eSciI2cReturn = master_send(&g_tSciI2cInfo);
	if (eSciI2cReturn != SCI_IIC_SUCCESS)
	{
#ifdef DEBUG_PRINT
		printf("master_send Error! [eSciI2cReturn:%d]\n", eSciI2cReturn);
#endif // #ifdef DEBUG_PRINT
		return eSciI2cReturn;
	}

	return eSciI2cReturn;
}


//*******************************************************
// データ書込み
//*******************************************************
sci_iic_return_t ACM1602_WriteData(const uint8_t *pszData, uint8_t DataSize)
{
	sci_iic_return_t		eSciI2cReturn = SCI_IIC_SUCCESS;

	for(uint8_t i = 0 ; i < DataSize ; i++)
	{
		eSciI2cReturn = ACM1602_WriteDataRam(pszData[i]);
		if (eSciI2cReturn != SCI_IIC_SUCCESS)
		{
#ifdef DEBUG_PRINT
			printf("ACM1602_WriteDataRam Error! [eSciI2cReturn:%d, i:%d]\n", eSciI2cReturn, i);
#endif // #ifdef DEBUG_PRINT
			break;
		}
	}

	return eSciI2cReturn;
}



