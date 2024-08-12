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
unsigned short shar_calc(unsigned short inputWd, int numArithmeticShifts);
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
	int fsizeBytes, flag, rval, dstSize;
	unsigned int comprSrcAddr, dstAddr, dstBufferSize;

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
	fsizeBytes = fread(cmprInputBuf,1,MAX_FSIZE,cmpInfile);
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
		sprintf(outFileName,"%s_.CGX",cmprFname);
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










/* Performs a shift arithmetic right on the input "numAritemeticShifts number of times*/
unsigned short shar_calc(unsigned short inputWd, int numArithmeticShifts){

	int x;
	unsigned short rval = inputWd;
	unsigned short shiftUpperValue = inputWd & 0x8000;

	for(x = 0; x < numArithmeticShifts; x++){
		rval = rval >> 1;
		rval |= shiftUpperValue;
	}

	return rval;
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
	int rval = 0;

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

//		unsigned int maskAddr;
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
//				unsigned int srcAddress;
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
//					unsigned char uint8_byte0, uint8_byte1;
					unsigned short uint16_cpyLen;
					unsigned int remainingDstBytes;

					//Length to copy
					uint16_cpyLen = tmpWd >> 12;//shar_calc(tmpWd,12); 
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

			uint8_byte0 = (unsigned char)(tmpWd >> 8); //(unsigned char)shar_calc(tmpWd,8); 
			ptrDecmprBuf = (unsigned char*)addrDecmprBuf;
			*ptrDecmprBuf = uint8_byte0;
			addrDecmprBuf++;
//			addrCmprInput++;

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












#if 0



void preExtract(unsigned char* ptrDstbuf, char* outFileName){

	int x;
	unsigned char* tmpPtr;
	FILE* outFile;
	outFile = NULL;
    outFile = fopen(outFileName, "wb");
    if (outFile == NULL){
        printf("Error occurred while opening output %s for writing\n", outFileName);
        return;
    }
	tmpPtr = ptrDstbuf;
	for(x = 0; x < (1024*1024); x++){
		fwrite(tmpPtr,1,1,outFile);
		tmpPtr++;
	}
	fclose(outFile);
}

void extractMode0(unsigned char* ptrDstbuf, char* outFileName, int width, int height){

	int x,y;
	unsigned char* tmpPtr;
	FILE* outFile;
	outFile = NULL;
    outFile = fopen(outFileName, "wb");
    if (outFile == NULL){
        printf("Error occurred while opening output %s for writing\n", outFileName);
        return;
    }
	tmpPtr = ptrDstbuf;
	for(x = 0; x < height; x++){
		for(y = 0; y < (width/4); y++){
			fwrite(tmpPtr,1,4,outFile);
			tmpPtr += 4;
		}
		tmpPtr += 0x280-width;
	}	
	fclose(outFile);
}


/* 16-bit reads, assemble 1 byte */
void extractMode1(unsigned char* ptrDstbuf, char* outFileName, int width, int height){

	int x,y;
	unsigned char* tmpPtr;
	FILE* outFile;
	outFile = NULL;
    outFile = fopen(outFileName, "wb");
    if (outFile == NULL){
        printf("Error occurred while opening output %s for writing\n", outFileName);
        return;
    }
	tmpPtr = ptrDstbuf;
	for(x = 0; x < height; x++){
		for(y = 0; y < (width/2); y++){
			unsigned char newData = ((tmpPtr[0] << 4) & 0xF0) | (tmpPtr[1] & 0x0F); 
			fwrite(&newData,1,1,outFile);
			tmpPtr += 2;
		}
		tmpPtr += 0x280-width;
	}	
	fclose(outFile);
}





//Init these 3 to 0x80
unsigned int FC14_value = 0x80; //0x0603fc14
unsigned int FC18_value = 0x80; //0x0603fc18
unsigned int FC1C_value = 0x80; //0x0603fc1C

//Init these to zero
unsigned int G_fbf0_value = 0;   //0603fbf0_value
unsigned int G_fbf4_value = 0;   //0603fbf4_value
unsigned int G_fbf8_value = 0;   //0603fbf8_value





//FUN_0603f3ee                      //compressed                        //decompressed             //0x280 always?
//FC1C_value is only used in this function
unsigned int decompressVLY(unsigned char* ptrCompressedSrc, unsigned char* ptrDecompressionDst, unsigned int offset_value,
	                       int* validDataPerSection, int* numSections, int* paletteDataSize){

	int x, y, num15bitReads, numBytes, rval;
	unsigned char* ptrSection1_FBFC, *ptrSection2_FC00, *ptrSection3_FC04, *ptrSection4_FC08, *ptrSection5_FC0C, *ptrSection6_FC10;
	unsigned char* ptrScratchSpaceBase_2F3000, *ptrScratchSpace;
	unsigned char* ptrHeaderOffsetFor15bitReads;
	unsigned char* ptrSaved, *ptrSavedInner;
	unsigned short ushort1,ushort2;
	unsigned short ushort_hdr_offset_x18_value;
	unsigned int hdr_offset_x20_value;

	unsigned int* ptrHdrOffset_x20 = (unsigned int*)(ptrCompressedSrc + 0x20);
	unsigned short* ptrHdrOffset_x18 = (unsigned short*)(ptrCompressedSrc + 0x18);
	hdr_offset_x20_value = *ptrHdrOffset_x20;
	ushort_hdr_offset_x18_value = *ptrHdrOffset_x18;
	swap16(&ushort_hdr_offset_x18_value);


	decmprLog = fopen("decmprlog.txt","wb");


	/* Determine how many 3-byte reads for 15-bit palette data to be made */
	if(ushort_hdr_offset_x18_value != 0){
		num15bitReads = 256;  //0x100
	}
	else{
		num15bitReads = 16;  //0x10
	}
	*paletteDataSize = num15bitReads;


	/* Perform reads for 15-bit Palette data */
	ptrHeaderOffsetFor15bitReads = (unsigned char*)((unsigned int)ptrCompressedSrc + hdr_offset_x20_value);
	for(x = 0; x < num15bitReads; x++){

		unsigned short tmpUshort;
		unsigned short tval;
		
		//High 5 bits
        //Read:   00000000 XXXXX000
		//Result: 0XXXXX00 00000000
		tmpUshort = (unsigned short) *(ptrHeaderOffsetFor15bitReads + 0x2);
		tmpUshort &= 0xF8;
		tmpUshort <<= 7;    /* Shift left 7 */
		tval = tmpUshort;

		//Middle 5 bits
		//Read:   00000000 YYYYY000
		//Result: 0XXXXXYY YYY00000
		tmpUshort = (unsigned short) *(ptrHeaderOffsetFor15bitReads + 0x1);
		tmpUshort &= 0xF8;
		tmpUshort <<= 2;    /* Shift left 2 */
		tval = tval | tmpUshort;

		//Low 5 bits
		//Read:   00000000 ZZZZZ000
		//Result: 0XXXXXYY YYYZZZZZ
		tmpUshort = (unsigned short) *(ptrHeaderOffsetFor15bitReads + 0x0);
		tmpUshort &= 0xF8;
		tmpUshort >>= 3;        /* Shift right 3 */
		tval = tval | tmpUshort;

		paletteData[x] = (unsigned short)tval;

		ptrHeaderOffsetFor15bitReads += 3; //Advance 3 bytes in the src
	}

	//FUN_0603fbc4() calls
	//These all add the start address of the compressed data to a 32-bit value located in the header
	//I believe these might all be start regions to different images in the file
	ptrSection1_FBFC = convToHostWithOffset(ptrCompressedSrc, (ptrCompressedSrc + 0x24)); //0603fbfc_value
	ptrSection2_FC00 = convToHostWithOffset(ptrCompressedSrc, (ptrCompressedSrc + 0x28)); //0603fc00_value
	ptrSection3_FC04 = convToHostWithOffset(ptrCompressedSrc, (ptrCompressedSrc + 0x2C)); //0603fc04_value
	ptrSection4_FC08 = convToHostWithOffset(ptrCompressedSrc, (ptrCompressedSrc + 0x30)); //0603fc08_value
	ptrSection5_FC0C = convToHostWithOffset(ptrCompressedSrc, (ptrCompressedSrc + 0x34)); //0603fc0c_value 
	ptrSection6_FC10 = convToHostWithOffset(ptrCompressedSrc, (ptrCompressedSrc + 0x38)); //0603fc10_value

	//Assign 0x80 to the next 32-bit integers in memory - moved elsewhere
//	FC14_value = 0x80; //0x0603fc14
//	FC18_value = 0x80; //0x0603fc18
//	FC1C_value = 0x80; //0x0603fc1C

	//Assign 3 to a uint  - Moved elsewhere
//	G_counter = 0x03;  //0x0603fc20

	//Init these to zero
	G_fbf0_value = 0;   //0603fbf0_value
	G_fbf4_value = 0;   //0603fbf4_value
	G_fbf8_value = 0;   //0603fbf8_value

	//Init
	ptrScratchSpaceBase_2F3000 = (unsigned char*)malloc(1024*1024);  // 0603fc28_value = 0x2F3000; //address (constant value)
	memset(ptrScratchSpaceBase_2F3000,0,1024*1024);
	ptrScratchSpace = ptrScratchSpaceBase_2F3000;					 //0603fc24_value = 0x2F3000; //assigned, dynamic

	//Read 2 Short words, divide both by 4
	ushort1 = *((unsigned short*)(ptrCompressedSrc + 0x14));  //Inner loop counter
	*validDataPerSection = ushort1;  //width
	ushort1 >>= 2;
	ushort2 = *((unsigned short*)(ptrCompressedSrc + 0x16));  //Outer loop counter
	*numSections = ushort2;         //height
	ushort2 >>= 2;

	/*******************************/
	/* Main Execution - Outer loop */
	/*******************************/
//	savedDstAddr = ptrDecompressionDst;
//	savedUshort1 = ushort1;
	for(x = 0; x < ushort2; x++){


		/**************/
		/* Inner Loop */
		/**************/
//		savedDstAddr_Inner = savedDstAddr;
//		savedUshort1_Inner = ushort1;
//		savedUshort2_Inner = ushort1;
		ptrSaved = ptrDecompressionDst;
		for(y = 0; y < ushort1; y++){
			ptrSavedInner = ptrDecompressionDst;

			rval = FUN_0603fbd0(&ptrSection2_FC00, &FC1C_value);
			if(rval){
				fprintf(decmprLog, "Encode Select: Dictionary (0)\n");
				FUN_0603f570(&ptrDecompressionDst, &ptrSection6_FC10, &ptrSection3_FC04, ptrScratchSpaceBase_2F3000, offset_value);
			}
			else{
				fprintf(decmprLog, "Encode Select: Literal (1)\n");
				FUN_0603f658(&ptrDecompressionDst, &ptrScratchSpace, &ptrSection1_FBFC, &ptrSection5_FC0C, &ptrSection4_FC08, offset_value);
			}

			/* Increment by short word or long word */
			/* As far as i can tell theres no way to incr by short word */
			/* But the check for this is still in the code.  Adding here just in case */
			ptrDecompressionDst = ptrSavedInner;
			if(ushortIncrFlg)
				ptrDecompressionDst += 2;
			else
				ptrDecompressionDst += 4;

		}
		ptrDecompressionDst = ptrSaved;
		ptrDecompressionDst += (offset_value*4);//fix
//		printf("0x%06X (dst)\n", ((unsigned int) ptrDecompressionDst - G_DST_BASE) + 0x2A0000);
	}

	/* Calculate # of bytes written to 2F3xxx location? */
	numBytes = (unsigned int)ptrScratchSpace - (unsigned int)ptrScratchSpaceBase_2F3000;
	if(numBytes > 0x8000){
		; //FUN_0606d340(); //Error handling, overflow of data?
	}

	fclose(decmprLog);

	return 0;
}




//                                0x0603fc00       ,  0x0603fc1c)
unsigned int FUN_0603fbd0(unsigned char** ptrSrcData, unsigned int* ptrMask){

	unsigned int mask, originalMask;
	unsigned char dataByte;

	dataByte = *(unsigned char*)(*ptrSrcData);

	originalMask = mask = *ptrMask;
	mask >>= 1;    //Right Shift
	if(mask == 0){
		mask = 0x80;
		*ptrSrcData = *ptrSrcData + 1;  //Increment source address by 1 byte
	}
	*ptrMask = mask; //Always update for the next time

	/* Return Value */
	if((dataByte & originalMask) == 0)
		return 1;

	return 0;
}



//FUN_0603fbc4
//Convert Uint to Host Endian and add to Offset
unsigned char* convToHostWithOffset(unsigned char* offset, unsigned char* addr_LE_Value){

	unsigned int hostEndianUint = *((unsigned int*)addr_LE_Value);

	return (offset + hostEndianUint);
}




//G_counter only used in this function.  Max value of 0x3.  Initial value is 0x03
//ptrScratchSpaceBase_2F3000 only used in this function
//G_fbf4_value only used in this function
//G_fbf8_value only used in this function
//ptrSection3_FC04 only used in this function
//ptrSection6_FC10 only used in this function
unsigned int G_counter = 0x03;  //0603FC20

unsigned int FUN_0603f570(unsigned char** p_dstAddress, 
	                      unsigned char** p_ptrSection6_FC10, 
						  unsigned char** p_ptrSection3_FC04,
	                      unsigned char* ptrScratchSpaceBase_2F3000,
						  unsigned int offsetVal){

	unsigned char srcDataUchar;
	unsigned int srcDataUint, currentCounter, data_offset, functionSelect;
	unsigned int scratchSrcAddress;
	unsigned char* ptrScratchSrc;

	unsigned char* ptr_dstAddress = *p_dstAddress;
	unsigned char* ptrSection3_FC04 = *p_ptrSection3_FC04;
	unsigned char* ptrSection6_FC10 = *p_ptrSection6_FC10;

	/* Read one byte */
	srcDataUchar = *((unsigned char*)ptrSection6_FC10);
	srcDataUint = (unsigned int)srcDataUchar;
	fprintf(decmprLog, "\tDict Byte Read (FC10): 0x%X Cntr=%d\n",srcDataUint,G_counter);

	/* Shift Data Bits based on the iteration this function was called */
	/* This isolates the byte into one of four 2-bit sequences.        */
	if(G_counter == 0){
		ptrSection6_FC10++;         //incr src Address to the next byte
		G_counter = 0x03;           //reset counter
	}
	else{
		currentCounter = G_counter; //currentCounter now equals the counter, FC10
		do{
			currentCounter--;  
			srcDataUint >>= 2; //shift right 2 on the data (up to 3x depending on counter value)
		}while (currentCounter!=0);
		G_counter--;
	}
	
	//Determine next data offset into the scratch buffer to be used
	//This selects an address to have 32-bit or 16-bit words copied from
	functionSelect = srcDataUint & 0x3;
	//G_fbf8_value = G_fbf4_value; //Update fbf8 to hold the last value of fbf4

	if(functionSelect == 0){
		data_offset = G_fbf4_value;
		fprintf(decmprLog, "\tFunction Select: %d (Last_Offset)\n",functionSelect);
	}
	else if(functionSelect == 1){
		data_offset = G_fbf8_value;
		fprintf(decmprLog, "\tFunction Select: %d (Last_Last_Offset)\n",functionSelect);
	}
	else if(functionSelect == 2){
		data_offset = G_fbf4_value;//G_fbf8_value;
		data_offset += 1;
		fprintf(decmprLog, "\tFunction Select: %d (Last_Offset+1)\n",functionSelect);
	}
	else{ // == 3
		//Read a short word from memory
		unsigned char low_byte, high_byte;

		low_byte = *((unsigned char*)ptrSection3_FC04);
		high_byte = *((unsigned char*)(ptrSection3_FC04+1));
		data_offset = (high_byte << 8) | low_byte;
		fprintf(decmprLog, "\tFunction Select: %d  Data LW Offset 0x%X\n",functionSelect,data_offset);
		ptrSection3_FC04 += +2;  //incr stored address by 2
	}
	G_fbf8_value = G_fbf4_value; //Update fbf8 to hold the last value of fbf4

//see 0603f5A4
	//Update fbf4 with the new offset
	G_fbf4_value = data_offset;
//printf("data offset is 0x%X\n",data_offset);	// 7, 1AB 9x, 1ED, 
	data_offset <<= 2;  /* mult offset by 4 (Offset was in LW, now its in bytes) */
	scratchSrcAddress = *((unsigned int*)(ptrScratchSpaceBase_2F3000 + data_offset));
	ptrScratchSrc = (unsigned char*)scratchSrcAddress;

	fprintf(decmprLog, "\tPerform 4 LW Copies from Addr 0x%X\n\n\n",0x2F3000 + data_offset);

	if(ushortIncrFlg){
		unsigned short tmp;
		int i;

		//Do 4 SW Copies from 0x2FExxx scratch space to the destination
		for(i = 0; i < 4; i++){
			tmp = *(unsigned short*)ptrScratchSrc;
			//*ptr_dstAddress = (unsigned short)tmp;
			//swap16(&tmp);
			memcpy(ptr_dstAddress,&tmp,2);
			ptrScratchSrc += offsetVal;
			ptr_dstAddress += offsetVal;
		}

	}
	else{
		unsigned int tmp;
		int i;

		//Do 4 LW Copies from 0x2FExxx scratch space to the destination
		for(i = 0; i < 4; i++){
			tmp = *(unsigned int*)ptrScratchSrc;
			//*ptr_dstAddress = (unsigned int)tmp;
			//swap32(&tmp);
			memcpy(ptr_dstAddress,&tmp,4);
			ptrScratchSrc += offsetVal;
			ptr_dstAddress += offsetVal;
		}
	}

	/* Update all pointers passed by reference */
	*p_dstAddress = ptr_dstAddress;
	*p_ptrSection3_FC04 = ptrSection3_FC04;
	*p_ptrSection6_FC10 = ptrSection6_FC10;

	return 0;
}



// FC14_value is only used in this function
// FC18_value is only used in this function
// G_fbf0_value is only used in this function
unsigned int FUN_0603f658(	unsigned char** ptr_dstAddress, 
							unsigned char** p_ptrScratchSpace,
							unsigned char** p_ptrSection1_FBFC,
							unsigned char** p_ptrSection5_FC0C,
							unsigned char** p_ptrSection4_FC08,
							unsigned int offsetVal){

        int i,j, rval;

        unsigned char* ptrSrc = NULL;
		unsigned char* ptrRLEData = NULL;
        unsigned char* dstAddress = *ptr_dstAddress;
		unsigned char* ptrScratchSpace = *p_ptrScratchSpace;
        unsigned char* ptrSection1_FBFC = *p_ptrSection1_FBFC;
        unsigned char* ptrSection5_FC0C = *p_ptrSection5_FC0C;
		unsigned char* ptrSection4_FC08 = *p_ptrSection4_FC08;
	
		/* The scratch space is an array of address locations in the destination buffer */
		/* Copy pointer to the current dst address to the scratch space */
		/* Move the scratch space to the next empty pointer slot        */
		*((unsigned int*)ptrScratchSpace) = (unsigned int)dstAddress;
		ptrScratchSpace += 4;

        rval = FUN_0603fbd0(&ptrSection5_FC0C, &FC18_value);
        if(rval){

			fprintf(decmprLog, "\tSelect Encode/No Encode:  No Encode, 16 byte Direct Read (0)\n");
            //See 0603f672
            if(ushortIncrFlg){
                //0603f700
                unsigned char uchar, uchar2;
                ptrSrc = ptrSection1_FBFC;
                offsetVal -= 0x2;

                for(i = 0; i < 4; i++){
                    for(j = 0; j < 2; j++){
                        uchar = *ptrSrc;
                        ptrSrc++;
                        uchar &= 0xF;
                        uchar2 = uchar;
                        uchar2 <<= 4;
                        uchar = *ptrSrc;
                        ptrSrc++;
                        uchar &= 0xF;
                        uchar = uchar2 | uchar;

                        *dstAddress = uchar;
                        dstAddress++;
                    }
                    dstAddress += offsetVal;
                }
            }
            else{
                unsigned char uchar;
                ptrSrc = ptrSection1_FBFC;
                offsetVal -= 0x4;

                for(i = 0; i < 4; i++){
                    for(j = 0; j < 4; j++){
                        uchar = *ptrSrc;
                        *dstAddress = uchar;
                        dstAddress++;
                        ptrSrc++;
                    }
                    dstAddress += offsetVal;
                }
            }
            ptrSection1_FBFC = ptrSrc;
            *p_ptrSection1_FBFC = ptrSection1_FBFC; //update src address
			*p_ptrSection5_FC0C = ptrSection5_FC0C;
			*ptr_dstAddress = dstAddress;
			*p_ptrScratchSpace = ptrScratchSpace;
			return 0;
        }
		fprintf(decmprLog, "\tSelect Encode/No Encode:  Encode (1)\n");

        /* Determine whether to update 4 bytes at FBFO */
        ptrSrc = ptrSection1_FBFC; // <-- BAD POINTER
        rval = FUN_0603fbd0(&ptrSection4_FC08, &FC14_value);
        if(!rval){
            //0603f8bc
            unsigned char* ptrTmpUChar;
            ptrTmpUChar = (unsigned char*)&G_fbf0_value;

			fprintf(decmprLog, "\tSelect Read 4-byte Wd:  Yes (1)\n");
            //Read in bytes and store a new 32-bit address at 0603fbf0 through 0603fbf3
            for(i = 0; i < 4; i++){
				unsigned char byte0;
                byte0 = *ptrSrc;
				fprintf(decmprLog, "\t\tRead 4-byte Wd: 0x%X\n",byte0);
                ptrSrc++;
                *ptrTmpUChar  = byte0;
                ptrTmpUChar++;
            }
        }
		else
			fprintf(decmprLog, "\tSelect Read 4-byte Wd:  No (0)\n");

        if(ushortIncrFlg){
            //0603fa3c
            offsetVal -= 0x2;
            ptrRLEData = (unsigned char*)&G_fbf0_value;

            for(i = 0; i < 4; i++){
                unsigned char local_offset[4];
                unsigned char dataOffsets;

                /* Read 1 byte, holds 4 offsets */
                dataOffsets = *ptrSrc;
                ptrSrc++;
                local_offset[0] = (dataOffsets >> 6) & 0x3;
                local_offset[1] = (dataOffsets >> 4) & 0x3;
                local_offset[2] = (dataOffsets >> 2) & 0x3;
                local_offset[3] = dataOffsets & 0x3;

                /* Write out 2 bytes to the destination */
                for(j=0; j < 4; j+=2){
                    unsigned char tvalue1,tvalue2,tvalue;
                    tvalue1 = *(ptrRLEData + local_offset[j]) & 0xF;
                    tvalue1 <<= 4;
                    tvalue2 = *(ptrRLEData + local_offset[j]) & 0xF;
                    tvalue = tvalue1 | tvalue2;

                    *dstAddress = tvalue;
                    dstAddress++;
                }
                dstAddress += offsetVal;
            }
        }
        else{
            //0603f8de
            offsetVal -= 0x4;
            ptrRLEData = (unsigned char*)&G_fbf0_value;

            for(i = 0; i < 4; i++){
                unsigned char local_offset[4];
                unsigned char dataOffsets;

                /* Read 1 byte, holds 4 offsets */
                dataOffsets = *ptrSrc; //
				fprintf(decmprLog, "\tRead Offset Byte: 0x%X\n",dataOffsets);
                ptrSrc++;
                local_offset[0] = (dataOffsets >> 6) & 0x3;
                local_offset[1] = (dataOffsets >> 4) & 0x3;
                local_offset[2] = (dataOffsets >> 2) & 0x3;
                local_offset[3] = dataOffsets & 0x3;

                /* Write out 4 bytes to the destination */
                for(j=0; j < 4; j++){
                    *dstAddress = *(ptrRLEData + local_offset[j]);
                   // printf("0x%06X: Wrote 0x%02X\n", ((unsigned int) dstAddress - G_DST_BASE) + 0x2A0000, *dstAddress);
					dstAddress++;
                }
                dstAddress += offsetVal;
            }
        }
        /* Update ptr to src address */
		ptrSection1_FBFC = ptrSrc;  //fix
        *p_ptrSection1_FBFC = ptrSection1_FBFC;
		*p_ptrSection4_FC08 = ptrSection4_FC08;
		*p_ptrSection5_FC0C = ptrSection5_FC0C;
        *ptr_dstAddress = dstAddress;
		*p_ptrScratchSpace = ptrScratchSpace;
		fprintf(decmprLog, "\n\n");

        return 0;
}








/* Create a Windows Palette File of the Saturn Data for viewing in Crystal Tile 2 */
void createWindowsPalette(char* outFileName, int paletteDataSize, ColorData* imageInfo, int cdataSize){

	int x;
	unsigned char value;
	unsigned short numColors;
	unsigned int fileSize = 0;
	FILE* outFile = NULL;
	char* hdr1Str = "RIFF";
	char* hdr2Str = "PAL data";

	/* Open the output file */
	outFile = fopen(outFileName, "wb");
    if (outFile == NULL){
        printf("Error occurred while opening output %s for writing palette data\n", outFileName);
        return;
    }

	/* Write the Header */

	/* "RIFF" */
	fwrite(hdr1Str,1,4,outFile);

	/* Filesize-8 (uint l.e.) */
	fileSize = 24 + (paletteDataSize*4) - 8;
	fwrite(&fileSize,4,1,outFile);

	/* "PAL data" */
	fwrite(hdr2Str,1,8,outFile);

	/* Filesize-20 (uint l.e.) */
	fileSize = 24 + (paletteDataSize*4) - 20;
	fwrite(&fileSize,4,1,outFile);

	/* 0, 3, # of colors (ushort l.e.) */
	value = 0;
	fwrite(&value,1,1,outFile);
	value = 3;
	fwrite(&value,1,1,outFile);
	numColors = (unsigned short)paletteDataSize;
	fwrite(&numColors,2,1,outFile);

	/* Write out the data */
	/* Saturn format is Blue(5bit) , Green(5bit) , Red(5bit) */
	for(x = 0; x < paletteDataSize; x++){

		unsigned char red, green, blue, alpha;
		double scaleFactor, redScaled, greenScaled, blueScaled;
		red   = (paletteData[x] & 0x001F);
		green = (paletteData[x] >> 5)  & 0x001F;
		blue  = (paletteData[x] >> 10) & 0x001F;

		scaleFactor = 255.0 / 31.0;
		redScaled = scaleFactor * (double)red;
		greenScaled = scaleFactor * (double)green;
		blueScaled = scaleFactor * (double)blue;
		if(redScaled > 255.0)
			redScaled = 255;
		if(greenScaled > 255.0)
			greenScaled = 255;
		if(blueScaled > 255.0)
			blueScaled = 255;

		red = (unsigned char)redScaled;
		green = (unsigned char)greenScaled;
		blue = (unsigned char)blueScaled;

		alpha = 0x00;

		fwrite(&red,1,1,outFile);
		fwrite(&green,1,1,outFile);
		fwrite(&blue,1,1,outFile);
		fwrite(&alpha,1,1,outFile);

		if(cdataSize > x){
			imageInfo[x].red = red;
			imageInfo[x].green = green;
			imageInfo[x].blue = blue;
		}
	}

	fclose(outFile);

	return;
}


#endif
