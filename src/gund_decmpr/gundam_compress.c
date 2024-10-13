/******************************************************************************/
/* gundam_compress.c - .CG Compressor                                         */
/* Usage:  -c GFiles\DEMOT01.CGX newFile.CG                                   */
/******************************************************************************/
#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

/* Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "gundam_compress.h"

/* Defines */
#define DBG_CMPR

/*****************************************************************************/
/* Compressed CG File Format                                                 */
/* ========================================================================= */
/* Byte 0: xxxx_xxxx   Some sort of ID                                       */
/* Byte 1: xxxx_xxx1   Compression enabled if lowest bit in second byte set  */
/* Bytes 2,3:          Number of full compression operations to perform / 16 */
/* [Compression Encoded Data Follows]                                        */
/* Bytes Z,Z+1:        uint16 number of compression operations left          */
/* [Compression Encoded Data Follows]                                        */
/* ========================================================================= */
/*                                                                           */
/* Compression Encoding                                                      */
/* ====================                                                      */
/* uint16 CompressionSelectFlags: Tested from lowest bit 0 to highest bit 15.*/
/*                                For each bit:                              */
/*                                                                           */
/* If Set, Literal.  Copy next 2 bytes in compression stream to output.      */
/*                                                                           */
/* When Clear, Read 1 SW from compression stream & interpret encoding:       */
/*    A.) [Bits 15-12 == 0]                                                  */
/*        *Copy of last byte in decompression stream N times                 */
/*           CPY_LEN = [Bits 11-0] + 3                                       */
/*           Copy last byte in decompression stream 3+CPY_LEN Times.         */
/*    B.) [Bits 15-12 == 1]                                                  */
/*        *Copy of 17 to 272 bytes from sliding window of size 0xFFF         */
/*           Read 1 additional byte (ByteA) from compression stream          */
/*           CPY_LEN = ByteA + 17                                            */
/*           SRC = DST_BUFFER - [Bits 11-0]                                  */
/*           Perform sliding window byte copy from SRC, size = CPY_LEN       */
/*    C.) [Bits 15-12 > 1]                                                   */
/*        *Copy of 3 to 16 bytes from sliding window of size 0xFFF           */
/*           CPY_LEN = [Bits 15-12] + 1                                      */
/*           SRC = DST_BUFFER - [Bits 11-0]                                  */
/*           Perform sliding window byte copy from SRC, size = CPY_LEN       */
/*                                                                           */
/* After all 16 bits of CompressionSelectFlags have been tested, another     */
/* uint16 containing CompressionSelectFlags appears on the input...          */
/*****************************************************************************/



/***********/
/* Globals */
/***********/
#ifdef DBG_DECMPR
static FILE* dbgLog = NULL;
static int G_innerLoop = 1;
#endif

/**********************************************/
/* Dictionary Related Definitions and Globals */
/**********************************************/
#define MAX_SLIDING_WINDOW    0xFFF
#define MAX_SIZE              (1024*1024)

/* Encoding Types */
#define CMPRESSION_TYPE       0
#define LITERAL_TYPE		  1


/* Turn Logging On/Off */
#undef DBG_CMPR

/***********************/
/* Function Prototypes */
/***********************/
int compressCG(char* inputName, char* outputName);
int testCpyPrevious(char* pCGX, int cgx_offset, int fsizeRemaining);
int testSlidingWindowA(char* pCGX, int cgx_offset, int slidingWindowSize, 
	int* windowOffset, int fsizeRemaining);
int testSlidingWindowB(char* pCGX, int cgx_offset, int slidingWindowSize, 
	int* windowOffset, int fsizeRemaining);


/* Globals */
#ifdef DBG_CMPR
static FILE* dbgLog = NULL;
#endif


/*****************************************************************************/
/* compressCG - Compresses a CGX File back into a CG File                    */
/*****************************************************************************/
int compressCG(char* inputName, char* outputName)
{
	FILE* inFile, *outFile;
	char* pCGX, * pCmprStream, *pCmprStream16;
	int fsize, fsizeRemaining, cmprOffset, cmprOffset16, cgxOffset, 
		slidingWindowSize, encodeType, numFullEncodes, numPartialEncodes;
	int cmprBytesA, cmprBytesB, cmprBytesC;
	int windowOffsetA,windowOffsetB;
	unsigned char encWd8;
	unsigned short encWd16;
	unsigned int encodeWdBitOffset;
	unsigned short encodeWord;
	unsigned short* pFullEncodeWd16;

#ifdef DBG_CMPR
    dbgLog = fopen("cmpr_log.txt","w");
	if(dbgLog == NULL){
		printf("Error opening compression log file %s.\n","cmpr_log.txt");
		return -1;
	}
#endif

	/**************************************/
	/* Read decompressed file into memory */
	/**************************************/
	inFile = fopen(inputName,"rb");
	if(inFile == NULL){
		printf("Error opening file %s for reading.\n",inputName);
		return -1;

	}
	pCGX = (char*)malloc(MAX_SIZE);
	cgxOffset = 0;
	fsize = fread(pCGX,1,MAX_SIZE,inFile);
	fclose(inFile);

	/********/
	/* Init */
	/********/
	cmprOffset = cmprOffset16 = 0;
	slidingWindowSize = 0;
	fsizeRemaining = fsize;

	/* Allocate memory to hold up to 16 compression commands    */
	/* Need to do this because of the way the file format works */
	pCmprStream16 = (char*)malloc(MAX_SIZE);
	if(pCmprStream16 == NULL){
		printf("Malloc Error.\n");
		return -1;
	}
	cmprOffset16 = 0;

	/* Allocate memory to hold the entire compression stream */
	pCmprStream = (char*)malloc(MAX_SIZE);
	if(pCmprStream == NULL){
		printf("Malloc Error.\n");
		return -1;
	}
	memset(pCmprStream, 0, MAX_SIZE);

#ifdef DBG_CMPR
		fprintf(dbgLog,"Compression Log\n==========================\n");
#endif

	/* Write short header */
	pCmprStream[0] = 0x10;  /* ID I guess */
	pCmprStream[1] = 0x01;  /* Set Compression Flag */
	pFullEncodeWd16 = (unsigned short*)&pCmprStream[2];  /* Filled in at the end */
	cmprOffset = 4;  /* Start after # of full encodes ushort */ 

	numFullEncodes = 0;
	encodeWord = 0;
	encodeWdBitOffset = 0;
	fsizeRemaining = fsize - cgxOffset;
	while(fsizeRemaining > 0){
		printf("Input offset = 0x%X  Output Size = 0x%X\n",cgxOffset, cmprOffset+cmprOffset16);
		fflush(stdout);

		/* Evaluate all compression modes */
		cmprBytesA = testCpyPrevious(pCGX, cgxOffset, fsizeRemaining);
		cmprBytesB = testSlidingWindowA(pCGX, cgxOffset, slidingWindowSize,&windowOffsetA,fsizeRemaining);
		cmprBytesC = testSlidingWindowB(pCGX, cgxOffset, slidingWindowSize,&windowOffsetB,fsizeRemaining);

		if( (cmprBytesA >= cmprBytesB) && (cmprBytesA >= cmprBytesC) && (cmprBytesA > 0)){
			//Previous Byte Encoding 
			encWd16 = (unsigned short)((cmprBytesA-3) & 0xFFF);
			swap16(&encWd16);
			memcpy(pCmprStream16 + cmprOffset16, &encWd16, 2);
			cmprOffset16 += 2;
			cgxOffset += cmprBytesA;
			encodeType = CMPRESSION_TYPE;

#ifdef DBG_CMPR
			swap16(&encWd16);
			fprintf(dbgLog,"\t\tByte Copy:  0x%04X; (0x%04X) (CpyValue = 0x%02X, CpyLen = %d)\n",encWd16,encWd16 & 0xF000,pCGX[cgxOffset-1]&0xFF,(encWd16&0xFFF)+3);
			fflush(dbgLog);
#endif
		}
		else if( ((cmprBytesB-1) >= cmprBytesA) && ((cmprBytesB-1) >= cmprBytesC) && (cmprBytesB > 0)){
			//Note: Sliding Window A incurs a 1 Word encoding penalty for comparison purposes
			//Sliding Window A Encoding 
			encWd16 = (unsigned short)((0x1000) | (windowOffsetA & 0xFFF));
			swap16(&encWd16);
			memcpy(pCmprStream16 + cmprOffset16, &encWd16, 2);
			cmprOffset16 += 2;
			encWd8 = (unsigned char)(cmprBytesB-17);
			memcpy(pCmprStream16 + cmprOffset16, &encWd8, 1);
			cmprOffset16++;
			cgxOffset += cmprBytesB;
			encodeType = CMPRESSION_TYPE;
#ifdef DBG_CMPR
			swap16(&encWd16);
			fprintf(dbgLog,"\t\tSliding Window A:  0x%04X; (0x%04X) (CpyLen = %d)\n",encWd16,encWd16&0xF000,cmprBytesB);
			fflush(dbgLog);
#endif

		}
		else if( (cmprBytesC >= cmprBytesA) && (cmprBytesC >= cmprBytesB) && (cmprBytesC > 0)){
			//Sliding Window B Encoding
			encWd16 = (unsigned short)(((cmprBytesC-1) << 12) | (windowOffsetB & 0xFFF));
			swap16(&encWd16);
			memcpy(pCmprStream16 + cmprOffset16, &encWd16, 2);
			cmprOffset16 += 2;
			cgxOffset += cmprBytesC;
			encodeType = CMPRESSION_TYPE;
#ifdef DBG_CMPR
			swap16(&encWd16);
			fprintf(dbgLog,"\t\tSliding Window B:  0x%04X; (0x%04X) (CpyLen = %d)\n",encWd16,encWd16&0xF000,(encWd16>>12)+1);
			fflush(dbgLog);
#endif
		}
		else{
			//Literal Encoding (Direct Write of 2 bytes)
			encodeType = LITERAL_TYPE;
			memcpy(pCmprStream16 + cmprOffset16, pCGX + cgxOffset, 2);
			cmprOffset16 += 2;
			cgxOffset += 2;

#ifdef DBG_CMPR
			fprintf(dbgLog,"\t\tLiteral Copy:  0x%02X%02X\n",pCGX[cgxOffset-2] & 0xFF,pCGX[cgxOffset-1] & 0xFF);
			fflush(dbgLog);

			if( ((pCGX[cgxOffset-2] & 0xFF) == 0x80) && ((pCGX[cgxOffset-1] & 0xFF) == 0x00))
				printf("\nHERE\n");
#endif
		}

		/* Update the size of the sliding window, max size is 0xFFF */
		if(slidingWindowSize < 0xFFF){
			slidingWindowSize = cgxOffset; /* Ignore 2 byte header */
			if(slidingWindowSize > 0xFFF)
				slidingWindowSize = 0xFFF;
		}

		/* Update encoding flags  */
		encodeWord |= (encodeType << encodeWdBitOffset);
		encodeWdBitOffset++;

		/* Update the main compression stream      */
		/* Write encoding word on output           */
		/* Reset the 16-command compression stream */
		if(encodeWdBitOffset >= 16){
			encodeWdBitOffset = 0;
			swap16(&encodeWord);
			memcpy(&pCmprStream[cmprOffset], &encodeWord, 2);
#ifdef DBG_CMPR
			swap16(&encodeWord);
			fprintf(dbgLog,"\t\tMaskWD:  0x%04X\n\n",encodeWord & 0xFFFF);
			fflush(dbgLog);
#endif
			cmprOffset += 2;
			memcpy(&pCmprStream[cmprOffset], pCmprStream16, cmprOffset16);
			cmprOffset += cmprOffset16;
			cmprOffset16 = 0;
			encodeWord = 0;
			numFullEncodes++;
		}

		fsizeRemaining = fsize - cgxOffset;
	}

	/* Write out the # of full encodes */
	swap16(&numFullEncodes);
	memcpy(pFullEncodeWd16, &numFullEncodes, 2);

	/* Write out the # of partial encodes */
	numPartialEncodes = encodeWdBitOffset;
	swap16(&numPartialEncodes);
	memcpy(&pCmprStream[cmprOffset], &numPartialEncodes, 2);
	cmprOffset += 2;

#ifdef DBG_CMPR
	swap16(&numPartialEncodes);
	fprintf(dbgLog,"\t\tNum Partial Encodes:  0x%04X\n\n",numPartialEncodes & 0xFFFF);
	fflush(dbgLog);
	swap16(&numPartialEncodes);
#endif

	/* Write out the remainder of encoded data */
	swap16(&numPartialEncodes);
	if(numPartialEncodes > 0){
		swap16(&encodeWord);
		memcpy(&pCmprStream[cmprOffset], &encodeWord, 2);
		cmprOffset += 2;
		memcpy(&pCmprStream[cmprOffset], pCmprStream16, cmprOffset16);
		cmprOffset += cmprOffset16;

#ifdef DBG_CMPR
		swap16(&encodeWord);
		fprintf(dbgLog,"\t\tPartial MaskWD:  0x%04X\n\n",encodeWord & 0xFFFF);
		fflush(dbgLog);
#endif
	}

	/* Release Resources */
	free(pCmprStream16);
	free(pCGX);

	/*********************************/
	/* Write the output file to disk */
	/*********************************/
	outFile = fopen(outputName, "wb");
	if(outFile == NULL){
		printf("Error opening %s for writing.\n", outputName);
		return -1;
	}
	fwrite(pCmprStream, 1, cmprOffset, outFile);
	fclose(outFile);

	/* Release Resources */
	free(pCmprStream);

#ifdef DBG_CMPR
    fclose(dbgLog);
#endif

	printf("Compression Ratio = %.2f",((float)fsize/cmprOffset));

	return 0;
}



/* Copy Previous Byte Encoding Method */
int testCpyPrevious(char* pCGX, int cgx_offset, int fsizeRemaining){

	char value = 0;
	unsigned short maxSize = 0;
	int runLength = 0;

	/* Need to have a previous byte */
	if(cgx_offset == 0){
		return -1;
	}
	else{
		value = *((pCGX+cgx_offset)-1);
		maxSize = 0xFFF+3;
		if(maxSize > fsizeRemaining){
			maxSize = fsizeRemaining;
		}

		/* Need at least 3 bytes to perform this */
		if(maxSize < 3)
			return -1;

		/* Count the run up to max size */
		runLength = 0;
		while(*(pCGX+cgx_offset+runLength) == value){
			runLength++;
			if(runLength == maxSize)
				break;
		}
	}

	/* Need 3 bytes or more */
	if(runLength < 3)
		return -1;

	return runLength;
}




/*    B.) [Bits 15-12 == 1]                                                  */
/*        *Copy of 17 to 272 bytes from sliding window of size 0xFFF         */
/*           Read 1 additional byte (ByteA) from compression stream          */
/*           CPY_LEN = ByteA + 17                                            */
/*           SRC = DST_BUFFER - [Bits 11-0]                                  */
/*           Perform sliding window byte copy from SRC, size = CPY_LEN       */
int testSlidingWindowA(char* pCGX, int cgx_offset, int slidingWindowSize, 
	int* windowOffset, int fsizeRemaining){

	int y,z;
	int storedOffset = 0;
	int maxRunLength = 0;

	/* Need to have at least 17 bytes of sliding window data */
	if(slidingWindowSize < 17){
		return -1;
	}
	else{
		int testRunLength = 0;
		int testOffset = 0;

		for(z = slidingWindowSize; z > 0; z--){

			testRunLength = 0;
			for(y = 0; y < 272; y++){
				testOffset = cgx_offset-z+y;
				if(pCGX[testOffset] == pCGX[cgx_offset + y])
					testRunLength++;
				else
					break;
			}
			if(testRunLength >= maxRunLength){
				maxRunLength = testRunLength;
				storedOffset = z;
			}
		}
	}

	/* EOF Check */
	if(maxRunLength > fsizeRemaining)
		maxRunLength = fsizeRemaining;
	
	/* Need a copy of 17 bytes or more */
	if(maxRunLength < 17)
		return -1;

	*windowOffset = storedOffset;
	return maxRunLength;
}




/*    C.) [Bits 15-12 > 1]                                                   */
/*        *Copy of 1 to 16 bytes from sliding window of size 0xFFF           */
/*           CPY_LEN = [Bits 15-12] + 1                                      */
/*           SRC = DST_BUFFER - [Bits 11-0]                                  */
/*           Perform sliding window byte copy from SRC, size = CPY_LEN       */
int testSlidingWindowB(char* pCGX, int cgx_offset, int slidingWindowSize, 
	int* windowOffset, int fsizeRemaining){

	int y,z;
	int storedOffset = 0;
	int maxRunLength = 0;

	/* Need to have at least 1 bytes of sliding window data */
	if(slidingWindowSize < 1){
		return -1;
	}
	else{
		int testRunLength = 0;
		int testOffset = 0;
		
		for(z = slidingWindowSize; z > 0; z--){
			testRunLength = 0;
			for(y = 0; y < 16; y++){
				testOffset = cgx_offset-z+y;
				if(pCGX[testOffset] == pCGX[cgx_offset+y])
					testRunLength++;
				else
					break;
			}
			if(testRunLength >= maxRunLength){
				maxRunLength = testRunLength;
				storedOffset = z;
			}
		}
	}

	/* EOF Check */
	if(maxRunLength > fsizeRemaining)
		maxRunLength = fsizeRemaining;
	
	/* Need a copy of 3 bytes or more */
	if(maxRunLength < 3)
		return -1;

	*windowOffset = storedOffset;
	return maxRunLength;
}

