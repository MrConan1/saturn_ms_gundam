/******************************************************************************/
/* anm_create.c - .ANM File Creator with RLE Compression                      */
/******************************************************************************/
#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

/* Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "qdbmp.h"
#include "anm_create.h"



/* Function Prototypes */
int createANM(char* cfgFilename, char* outFname);
int rleCompressor(char* inputStream, unsigned int inputSizeBytes,
	char* outputStream, unsigned int* outSizeBytes);
int detectRun(char* inputStream, unsigned int inputLenBytes, 
	          char** ppRunLocation);


/*******************************************************/
/* ANM File Header                                     */
/* ==================================================  */
/*                                                     */
/* Offset      Info                                    */
/* ------      --------------------------------------  */
/* 0x0000      Number of Valid Offsets (0x00000005)    */
/* 0x0004      Palette Offset (0x0000001C)             */
/* 0x0008      8bpp RLE Data Offset                    */
/* 0x000C      ?? Data A Offset                        */
/* 0x0010      ?? Data B Offset                        */
/* 0x0014      ?? Data C Offset                        */
/* Next Follows 32-bit length, data, 32-bit length,    */
/*              data, etc.  The offsets in the hdr     */
/*              point to the start of the data for     */
/*              each region.                           */
/*                                                     */
/* =================================================== */
/* Palette Data                                        */
/*    Palette data is located at palette offset + 4    */
/*    The size of the data region is 0x100 bytes       */
/* =================================================== */
/* RLE 8bpp Data                                       */
/*    The RLE 8bpp Data starts at 8bpp RLE Data Offset */
/*    First long word is the number of valid bytes in  */
/*    the compressed stream.  Remainder bytes not on a */
/*    32-bit boundary are zero filled.                 */
/* =================================================== */
/* Data B, C, D                                        */
/*    Offsets to uncompressed 8bpp data.               */
/*    Preceeded by a 32-bit LW that is the size of the */
/*    data in bytes.  In the one file its always 0x03EC*/
/*******************************************************/

struct anmHeader{
	unsigned int numValidOffsets;
	unsigned int data_N_Offset1; /* First offset value */
};
typedef struct anmHeader anmHeader;


/**************************************************************/
/* 8bpp RLE Encoding                                          */
/* =================                                          */
/*                                                            */
/* RLE Header                                                 */
/* Offset      Size    Info                                   */
/* ------      -----   ------------------------------         */
/* 0x0000      2       Length Flag (0x0 = 2b; 0x8 = 4 bytes)  */
/* 0x0002      2-4     Size of Decompressed Data in Bytes     */
/*                                                            */
/* --------  RLE  Encoding Begins -----------                 */
/* While(DST_SIZE < SIZE_DECMPR_DATA) {                       */
/*    Read Byte, Value = N                                    */
/*      If Negative, Copy -1*N Bytes from Input Stream.       */
/*      If >= 0, Read an additional byte with Value A.        */
/*         Then Copy Run of N+2 Bytes of value A to Output.   */
/* }                                                          */
/**************************************************************/


/*******************************************************/
/* RLE Data for CSHAA11.ANM is 200x224 8bpp x=120, y=0 */
/* Uncmpr Data is 40x25 8bpp (mouth)   x=204,y=83      */
/* Once the data is uncompressed, the first SW is width*/
/* and the second SW is height                         */
/*******************************************************/
int createANM(char* cfgFilename, char* outFname){

	static char filename[300];
	FILE* inFile, *inFile2, *outFile;
	char* outBuffer, *pOutput;
	unsigned int* pHdrData, *pLWData;
	char tmpChar;
	static char line[301];
	static char tempStr[300];
	unsigned int numSections, paletteSize, paletteWord, totalFsize;
	int x;
	int rval = 0;

	/* Open the input file */
	inFile = fopen(cfgFilename,"r");
	if(inFile == NULL){
		printf("Error opening config file %s\n",cfgFilename);
		return -1;
	}

	/* Read Original Filename */
    if(fgets(line, 300, inFile) == NULL){
		printf("Error reading from cfg file\n");
        return -1;
    }

	/* Read the number of Sections, which provides the # of images */
    if(fgets(line, 300, inFile) == NULL){
        printf("Error reading NumSections from cfg file\n");
        return -1;
    }
	sscanf(line,"%s %d",tempStr, &numSections);

	/* Allocate a buffer for output data (4MB, being lazy, wont get this big) */
	outBuffer = (char*)malloc(4*1024*1024);
	if(outBuffer == NULL){
		printf("Cannot allocate memory for output file.\n");
	}
	pOutput = (char*)outBuffer;
	pOutput += 4+(numSections*4); /* Point at start of data */
	pHdrData = (unsigned int*)outBuffer;
	*pHdrData = numSections;
	swap32(pHdrData);
	pHdrData++;
	
	/******************************************************************************/
	/* First Section is the Palette, 32-bit Palette Word followed by Palette Data */
	/******************************************************************************/

	/* Read filename, palette word, palette size from CFG File */
	if(fgets(line, 300, inFile) == NULL){
		return -1;
	}
	sscanf(line, "%s %s",tempStr, filename);
	if(fgets(line, 300, inFile) == NULL){
		return -1;
	}
	sscanf(line, "%s %X",tempStr,&paletteWord);
	if(fgets(line, 300, inFile) == NULL){
		return -1;
	}
	sscanf(line, "%s %X",tempStr,&paletteSize);

	/* Write the size of the palette data + palette word to the output */
	pLWData = (unsigned int*)pOutput;
	*pLWData = 4+paletteSize;
	swap32(pLWData);
	pOutput += 4;

	/* Update the Header Offset for this section      */
	pHdrData[0] = ((unsigned int)pOutput - (unsigned int)outBuffer);
	swap32(pHdrData);

	/* Write palette word to the output */
	pLWData = (unsigned int*)pOutput;
	*pLWData = paletteWord;
	swap32(pLWData);
	pOutput += 4;

	/* Read the Palette Data into the output buffer */
	inFile2 = fopen(filename,"rb");
	if(inFile == NULL){
		printf("Error opening section image %s\n",filename);
		return -1;
	}
	fread(pOutput,1,paletteSize,inFile2);
	fclose(inFile2);
	pOutput += paletteSize;


	/**********************/
	/* Image Section Info */
	/*                    *********************************************************/
	/* For each additional section, place the following to the output buffer:     */
	/* Section Size, Width (16-bits), Height (16-bits), Image Data                */
	/******************************************************************************/
	for(x = 1; x < (int)numSections; x++){

		char* pSectionData;
		unsigned short* pSW;
		unsigned int fsizeBytes, rleFlag, totalSizeBytes;
		unsigned short width, height;

		/* Read filename, rle flag, width, height from CFG File */
		if(fgets(line, 300, inFile) == NULL){
			rval = -1;
			break;
		}
		sscanf(line, "%s %s",tempStr, filename);
		if(fgets(line, 300, inFile) == NULL){
			rval = -1;
			break;
		}
		sscanf(line, "%s %c %d",tempStr, &tmpChar,&rleFlag);
		if(strcmp(tempStr, "RLE") == 0){  /* RLE Line is optional */
			if(fgets(line, 300, inFile) == NULL){
				rval = -1;
				break;
			}
		}
		else{
			rleFlag = 0;
		}
		sscanf(line, "%s %hd %c %hd",tempStr,&width,&tmpChar, &height);

	    /* Open file */
		inFile2 = fopen(filename,"rb");
		if(inFile == NULL){
			printf("Error opening section image %s\n",filename);
			return -1;
		}

		/* Determine Size */
		fseek(inFile2,0,SEEK_END);
		fsizeBytes = ftell(inFile2);
		fseek(inFile2,0,SEEK_SET);

		/* Hold the offset for the size of the image data + height/width */
		pLWData = (unsigned int*)pOutput;
		pOutput += 4;

		/* Update the Header Offset for this section      */
		pHdrData[x] = ((unsigned int)pOutput - (unsigned int)outBuffer);
		swap32(&pHdrData[x]);

		/* Allocate memory, adding 4 bytes for width/height */
		pSectionData = (char*)malloc(fsizeBytes+4);
		if(pSectionData == NULL){
			printf("Error detected allocating memory.\n");
			rval = -1;
			break;
		}

		/* Write Width/Height to Memory */
		pSW = (unsigned short*)&pSectionData[0];
		*pSW = width;
		swap16(pSW);
		pSW = (unsigned short*)&pSectionData[2];
		*pSW = height;
		swap16(pSW);

		/* Read File into Memory */
		fread(&pSectionData[4],1,fsizeBytes,inFile2);
		fclose(inFile2);
		totalSizeBytes = fsizeBytes+4;

	    /* Compress if requested */
		/* If the Image has the RLE compression flag set in the CFG File, compress it */
		/* Then add the section to the output file. Palettes do not compress */
		if(rleFlag){
			unsigned int updatedBytes;
			char* tmpBuf = (char*)malloc(4*1024*1024);
			if(tmpBuf == NULL){
				printf("Error allocating memory for tmp buffer for RLE.\n");
				return -1;
			}
			if(rleCompressor(pSectionData, totalSizeBytes, tmpBuf, &updatedBytes) < 0){
				printf("Compression Error.\n");
				return -1;
			}
			totalSizeBytes = updatedBytes; /* Update */
			memcpy(pSectionData, tmpBuf, updatedBytes);
			free(tmpBuf);
		}

		/* Write the updated size of the image data + height/width to the output */
		*pLWData = totalSizeBytes;
		swap32(pLWData);

	    /* Add to output stream*/
		memcpy(pOutput,pSectionData,totalSizeBytes);
		pOutput += totalSizeBytes;

		free(pSectionData);
	}
	fclose(inFile);
	if(rval < 0)
		return -1;

	/* Write output file to disk */
	outFile = fopen(outFname,"wb");
	if(outFile == NULL){
		printf("Error opening output file for writing");
		return -1;
	}
	totalFsize = (unsigned int)pOutput - (unsigned int)outBuffer;
	fwrite(outBuffer, 1, totalFsize, outFile);
	fclose(outFile);
	free(outBuffer);

	return 0;
}




/******************************************************************/
/* rleCompressor - Performs RLE compression on the input stream   */
/******************************************************************/
int rleCompressor(char* inputStream, unsigned int inputSizeBytes,
	char* outputStream, unsigned int* outSizeBytes){

	int rval;
	int dataRemaining;
	unsigned int outputSize;
	unsigned int* pLW;
	unsigned short* pSW;
    char* pOutput;
	char* pInput = inputStream;
	rval = 0;  /* Init to success */
	outputSize = 0;

	/* Encode Length Flag and Uncompressed Length */
	pSW = (unsigned short*)outputStream;
	if(inputSizeBytes <= (unsigned int)0xFFFF){
		*pSW = 0x0000;
		swap16(pSW);
		pSW++;
		*pSW = (unsigned short)inputSizeBytes;
		swap16(pSW);
		outputSize = 4;
	}
	else{
		*pSW = 0x0008;
		pSW++;
		*pSW = 0x0000;
		pSW++;
		pLW = (unsigned int*)pSW;
		*pLW = (unsigned int)inputSizeBytes;
		swap32(pLW);
		outputSize = 8;
	}
	pOutput = &outputStream[outputSize];

	/********************************************************************/
	/* Compress the RLE Stream                                          */
	/* Runs of Data are of size 2 to 129 bytes                          */
	/* Literal sets of data existing between runs can be 1 to 127 bytes */
	/********************************************************************/
	dataRemaining = inputSizeBytes;
	while(dataRemaining > 0){

		unsigned int distance;
		char* runLocation;
		char* pRunEnd;
		int runSize = 0;
		int runDetected = 0;

		/* Search for a Run of data (2 to 129) */
		rval = detectRun(pInput, dataRemaining, &runLocation);
		if(rval < 0){
			/* No more runs left in the file */
			distance = (unsigned int)dataRemaining - (unsigned int)pInput;
			runDetected = 0;
		}
		else{
			/* Encode all literals up to the run */
			/* Then encode the run */
			distance = (unsigned int)runLocation - (unsigned int)pInput;
			runDetected = 1;
		}
		dataRemaining -= distance;
		
		/* Encode all literals up to the run */
		while(distance > 127){
			*pOutput = (char)(127 * -1);
			pOutput++;
			outputSize++;
			memcpy(pOutput,inputStream,127);
			distance -= 127;
			pInput += 127;
			pOutput += 127;
			outputSize += 127;
		}
		if(distance > 0){
			*pOutput = (char)(distance * -1);
			pOutput++;
			outputSize++;
			memcpy(pOutput,inputStream,distance);
			pInput += distance;
			pOutput += distance;
			outputSize += distance;
		}

		/* Encode the run, make sure not to overrun the amount of data left in the stream */
		/* A run cannot be greater than 129 */
		if(runDetected){
			
			pRunEnd = pInput+1;
			runSize = 2;
			while(1){
				if((unsigned int)(pRunEnd+1) >= ((unsigned int)pInput + dataRemaining))
					break;
				if(runSize >= 129)
					break;

				/* Check to increase the run size */
				if(*pRunEnd == *(pRunEnd+1)){
					runSize++;
					pRunEnd++;
				}
				else
					break;
			}

			/* Encode the run */
			*pOutput = (char)(runSize - 2); /* Run Size */
			pOutput++;
			*pOutput = *pInput; /* Run Value */
			pInput += runSize;
			pOutput++;

			outputSize += 2;
			dataRemaining -= runSize;
		}
		else{
			break;
		}
	}

	*outSizeBytes = outputSize;

	return rval;
}




/***************************************************************************************/
/* Detects a run of 2 identical characters and updates a pointer to that location      */
/* Returns -1 if a run is not deteted by the time the end of the stream is encountered */
/* Returns 0 on successful detection.                                                  */
/***************************************************************************************/
int detectRun(char* inputStream, unsigned int inputLenBytes, char** ppRunLocation){

	char* pInput = inputStream;
	unsigned int remainingSize = inputLenBytes;
	*ppRunLocation = NULL;

	while(remainingSize > 1){
		if(pInput[0] == pInput[1]){
			*ppRunLocation = pInput;
			return 0;
		}
		remainingSize--;
		pInput++;
	}

	return -1;
}
