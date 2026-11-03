#include "main.h"
uint32_t ProtocolSendPack(STRUCT_FRAMERST *sPtRst,uint8_t *sBuff,uint16_t sTime)
{
	uint32_t sCnt = 0;
	uint32_t sLen = 0;
	sBuff[0]=0x55;
	sBuff[1]=0xAA;
	sCnt=sCnt+2;
	switch (r2_str.numTLV)
	{
		case 1: //µãÔÆ
			sLen = sPtRst->targetNum*9+6;  //6=numTLV(1B)+type(1B)+targetnum(2B)+timeInterval(2B)  9=d1Idx(2B)+d2Idx(1B)+d3Idx(1B)+d4Idx(1B)+powABS(4B)

			#ifdef MPU_OPEN_FLAG
				memcpy(sBuff+sCnt,&sLen,4);
				sCnt=sCnt+4;
			#else
				sBuff[sCnt]=sLen&0xFF;
				sCnt++;
				sBuff[sCnt]=(sLen>>8)&0xFF;
				sCnt++;
				sBuff[sCnt]=(sLen>>16)&0xFF;
				sCnt++;
				sBuff[sCnt]=(sLen>>24)&0xFF;
				sCnt++;
			#endif
			sBuff[sCnt]= sTime&0xFF;
			sBuff[sCnt+1]= (sTime>>8)&0xFF;
			sCnt=sCnt+2; //frame interval time
			sBuff[sCnt] = (uint8_t)r2_str.numTLV;
			sCnt=sCnt+1;
			sBuff[sCnt] = 1; //type=1
			sCnt=sCnt+1;
			#ifdef MPU_OPEN_FLAG
				memcpy(sBuff+sCnt,&sPtRst->targetNum,2);
				sCnt=sCnt+2;
			#else
				sBuff[sCnt]=(sPtRst->targetNum)&0xFF;
				sCnt++;
				sBuff[sCnt]=(sPtRst->targetNum>>8)&0xFF;
				sCnt++;
			#endif
			for(uint32_t i=0;i<sPtRst->targetNum;i++)
			{
				#ifdef MPU_OPEN_FLAG
					memcpy(sBuff+sCnt,&sPtRst->target[i].d1Idx,2);
					sCnt=sCnt+2;
				#else
					sBuff[sCnt]=(sPtRst->target[i].d1Idx)&0xFF;
					sCnt++;
					sBuff[sCnt]=(sPtRst->target[i].d1Idx>>8)&0xFF;
					sCnt++;
				#endif
				sBuff[sCnt]=sPtRst->target[i].d2Idx;
				sCnt++;
				sBuff[sCnt]=sPtRst->target[i].d3Idx;
				sCnt++;
				sBuff[sCnt]=sPtRst->target[i].d4Idx;
				sCnt++;

				#ifdef MPU_OPEN_FLAG
					memcpy(sBuff+sCnt,&sPtRst->target[i].powABS,4);
					sCnt=sCnt+4;
				#else
					sBuff[sCnt]=sPtRst->target[i].powABS&0xFF;
					sCnt++;
					sBuff[sCnt]=(sPtRst->target[i].powABS>>8)&0xFF;
					sCnt++;
					sBuff[sCnt]=(sPtRst->target[i].powABS>>16)&0xFF;
					sCnt++;
					sBuff[sCnt]=(sPtRst->target[i].powABS>>24)&0xFF;
					sCnt++;
				#endif
			}
			sLen = sLen+6; //6=Head(2B)+Len(4B)
	}
	return sLen;
}








