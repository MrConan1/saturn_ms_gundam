/******************************************************************************/
/* gundam_decompress.c - .CG Decompressor                                     */
/******************************************************************************/
#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

/* Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "gundam_decompress.h"
#include "gundam_extract.h"

/* Globals */

#define MAX_FSIZE (1024*1024)  /* 1MB */


/* Function Prototypes */
int analyzeCGHeader(char* cmprFname);
int performCGDecompression(int cmprFlag, unsigned int comprSrcAddr, 
	unsigned int dstAddr, unsigned int dstBufferSize, unsigned int* dstSize);
int runInnerDecmpression(unsigned int* p_addrCmprInput, 
	unsigned int* p_addrDecmprBuf, unsigned int addrDecmprBufEnd, int loopCtr);


//R4 = 00000001
//R5 = 060DA800    <-- SRC Address for compressed data.  060DA820 is the start of DEMOT01.CG, No clue where the 0x20 header comes from
//R6 = 06087F94    *06087F94 = 06088000 <-- DST Address for decompressed data
//R7 - 00052800

int analyzeCGHeader(char* cmprFname){

    FILE* cmpInfile = NULL;
	FILE* outfile = NULL;
	char* cmprInputBuf = NULL;
	char* decmprBuf = NULL;
	int flag, rval;
	unsigned int comprSrcAddr, dstAddr, dstBufferSize, dstSize;

	/* Open Input File */
	cmpInfile = fopen(cmprFname,"rb");
	if(cmpInfile == NULL){
		printf("Error opening compressed input file %s.\n",cmprFname);
		return -1;
	}

	/* Read Compressed File into Memory */
	cmprInputBuf = (char*)malloc(MAX_FSIZE);
	if(cmprInputBuf == NULL){
		printf("Malloc error on cmprInputBuf.\n");
		fclose(cmpInfile);
		return -1;
	}
	fread(cmprInputBuf,1,MAX_FSIZE,cmpInfile);
	fclose(cmpInfile);

	/* Create a Buffer for Decompression Data */
	decmprBuf = (char*)malloc(MAX_FSIZE);
	if(decmprBuf == NULL){
		printf("Malloc error on decmprBuf.\n");
		free(cmprInputBuf);
		return -1;
	}

	/* Read the decompression selection flag */
	flag = cmprInputBuf[1] & 0x1;

	/* Set up input parameters */
	comprSrcAddr = (unsigned int)&cmprInputBuf[2];
	dstAddr = (unsigned int)decmprBuf;
	dstBufferSize = MAX_FSIZE;
	dstSize = 0;
		
	/* Perform the decompression */
	rval = performCGDecompression(flag, comprSrcAddr, dstAddr, dstBufferSize, &dstSize);
	free(cmprInputBuf);

	/******************************************************************/
	/* Output Decompressed Data to a file if decompression successful */
	/* Also extract files from the data.                              */
	/******************************************************************/
	if(rval == 0){
		static char outFileName[300];
		
		/* Create Decompressed File */
		sprintf(outFileName,"%s.CGX",cmprFname);
		outfile = fopen(outFileName,"wb");
		fwrite((unsigned char*)dstAddr,dstSize,1,outfile);
		fclose(outfile);
		printf("Output file %s created.\n",outFileName);

		/* Extract files within decompressed file  */
		/* Only DEMO* files supported at this time */
		if(strncmp(cmprFname,"DEMO",4) == 0){
			extractCGFiles(outFileName, decmprBuf,dstSize);
		}
	}

	free(decmprBuf);
	return rval;
}





//R4 = 00000001
//R5 = 060DA800    <-- SRC Address for compressed data.  060DA820 is the start of DEMOT01.CG, No clue where the 0x20 header comes from
//R6 = 06087F94    *06087F94 = 06088000 <-- DST Address for decompressed data
//R7 - 00052800

int performCGDecompression(int cmprFlag, unsigned int comprSrcAddr, unsigned int dstAddr, unsigned int dstBufferSize, unsigned int* dstSize){

	int x, rval;
	unsigned short tmpWd;
	int numLoops, remainder;
	unsigned int addrDecmprBufStart, addrDecmprBufEnd;
	addrDecmprBufStart = dstAddr;
	addrDecmprBufEnd = dstAddr + dstBufferSize - 1;

	/* Handle Data differently based on compression flag */
	if(cmprFlag == 0){
		printf("Unsupported compression mode.\n");
		return -1;
	}
	else{

		/* Read 1 Short Word - # of times to do full 16-loop compression calls */
		tmpWd = *((unsigned short*)comprSrcAddr);
		swap16(&tmpWd);
		comprSrcAddr += 2;
		numLoops = (int)tmpWd;

		for(x = 0; x < numLoops; x++){
			rval = runInnerDecmpression(&comprSrcAddr, &dstAddr, addrDecmprBufEnd, 0x10);
			if(rval != 0){
				printf("Decompression error detected.\n");
				return -1;
			}
		}

		/* Read 1 Short Word - # of times to do remainder-loop compression calls */
		tmpWd = *((unsigned short*)comprSrcAddr);
		swap16(&tmpWd);
		comprSrcAddr += 2;
		remainder = (int)tmpWd;

		rval = runInnerDecmpression(&comprSrcAddr, &dstAddr, addrDecmprBufEnd, remainder);
		if(rval != 0){
			printf("Decompression error detected.\n");
			return -1;
		}
	}

	/* Update size of destination buffer on success */
	/* dstAddr is one greater than the used space, so dont add 1 */
	*dstSize = dstAddr - addrDecmprBufStart;

	return 0;
}




void sliding_window_cpy(unsigned char* ptrDst, unsigned char* ptrSrc, unsigned short cpyLen){

	int x;
	for(x = 0; x < (int)cpyLen; x++){
		*ptrDst = *ptrSrc;
		ptrDst++;
		ptrSrc++;
	}

	return;
}


//Array of 16 ushort values
unsigned short usArray_060675a8[16] = {	0x0001,0x0002,0x0004,0x0008,
										0x0010,0x0020,0x0040,0x0080,
										0x0100,0x0200,0x0400,0x0800,
										0x1000,0x2000,0x4000,0x8000};


//FUN_06012e70
int runInnerDecmpression(unsigned int* p_addrCmprInput, unsigned int* p_addrDecmprBuf, unsigned int addrDecmprBufEnd, int loopCtr){


	unsigned char* ptrCmprInput;
	unsigned char* ptrDecmprBuf;
	int x;
	unsigned short initialWd,tmpWd;
	unsigned int addrCmprInput = *p_addrCmprInput;
	unsigned int addrDecmprBuf = *p_addrDecmprBuf;

	/* Read 2 bytes from cmpr data input stream */
	ptrCmprInput = (unsigned char*)addrCmprInput;
	memcpy(&initialWd,ptrCmprInput,2);
	addrCmprInput += 2;
	swap16(&initialWd);

	/* Loop Counter needs to be > 0 */
	if(loopCtr <= 0){
		printf("Error, loop Counter <= 0\n");
		return 0;
	}

	for(x = 0; x < loopCtr; x++){

		unsigned short maskValue, testValue;

		/* Verify that Decompression Buffer will not be blown */
		if(addrDecmprBuf >= addrDecmprBufEnd){
			printf("Error, out of space for decompression to continue.\n");
			return -1;
		}

		/* Read 2 bytes from cmpr data input stream */
		ptrCmprInput = (unsigned char*)addrCmprInput;
		memcpy(&tmpWd,ptrCmprInput,2);
		addrCmprInput += 2;
		swap16(&tmpWd);
//printf("TMP_WD = 0x%X\n",tmpWd & 0xFFFF);
		//cnt *= 2; maskValue = *(0x060675A8 + cnt);
		maskValue = usArray_060675a8[x];
		testValue = initialWd & maskValue;

		/* When testValue is 0, its an encoded cpy, otherwise its a literal copy */
		if(testValue == 0){

			unsigned short testValue2;

			//LAB_06012f14
			testValue2 = tmpWd & 0xF000;

			if(testValue2 == 0x0){
				//06012f1e - memset call
				unsigned char uint8_Value;
				unsigned short uint16_cpyLen;
				unsigned int remainingDstBytes;

				/* Value to copy is last value in the decompression buffer */
				ptrDecmprBuf = (unsigned char*)(addrDecmprBuf-1);
				uint8_Value = *ptrDecmprBuf;

				/* Assign Copy Length */
				uint16_cpyLen = tmpWd + 3;

				/* Error checking (not enough buffer to copy) */
				remainingDstBytes = (addrDecmprBufEnd - addrDecmprBuf) + 1;
				if(uint16_cpyLen > remainingDstBytes)
					uint16_cpyLen = remainingDstBytes;

				/* Perform the Copy */
				ptrDecmprBuf = (unsigned char*)(addrDecmprBuf);
				memset(ptrDecmprBuf,uint8_Value,uint16_cpyLen);
				addrDecmprBuf += uint16_cpyLen;
			}
			else{
				//06012f60
				if(testValue2 == 0x1000){
					//06012f6c - Sliding Window Copy (Type 1)
					unsigned int srcAddress;
					unsigned char uint8_tmpIn;
					unsigned short uint16_cpyLen;
					unsigned int remainingDstBytes;

					//Read 1 byte from input stream for copy length
					ptrCmprInput = (unsigned char*)addrCmprInput;
					uint8_tmpIn = *ptrCmprInput;
					addrCmprInput += 1;
					uint16_cpyLen = (unsigned short)uint8_tmpIn + 0x11;

					//Error checking (not enough buffer to copy)
					remainingDstBytes = (addrDecmprBufEnd - addrDecmprBuf) + 1;
					if(uint16_cpyLen > remainingDstBytes)
						uint16_cpyLen = remainingDstBytes;

					//calc srcAddress 
					srcAddress = addrDecmprBuf - (tmpWd & 0x0FFF);

					//Perform copy from decompression buffer
					ptrDecmprBuf = (unsigned char*)addrDecmprBuf;
					sliding_window_cpy(ptrDecmprBuf, (unsigned char*)srcAddress,uint16_cpyLen);  //r4=0608811E, r5=0608811A, r6 = 110
					addrDecmprBuf += uint16_cpyLen;
				}
				else{
					//06012fa8 - Sliding Window Copy (Type 2)
					unsigned int srcAddress;
					unsigned short uint16_cpyLen;
					unsigned int remainingDstBytes;

					//Length to copy
					uint16_cpyLen = tmpWd >> 12; 
					uint16_cpyLen++;

					//Error checking (not enough buffer to copy)
					remainingDstBytes = (addrDecmprBufEnd - addrDecmprBuf) + 1;
					if(uint16_cpyLen > remainingDstBytes)
						uint16_cpyLen = remainingDstBytes;

					//calc srcAddress and copy length
					uint16_cpyLen &= 0x0FFF;
					srcAddress = addrDecmprBuf - (tmpWd & 0x0FFF);

					ptrDecmprBuf = (unsigned char*)addrDecmprBuf;
					sliding_window_cpy(ptrDecmprBuf, (unsigned char*)srcAddress,uint16_cpyLen);
					addrDecmprBuf += uint16_cpyLen;
				}
			}

		}
		else{
			/* Write 2 bytes from the input stream */
			unsigned char uint8_byte0, uint8_byte1;

			uint8_byte0 = (unsigned char)(tmpWd >> 8); 
			ptrDecmprBuf = (unsigned char*)addrDecmprBuf;
			*ptrDecmprBuf = uint8_byte0;
			addrDecmprBuf++;

			if(addrDecmprBuf >= addrDecmprBufEnd){
				printf("Error, out of space for decompression to continue.\n");
				return 1;
			}

			//LAB_06012f00
			uint8_byte1 = tmpWd & 0xFF;
			ptrDecmprBuf = (unsigned char*)addrDecmprBuf;
			*ptrDecmprBuf = uint8_byte1;
			addrDecmprBuf++;
		}


		//LAB_06012fe6
		// End of for loop check

	}

	//Success
	*p_addrCmprInput = addrCmprInput;
	*p_addrDecmprBuf = addrDecmprBuf;
	return 0;
}
