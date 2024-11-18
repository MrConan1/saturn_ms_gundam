/******************************************************************************/
/* anm_decompress.c - .ANM File Decompressor                                  */
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
#include "anm_decompress.h"


//Retain Palette Data in Memory
struct ColorData{
	unsigned char red;
	unsigned char green;
	unsigned char blue;
};
typedef struct ColorData ColorData;

/* Globals */
ColorData imageInfo[1024];

//Stores 15-bits of data pieced together from the input
unsigned short paletteData[256];   



/* Function Prototypes */
int extractANMData(char* pBuffer, char* baseFilename);
int decompressRLEData(char* inputStream, char** outputStream, 
	unsigned int* outSize);
void createWindowsPalette(char* outFileName, unsigned short* paletteData, 
	int paletteDataSize, ColorData* imageInfo, int cdataSize);
void createBitmap(unsigned int width, unsigned int height, 
	char* saturnTileFile, ColorData* paletteData, char* outputBMPname);


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
int extractANMData(char* pBuffer, char* baseFilename){

	FILE* outfile, *outputInfo;
	static char filename[300];
	static char bmpname[300];
	int numOffsets, dataSize,rval,x,rle_Flag;
	unsigned int offset;
	unsigned int* pOffset;
	unsigned short* pSWData;
	unsigned short width, height;
	char* pData;
	int* ptrSize;
	anmHeader* anmHdr = (anmHeader*)pBuffer;
	numOffsets = anmHdr->numValidOffsets;
	swap32(&numOffsets);
	rle_Flag = 0;

	/* Create a file to hold information about the output files */
	sprintf(filename,"%s_info.txt",baseFilename);
	outputInfo = fopen(filename,"wb");
	if(outputInfo == NULL){
		printf("Error opening output info file for writing");
		return -1;
	}
	fprintf(outputInfo,"Original_File: %s\n",baseFilename);
	fprintf(outputInfo,"Num_Sections: %d\n",numOffsets);

	/* Output all images in the file */
	for(x = 0; x < numOffsets; x++){
		
		pOffset = &(anmHdr->data_N_Offset1);
		offset = pOffset[x];
		swap32(&offset);
		pData = (pBuffer + offset);
		ptrSize = (int*)(pBuffer + offset - 4);
		dataSize = *ptrSize;
		swap32(&dataSize);

		/* RLE Data is X = 1 */
		if((x == 1) && (rle_Flag==1)){
			char* outBufRLE = NULL;
			unsigned int outputSize;
			rval = decompressRLEData(pData, &outBufRLE,&outputSize);
			if(rval >= 0){

				/* Get width and height */
				pSWData = (unsigned short*)outBufRLE;
				width = *pSWData;
				height = *(pSWData+1);
				swap16(&width);
				swap16(&height);
				outputSize -= 4;

				/* Just copy the uncompressed data out to a file */
				sprintf(filename,"%s_%d_%dw_%dh.bin",baseFilename,x,width,height);
				outfile = fopen(filename,"wb");
				if(outfile == NULL){
					printf("Error opening output file for writing");
					return -1;
				}
				fwrite((outBufRLE+4),1,outputSize,outfile);
				fclose(outfile);

				/* Create a bitmap */
				sprintf(bmpname,"%s_%d_%dw_%dh.bmp",baseFilename,x,width,height);
				createBitmap((unsigned int)width, (unsigned int)height, 
					filename, imageInfo, bmpname);

				fprintf(outputInfo,"Img_%d: %s\n",x,filename);
				fprintf(outputInfo,"Width_Height: %d x %d\n",width,height);
			}
			if(outBufRLE != NULL)
				free(outBufRLE);
		}
		else{
			/* Just copy the data out to a file */
			if(x==0){
				unsigned int paletteHdr;
				paletteHdr = *((unsigned int*)pData);
				swap32(&paletteHdr);
				sprintf(filename,"%s_.pal",baseFilename);
				createWindowsPalette(filename, (unsigned short*)(pData+4), 0x100, imageInfo, 1024);
				sprintf(filename,"%s_pal.bin",baseFilename);
				outfile = fopen(filename,"wb");
				if(outfile == NULL){
					printf("Error opening output file for writing");
					return -1;
				}
				fwrite((pData+4),1,dataSize-4,outfile);
				fclose(outfile);

				/* First image is RLE compressed when the palette section is 256 bytes I think */
				if((dataSize-4) <= 0x100){
					rle_Flag = 1;
				}

				fprintf(outputInfo,"Palette: %s\n",filename);
				fprintf(outputInfo,"Palette_HDR_WD: 0x%X\n",paletteHdr);
				fprintf(outputInfo,"Palette_Size: 0x%X\n",dataSize-4);
			}
			else{

				/* Get width and height */
				pSWData = (unsigned short*)pData;
				width = *pSWData;
				height = *(pSWData+1);
				swap16(&width);
				swap16(&height);
				pData += 4;
				dataSize -= 4;
				sprintf(filename,"%s_%d_%dw_%dh.bin",baseFilename,x,width,height);

				/* Output binary image data to a file */
				outfile = fopen(filename,"wb");
				if(outfile == NULL){
					printf("Error opening output file for writing");
					return -1;
				}
				fwrite(pData,1,dataSize,outfile);
				fclose(outfile);

				/* Create a bitmap */
				sprintf(bmpname,"%s_%d_%dw_%dh.bmp",baseFilename,x,width,height);
				createBitmap((unsigned int)width, (unsigned int)height, 
					filename, imageInfo, bmpname);

				fprintf(outputInfo,"Img_%d: %s\n",x,filename);
				fprintf(outputInfo,"Width_Height: %d x %d\n",width,height);
			}
		}
	}
	fclose(outputInfo);
	return 0;
}


int decompressRLEData(char* inputStream, char** outputStream, 
	unsigned int* outSize){

	int rval;
	int dataRemaining;
	unsigned short lengthFlag;
	unsigned int outputSize;
	unsigned int* pLW;
	unsigned short* pSW;
    char* pOutput;
	char* pInput = inputStream;
	rval = 0;  /* Init to success */
	outputSize = 0;

	/* Read Length Flag */
	pSW = (unsigned short*)pInput;
	pInput += 2;
	lengthFlag = (*pSW);
	swap16(&lengthFlag);
	lengthFlag &= 0x8;

	/* Read length of decompressed data */
	*outSize = 0;
	if(lengthFlag == 0){
		unsigned short outputSizeSW;
		pSW = (unsigned short*)pInput;
		outputSizeSW = (unsigned short)(*pSW);
		swap16(&outputSizeSW);
		outputSize = (unsigned int)outputSizeSW;
		pInput += 2;
	}
	else{
		pInput += 2;
		pLW = (unsigned int*)pInput;
		outputSize = *pLW;
		swap32(&outputSize);
		pInput += 4;
	}

	/* Allocate memory to hold output data */
	pOutput = (char*)malloc(outputSize);
	if(pOutput == NULL){
		printf("Error allocing memory for RLE output data.\n");
		return -1;
	}
	*outputStream = pOutput;

	/* Decompress the RLE Stream */
	dataRemaining = outputSize;
	while(dataRemaining > 0){

		char byteCmd, runByte;
		unsigned int numBytesToCpy = 0;
		byteCmd = *pInput;
		pInput++;

		if(byteCmd >= 0){
			/* Copy a Run of data (2 to 257) */
			numBytesToCpy = byteCmd + 2;
			runByte = *pInput;
			pInput++;
			if((dataRemaining - numBytesToCpy) < 0){
				numBytesToCpy = dataRemaining;
				printf("Error in Run Copy, output buffer size too small, reducing data.\n");
				rval = -1;
			}
			memset(pOutput,runByte,numBytesToCpy);
			pOutput += numBytesToCpy;
		}
		else{
			/* Copy Bytes from the Input Stream */
			numBytesToCpy = byteCmd * -1;
			if((dataRemaining - numBytesToCpy) < 0){
				numBytesToCpy = dataRemaining;
				printf("Error in Direct Copy, output buffer size too small, reducing data.\n");
				rval = -1;
			}
			memcpy(pOutput,pInput,numBytesToCpy);
			pInput += numBytesToCpy;
			pOutput += numBytesToCpy;
		}
		dataRemaining -= numBytesToCpy;
	}
	*outSize = outputSize;
	return rval;
}




/* Create a Windows Palette File of the Saturn Data for viewing in Crystal Tile 2 */
void createWindowsPalette(char* outFileName, unsigned short* paletteData, 
	int paletteDataSize, ColorData* imageInfo, int cdataSize){

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
		swap16(&paletteData[x]);
		red   = (paletteData[x] & 0x001F);
		green = (paletteData[x] >> 5)  & 0x001F;
		blue  = (paletteData[x] >> 10) & 0x001F;
		swap16(&paletteData[x]);

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




void createBitmap(unsigned int width, unsigned int height, char* saturnTileFile, ColorData* paletteData, char* outputBMPname){


	FILE* inFile;
	BMP* ptrBmp;
	unsigned char* ptrTilebuf;
	int fsize, x,y,z;
	unsigned int tileIndex;

	/* Open the Saturn Tile file in 8bpp */

	/***********************/
    /* Open the input file */
    /***********************/
    inFile = NULL;
    inFile = fopen(saturnTileFile, "rb");
    if (inFile == NULL){
        printf("Error occurred while opening input file %s for reading\n", saturnTileFile);
        return;
    }

	/******************************************/
	/* Read the entire input file into memory */
	/******************************************/
	fseek(inFile,0,SEEK_END);
	fsize = ftell(inFile);
	fseek(inFile,0,SEEK_SET);
	ptrTilebuf = (unsigned char*)malloc(fsize);
	if(ptrTilebuf == NULL){
		printf("Malloc error, ptrTilebuf.\n");
		return;
	}
	fread(ptrTilebuf,1,fsize,inFile);
    fclose(inFile);

	/* Create a Bitmap of the Image Data */
	ptrBmp = BMP_Create(width, height, 24);

	z = 0;
	for(y = 0; y < (int)height; y++){
		for(x = 0; x < (int)width; x++){
			tileIndex = (unsigned int)ptrTilebuf[z++];

			BMP_SetPixelRGB(ptrBmp, x, y, paletteData[tileIndex].red, paletteData[tileIndex].green, paletteData[tileIndex].blue);
		}
	}
	BMP_WriteFile(ptrBmp, outputBMPname);
	BMP_Free(ptrBmp);
	
	free(ptrTilebuf);

	return;
}
