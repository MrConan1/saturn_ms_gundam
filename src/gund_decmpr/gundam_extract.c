/******************************************************************************/
/* gundam_extract.c - .CGX Extraction Functions                               */
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
int extractCGFiles(char* outFileName, char* decmprBuf, int dstSize);
void createWindowsPalette(char* outFileName, unsigned short* paletteData, 
	int paletteDataSize, ColorData* imageInfo, int cdataSize);
void createBitmap(unsigned int width, unsigned int height, 
	char* saturnTileFile, ColorData* paletteData, char* outputBMPname);





int extractCGFiles(char* outFileNameBase, char* decmprBuf, int dstSize){

	char* pData;
	static char outFileName[300];
	static char bmpName[300];
	unsigned short width, height, paletteOffsetA, paletteOffsetB;
	int x, newFsize, paletteDataSize;
	FILE* outfile = NULL;
	int imageSize = 0;

	paletteDataSize = 256; /* Only using size 256 byte palettes */

	/* Write out the ABGR VDP2 Image */
	width = *((unsigned short*)&decmprBuf[0x34]);
	swap16(&width);
	height = *((unsigned short*)&decmprBuf[0x36]);
	swap16(&height);
	imageSize = width*height*4;
	pData = &decmprBuf[0x11C];
	sprintf(outFileName,"%s_%hdw_%hdh_32-bitABGR_vdp2.bin",outFileNameBase,width, height);
	outfile = fopen(outFileName,"wb");
	if(outfile != NULL){
		fwrite(pData,1,imageSize,outfile); 
		fclose(outfile);
	}
	pData += imageSize;

	/* Write out the 2 palettes */
	newFsize = *((unsigned int*)pData);
	swap32(&newFsize);
	pData += 4;
	paletteOffsetA = *((unsigned short*)pData);  pData+=2;
	paletteOffsetB = *((unsigned short*)pData);  pData+=2;
	swap16(&paletteOffsetA);
	swap16(&paletteOffsetB);
	newFsize -= 4;  /* Remove Palette Offsets */

	if((newFsize) != 512)
		printf("Warning, palette size expected to be 256, got %d\n",newFsize/2);

	/* Palette */
	sprintf(outFileName,"%s_0x%04X_palette.bin",outFileNameBase,paletteOffsetA);
	memcpy(paletteData,pData,newFsize);
	outfile = fopen(outFileName,"wb");
	if(outfile != NULL){
		fwrite(paletteData,1,newFsize,outfile); 
		fclose(outfile);
	}
	sprintf(outFileName,"%s_palette.pal",outFileNameBase);
	createWindowsPalette(outFileName,paletteData,paletteDataSize,imageInfo,1024);
	pData += newFsize;

	/* Write out the 8bpp Tile Data */
	for(x = 0; x < 3; x++){
		newFsize = *((unsigned int*)pData);
		swap32(&newFsize);
		pData += 4;
		width = *((unsigned short*)pData);  pData+=2;
		height = *((unsigned short*)pData);  pData+=2;
		swap16(&width);
		swap16(&height);
		newFsize -= 4;  /* Remove Width/Height */
		
		sprintf(outFileName,"%s_%dw_%dh_8bpp_tileData.bin",outFileNameBase,width, height);
		outfile = fopen(outFileName,"wb");
		if(outfile != NULL){
			fwrite(pData,1,newFsize,outfile); 
			fclose(outfile);
		}
		pData += newFsize;

		sprintf(bmpName,"%s_%dw_%dh.bmp",outFileNameBase,width, height);
		createBitmap(width, height, outFileName, imageInfo, bmpName);
	}

	return 0;
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
