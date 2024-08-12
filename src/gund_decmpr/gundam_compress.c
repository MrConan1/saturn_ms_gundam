#if 0

/******************************************************************************/
/* vly_compress.c - .VLY Compressor                                           */
/******************************************************************************/
#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

/* Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "vly_compress.h"


/***********/
/* Globals */
/***********/

/* Note: a lot of the sizes of these buffers are hard-coded out of laziness */
/*       and the fact that the VLY files in question arent that large       */

/* Bit Array used to select whether to write out a dictionary copy, or encode*/
/* a literal (either direct or encoded) to the output compression stream     */
unsigned char dictVsLiteralBitArray[100*1024]; /* 100kB = 819,200 decisions  */
int dictVsLiteralBitArraySize = 0;
unsigned char dictVsLiteralBitArrayShift = 0x07;

/* Bit Array used to select whether to encode a literal either as a direct   */
/* 16 byte copy, or use spacial redundancy to take advantage of a smaller    */
/* encoding                                                                  */
unsigned char localVsDirectReadArray[100*1024]; /* 100kB = 819,200 decisions */
int localVsDirectBitArraySize = 0;
unsigned char localVsDirectBitArrayShift = 0x07;

/**********************************************/
/* Dictionary Related Definitions and Globals */
/**********************************************/
#define MAX_DICT_INDEX 8192  /* 32kB memory reserved in the Saturn */
                             /* decompressor implementation        */

struct DictEntry{
	unsigned int valid;
	unsigned int relative_address;
};
typedef struct DictEntry DictEntry;

/* Dictionary of data in the decompression buffer */
DictEntry dictionary[MAX_DICT_INDEX];      
int nextDictIndex = 0;

unsigned char dictSelectionBitArray[100*1024]; /* 00 =  MRU Index            */
                                               /* 01 =  Index Used just prior*/
                                               /*       to MRU Index         */
                                               /* 10 =  MRU Index + 1        */
                                               /* 11 =  Read a SW Index from */
                                               /*       dictIndexSWArray     */
int dictSelectionBitArraySize = 0;
int dictSelectionBitArrayShift = 0x03;


/* Array of SW indexes into the dicitonary */
/* Format is Little Endian                 */
unsigned short dictIndexSWArray[32*1024]; 
int dictIndexSWArraySize = 0;

/* Init MRU Entries to an invalid value */
int G_mruEntry0 = -10;
int G_mruEntry1 = -10;


/***************************************************************/
/* Literal and Encoded Literal Related Definitions and Globals */
/***************************************************************/

//Algorithm:  Init the Local 32-Bit word to zero.
unsigned char G_LocalWd32[4] = {0x00, 0x00, 0x00, 0x00};

/* Bit Array used to select whether to replace 4 bytes of data      */
/* That are used to reference 16 pixels that are spacially close to */
/* one another.  If not replaced, the previous value is used.       */
/* Only Examined when a literal encoded copy is to be used          */
unsigned char updateLclWd32BitArray[100*1024];
int updateLclWd32BitArraySize = 0;
unsigned char updateLclWd32BitArrayShift = 0x07;


/* Buffer of Literal Data */
/* Array of Data Containing sets of one of the following:      */
/* 1.) Sequences of 4 byte localized data                      */
/* 2.) 2-bit encoding of the 4 byte localized data             */
/* 3.) 16 bytes of data to copy directly to the output stream  */
unsigned char literalBuffer[1*1024*1024];
int literalBufferSize = 0;


/***********************/
/* Function Prototypes */
/***********************/
int updateVLYFile(char* origVLYFile, char* updatedTiledataFile,
                  char* newVLYFilename);
int compressVLYtiledata(unsigned char* ptr8bppData, int fsize, int width,
	                    int height);
void initDictionary();
int updateDictionary(unsigned int offset);
int tryDictionary(unsigned char* uncmprDataBase, unsigned char* uncmprData,
	              int width);
int tryReadLocalWd32AndLocalMask(unsigned char* uncmprData, int width);
int performDirectLiteralRead(unsigned char* uncmprData, int width);


/*****************************************************************************/
/* VLY Header Information                                                    */
/* ======================                                                    */
/* 0x00-0x03: 4-byte Ascii String "VLZ2"                                     */
/* 0x04-0x13: Zeroes                                                         */
/* 0x14-0x15: Width  (ushort, Little Endian)                                 */
/* 0x16-0x17: Height (ushort, Little Endian)                                 */
/* 0x18-0x19: Palette Size (ushort, Little Endian).                          */
/*               ==0 :  16 colors, 3-bytes each                              */
/*               !=0 : 256 colors, 3-bytes each                              */
/*                                                                           */
/* 32-bit ulong Little Endian Offsets to data sections follow                */
/* ----------------------------------------------------------                */
/* 0x20-0x23: Offset to Palette Data (Always 0x00000040)                     */
/* 0x24-0x27: Offset to Array of Direct Literal Reads (Not Encoded &         */
/*            Encoded Literals.  Bit Arrays select the type of data read.)   */
/* 0x28-0x2B: Offset to Bit Array for Selection of Dictionary or Literal     */
/*            Encoding                                                       */
/* 0x2C-0x2F: Offset to Array of Short Word Dictionary Offsets               */
/* 0x30-0x33: Offset to Bit Array for Selection of Updating the 4 bytes      */
/*            associated with an Encoded Literal                             */
/* 0x34-0x37: Offset to Bit Array for Selection of Literal or Encoded Literal*/
/* 0x38-0x3B: Offset to Array for Dictionary Index Mode                      */
/* 0x3C-0x3F: Offset to some additional data, not sure what it is            */
/*            When this section is not present, this is filled with zeroes   */
/* 0x40:      Start of Palette Data                                          */
/*****************************************************************************/



/*****************************************************************************/
/* updateVLYFile - Takes an existing VLY file and a replacement uncompressed */
/*                 8bpp set of tile data.  The palette is copied out of the  */
/*                 original VLY file, the tile data is compressed, and then  */
/*                 the appropriate headers are added and the new VLY file is */
/*                 created.                                                  */
/*****************************************************************************/
int updateVLYFile(char* origVLYFile, char* updatedTiledataFile, 
                  char* newVLYFilename)
{
	FILE* inFile, *outFile;
	int fsize, width, height;
	char* finalData;
	int finalDataSizeBytes;
	unsigned int paletteOffset, paletteEndAddress, dataAddrOffset, 
                 finalDataOffset;
	unsigned short paletteSize;
	unsigned int* ptrHdrOffset_x20, *ptrHdrOffset;
	unsigned char* ptrDataOffset;
	unsigned short* ptrHdrOffset_x18;
	unsigned char* ptrTilebuf = NULL;

	unsigned char* ptrVlybuf = (unsigned char*)malloc(1024*1024);
	if(ptrVlybuf == NULL){
		printf("Error with memory allocation for ptrVlybuf.\n");
		return -1;
	}
	memset(ptrVlybuf, 0, 1024*1024);

	/***************************/
    /* Open the input VLY file */
    /***************************/
    inFile = NULL;
    inFile = fopen(origVLYFile, "rb");
    if (inFile == NULL){
        printf("Error occurred while opening input file %s for reading\n",
			origVLYFile);
        return -1;
    }

	/******************************************/
	/* Read the entire input file into memory */
	/******************************************/
	fseek(inFile,0,SEEK_END);
	fsize = ftell(inFile);
	fseek(inFile,0,SEEK_SET);
	fread(ptrVlybuf,1,fsize,inFile);
    fclose(inFile);

	/********************************************************************/
	/* Edit the Headers as needed, zero out the data beyond the palette */
	/********************************************************************/
	ptrHdrOffset_x18 = (unsigned short*)(ptrVlybuf + 0x18); //Palette Size
	ptrHdrOffset_x20 = (unsigned int*)(ptrVlybuf + 0x20);   //Palette Offset
	paletteOffset = *ptrHdrOffset_x20;
	paletteSize = *ptrHdrOffset_x18;
	swap16(&paletteSize);
	if(paletteSize == 0){
		paletteSize = 16;
	}
	else{
		paletteSize = 256;
	}
	paletteEndAddress = paletteOffset + (paletteSize*3) - 1;


	/**********************************************************************/
	/* If the Header at 0x3C is non-zero, copy that data and append later */
	/* to the end of the file.  Not sure what this data is yet.           */
	/**********************************************************************/
	finalData = NULL;
	finalDataOffset = *((unsigned int*)(ptrVlybuf+0x3C));
	finalDataSizeBytes = 0;
	if(finalDataOffset != 0 )
	{
		finalDataOffset = *((unsigned int*)(ptrVlybuf+0x3C));
		finalDataSizeBytes = fsize - finalDataOffset;
		finalData = (char*)malloc(1024*1024);
		if(finalData == NULL)
		{
			printf("finalData: Malloc error.\n");
			return -1;
		}
		memcpy(finalData,ptrVlybuf+finalDataOffset,finalDataSizeBytes);
	}

	/*********************************************************************/
	/* Zero Headers and data that will not be used.                      */
	/* Not really necessary since we are going to write over this anyway */
	/*********************************************************************/

	/* Zero out offsets 0x24 through 0x3B since they will change */
	memset((ptrVlybuf+0x24),0x00,6*4);
	/* Zero out data from (paletteEndAddress+1) to end of the orig vly file */
	memset((ptrVlybuf+paletteEndAddress+1),0x00, fsize - paletteEndAddress);



	/*******************************************/
    /* Open the replacement file for tile data */
    /*******************************************/
    inFile = NULL;
    inFile = fopen(updatedTiledataFile, "rb");
    if (inFile == NULL){
        printf("Error occurred while opening input file %s for reading\n", 
			updatedTiledataFile);
        return -1;
    }
	fseek(inFile,0,SEEK_END);
	fsize = ftell(inFile);
	fseek(inFile,0,SEEK_SET);
	ptrTilebuf = (unsigned char*)malloc(fsize);
	if(ptrTilebuf == NULL){
		printf("Error with memory allocation for ptrTilebuf.\n");
		return -1;
	}
	fread(ptrTilebuf,1,fsize,inFile);
    fclose(inFile);

	/* Compress the Tile Data */
	width = (int)*((unsigned short*)(ptrVlybuf + 0x14));
	height = (int)*((unsigned short*)(ptrVlybuf + 0x16));
	compressVLYtiledata(ptrTilebuf, fsize, width, height);

	/*************************************/
	/* Update Headers and Data in Memory */
	/*************************************/


	/* Bit Array for Selection of Dictionary or Literal */
	ptrHdrOffset = (unsigned int*)(ptrVlybuf+0x28);
	dataAddrOffset = (unsigned int)paletteEndAddress+1;
	*ptrHdrOffset = dataAddrOffset;
	ptrDataOffset = ptrVlybuf+dataAddrOffset;
	memcpy(ptrDataOffset,dictVsLiteralBitArray,dictVsLiteralBitArraySize);

	/* Bit Array for Selection of Literal or Encoded Literal */
	ptrHdrOffset = (unsigned int*)(ptrVlybuf+0x34);
	dataAddrOffset += (unsigned int)dictVsLiteralBitArraySize;
	*ptrHdrOffset = dataAddrOffset;
	ptrDataOffset = ptrVlybuf+dataAddrOffset;
	memcpy(ptrDataOffset,localVsDirectReadArray,localVsDirectBitArraySize);

	/* Bit Array for Selection of Updating the 4 bytes associated with */
	/* an Encoded Literal                                              */
	ptrHdrOffset = (unsigned int*)(ptrVlybuf+0x30);
	dataAddrOffset += (unsigned int)localVsDirectBitArraySize;
	*ptrHdrOffset = dataAddrOffset;
	ptrDataOffset = ptrVlybuf+dataAddrOffset;
	memcpy(ptrDataOffset,updateLclWd32BitArray,updateLclWd32BitArraySize);

	/* Array for Dictionary Index Mode */
	ptrHdrOffset = (unsigned int*)(ptrVlybuf+0x38);
	dataAddrOffset += (unsigned int)updateLclWd32BitArraySize;
	*ptrHdrOffset = dataAddrOffset;
	ptrDataOffset = ptrVlybuf+dataAddrOffset;
	memcpy(ptrDataOffset,dictSelectionBitArray,dictSelectionBitArraySize);

	/* Array of Short Word Dictionary Offsets */
	ptrHdrOffset = (unsigned int*)(ptrVlybuf+0x2C);
	dataAddrOffset += (unsigned int)dictSelectionBitArraySize;
	*ptrHdrOffset = dataAddrOffset;
	ptrDataOffset = ptrVlybuf+dataAddrOffset;
	memcpy(ptrDataOffset,dictIndexSWArray,dictIndexSWArraySize*2);

	/* Array of Literal Reads (Mix of Encoded and 16-byte Not Encoded) */
	ptrHdrOffset = (unsigned int*)(ptrVlybuf+0x24);
	dataAddrOffset += (unsigned int)dictIndexSWArraySize*2;
	*ptrHdrOffset = dataAddrOffset;
	ptrDataOffset = ptrVlybuf+dataAddrOffset;
	memcpy(ptrDataOffset,literalBuffer,literalBufferSize);

	/* Copy last set of data (if applicable) */
	if(finalDataSizeBytes != 0){
		ptrHdrOffset = (unsigned int*)(ptrVlybuf+0x3C);
		dataAddrOffset += (unsigned int)literalBufferSize;
		*ptrHdrOffset = dataAddrOffset;
		ptrDataOffset = ptrVlybuf+dataAddrOffset;
		memcpy(ptrDataOffset,finalData,finalDataSizeBytes);
		fsize = dataAddrOffset + finalDataSizeBytes;
	}
	else{
		fsize = dataAddrOffset + literalBufferSize;
	}
	free(ptrTilebuf);

	/**********************************/
	/* Write the new VLY File to disk */
	/**********************************/

	/* Open the output file */
	outFile = fopen(newVLYFilename, "wb");
    if (outFile == NULL){
        printf("Error occurred while opening output %s for writing updated VLY File\n",
			newVLYFilename);
        return -1;
    }
	/* Write the File */
	fwrite(ptrVlybuf,1,fsize,outFile);
	fclose(outFile);
	
	/* Release Resources */
	free(ptrVlybuf);

	printf("%s was Compressed Successfully.\n",newVLYFilename);
	return 0;
}




/*****************************************************************************/
/* compressVLY - Called to create a compression stream for a given           */
/*               uncompressed 8bpp tilemap                                   */
/*****************************************************************************/
int compressVLYtiledata(unsigned char* ptr8bppData, int fsize, int width,
                        int height)
{
	int x, y;
	unsigned int offset, columnOffset; /* Byte offsets into ptr8bppData */

	/********/
	/* Init */
	/********/
	initDictionary();

	/**************************/
	/* Error Conditions Check */
	/**************************/
	if(width > 640){
		printf("Error, width cannot exceed 640 with this algorithm.\n");
		return -1;
	}
	if(fsize < (width*height)){
		printf("Error, not enough data in input file.\n");
		return -1;
	}


	/****************************************************************/
	/* General Compression Overview:                                */
	/*==============================                                */
	/* Read & Analyze 4 wide x 4 tall pixels of 8bpp data at a time */
	/* Move to the right until the width is reached.                */
	/* Then drop down 4 pixels and repeat.  Repeat this process     */
	/* until the entire image has been compressed.                  */
	/****************************************************************/
	offset = 0;
	for(y = 0; y < (height/4); y++){

		columnOffset = offset;
		for(x = 0; x < width/4; x++){

			int literalSelected = 0;
			int readLocal_Pass = 0;

			/* Compression Logic */
			/* Read 4x4 Set of Pixels and use History/Redundancy in Locality */
			/* to achieve compression.                                       */
			/* Best Compression is Copy from History: 1 to 2 bytes reqd      */
			/* Next Best is Local Mask Only: 4 bytes reqd                    */
			/* Next Best is Read Local Byte and use a Local Mask: 5 bytes req*/
			/* Worst Case is reading in 16 new literals                      */
			/* Bit arrays are used to guide the decompressor as to what the  */
			/* next action to be taken is                                    */

			/* Test to see which method yields the best compression */
			if(tryDictionary(ptr8bppData, ptr8bppData+columnOffset, width)){

				/* Update the Bit Array Selection to Indicate */
				/* "Use Dictionary". There are several more   */
				/* steps beyond this now since some methods   */
				/* save more space than others                */
				literalSelected = 0;
			}
			else{

				/* Update the Bit Array Selection to Indicate "Use Literal Cpy" */
				/* There are several more steps beyond this now since some methods save more space than others */
				literalSelected = 1;

				/* Update the Dictionary to include an entry for this address */
				updateDictionary(columnOffset);

				if( tryReadLocalWd32AndLocalMask(ptr8bppData+columnOffset, width) ){
					/* Special encoding of literal used to save space */
					readLocal_Pass = 1;
				}
				else{
					/* No special encoding, just copy data in directly */
					readLocal_Pass = 0;
					performDirectLiteralRead(ptr8bppData+columnOffset, width);
				}

				/* Update the local vs. direct read Selection array & shift for the next time. */
				localVsDirectReadArray[localVsDirectBitArraySize] |= (readLocal_Pass << localVsDirectBitArrayShift);
				if(localVsDirectBitArrayShift == 0){
					localVsDirectBitArrayShift = 0x07;
					localVsDirectBitArraySize++;
				}
				else{
					localVsDirectBitArrayShift--;
				}

			}

			/* Update the Dicitonary vs. Literal Selection array & shift for the next time.       */
			dictVsLiteralBitArray[dictVsLiteralBitArraySize] |= (literalSelected << dictVsLiteralBitArrayShift);
			if(dictVsLiteralBitArrayShift == 0){
				dictVsLiteralBitArrayShift = 0x07;
				dictVsLiteralBitArraySize++;
			}
			else{
				dictVsLiteralBitArrayShift--;
			}

			/* Update Offset to Shift the Start Column by 4 pixels */
			columnOffset += 4;
		}

		/* Update the Offset to go back to the left side */
		/* of the image, and down 4 rows                 */
		offset += (width*4);
	}

	/*************************************/
	/* Update Size of Global Bit Arrays  */
	/* Some will need to be rounded up.  */
	/* Also should be on 2-byte boundary */
	/*************************************/

	/* Dictionary vs. Literal */
	if(dictVsLiteralBitArrayShift != 0x07)
		dictVsLiteralBitArraySize++;
	if((dictVsLiteralBitArraySize % 2) != 0)
		dictVsLiteralBitArraySize++;

	/* Dictionary Index Method */
	if(dictSelectionBitArrayShift != 0x03)
		dictSelectionBitArraySize++;
	if((dictSelectionBitArraySize % 2) != 0)
		dictSelectionBitArraySize++;

	/* Literal Special Encoding vs. Direct Encoding */
	if(localVsDirectBitArrayShift != 0x07)
		localVsDirectBitArraySize++;
	if((localVsDirectBitArraySize % 2) != 0)
		localVsDirectBitArraySize++;

	/* Literal Special Encoding w./new Wd32 vs. Use Previous Wd32 */
	if(updateLclWd32BitArrayShift != 0x07)
		updateLclWd32BitArraySize++;
	if((updateLclWd32BitArraySize % 2) != 0)
		updateLclWd32BitArraySize++;

	return 0;
}




/********************************/
/* Dictionary Related Functions */
/********************************/


/***************************************************************/
/* initDictionary - Called to initialize use of the dictionary */
/***************************************************************/
void initDictionary(){

	/* Auto-built dictionary*/
	memset(dictionary,0,sizeof(DictEntry) * MAX_DICT_INDEX);
	nextDictIndex = 0;

	/* Used for referencing dictionary entries */
	memset(dictIndexSWArray,0,32*1024);
	dictIndexSWArraySize = 0;
	G_mruEntry0 = -10;
	G_mruEntry1 = -10;

	/* Selection */
	memset(dictSelectionBitArray,0,32*1024);
	dictSelectionBitArraySize = 0;

	return;
}


/******************************************************/
/* updateDictionary - Adds an entry to the dictionary */
/******************************************************/
int updateDictionary(unsigned int offset){

	if(nextDictIndex < MAX_DICT_INDEX){
		dictionary[nextDictIndex].valid = 1;
		dictionary[nextDictIndex].relative_address = offset;
		nextDictIndex++;
		return 0;
	}

	return -1;
}


/*********************************************************************************************/
/* tryDicitonary - Attempt to see if the existing dictionary can be used to encode an entry. */
/*                 Returns 1 (TRUE) if encoding was possible.  Otherwise returns 0 (FALSE)   */
/*********************************************************************************************/
int tryDictionary(unsigned char* uncmprDataBase, unsigned char* uncmprData, int width){

	int x;
	int rval = 0;
	unsigned char* addr1, *addr2, *addr3, *addr4;
	unsigned char row0[4];
	unsigned char row1[4];
	unsigned char row2[4];
	unsigned char row3[4];

	/*********************************************************************/
	/* Read the 4 sets of pixel data to be examined.  This is a 4x4 grid */
	/*********************************************************************/
	memcpy(row0,uncmprData+(width*0),4);
	memcpy(row1,uncmprData+(width*1),4);
	memcpy(row2,uncmprData+(width*2),4);
	memcpy(row3,uncmprData+(width*3),4);

	for(x = 0; x < MAX_DICT_INDEX; x++){

		/* Valid Entry check */
		if(!dictionary[x].valid)
			break;

		/* See if the dictionary entry corresponds with a match */
		addr1 = uncmprDataBase + dictionary[x].relative_address + width*0;
		addr2 = uncmprDataBase + dictionary[x].relative_address + width*1;
		addr3 = uncmprDataBase + dictionary[x].relative_address + width*2;
		addr4 = uncmprDataBase + dictionary[x].relative_address + width*3;

		if( (memcmp(addr1,row0,4) == 0) &&
			(memcmp(addr2,row1,4) == 0) &&
			(memcmp(addr3,row2,4) == 0) &&
			(memcmp(addr4,row3,4) == 0) )
		{
			/****************************************************/
			/* Determine & Update Encoding if a Match was found */
			/****************************************************/
			unsigned char newDictBitArrayValue = 0x00;
			int selectedDictIndex = x;
			rval = 1;

			if(selectedDictIndex == G_mruEntry0){
				newDictBitArrayValue = 0x00 << dictSelectionBitArrayShift*2;
			}
			else if(selectedDictIndex == G_mruEntry1){
				newDictBitArrayValue = 0x01 << dictSelectionBitArrayShift*2;
			}
			else if(selectedDictIndex == (G_mruEntry0+1)){
				newDictBitArrayValue = 0x02 << dictSelectionBitArrayShift*2;
			}
			else{
				/* Add SW Index */
				newDictBitArrayValue = 0x03 << dictSelectionBitArrayShift*2;
				dictIndexSWArray[dictIndexSWArraySize] = (unsigned short)selectedDictIndex;
				dictIndexSWArraySize++; /* Size in SW */
			}

			/* Update MRU Entries */
			G_mruEntry1 = G_mruEntry0;        /* Update second oldest most recently used entry    */
			G_mruEntry0 = selectedDictIndex;  /* Update most recently used entry to the new index */

			/* Add 2 bits to Selection Array */
			dictSelectionBitArray[dictSelectionBitArraySize] |= newDictBitArrayValue;

			/* Update Shift Value for Selection Array */
			if(dictSelectionBitArrayShift == 0){
				dictSelectionBitArrayShift = 0x03;
				dictSelectionBitArraySize++;
			}
			else{
				dictSelectionBitArrayShift--;
			}

			break;
		}
	}

	return rval;
}





/*************************************************/
/* Literal and Encoded Literal Related Functions */
/*************************************************/


/*********************************************************************************************/
/* tryReadLocalWd32AndLocalMask - Attempt to see if the existing global with 4 bytes of      */
/*                                spacially redundant data can be used to represent the next */
/*                                16 bytes of data, OR if another 4 byte sequence can be     */
/*                                substituted, OR if neither operation is possible.          */
/* Returns 1 (TRUE) if encoding was possible.  Otherwise returns 0 (FALSE) to indicate that  */
/* the bytes will need to be a direct encode on the compression stream.                      */
/*********************************************************************************************/
int tryReadLocalWd32AndLocalMask(unsigned char* uncmprData, int width)
{
	unsigned char testArray[16];
	unsigned char* pRow;
	int i, x, y, shiftValue, found;
	unsigned char row0[4];
	unsigned char row1[4];
	unsigned char row2[4];
	unsigned char row3[4];
	int numUniqueBytes = 0;

	/****************************************************************************************/
	/* Read the 4 sets of data to be examined and see if they share up to 4 bytes in common */
	/****************************************************************************************/
	memcpy(row0,uncmprData+(width*0),4);
	memcpy(row1,uncmprData+(width*1),4);
	memcpy(row2,uncmprData+(width*2),4);
	memcpy(row3,uncmprData+(width*3),4);
	
	/********************************************************************************/
	/* Check if there are at most 4 bytes in common amoung the 16 bytes in question */
	/********************************************************************************/
	numUniqueBytes = 0;
	for(x = 0; x < 16; x++){
		testArray[x] = 0xFF;
	}
	for(i = 0; i < 4; i++){

		if(i == 0)
			pRow = row0;
		else if(i == 1)
			pRow = row1;
		else if(i == 2)
			pRow = row2;
		else
			pRow = row3;

		for(x = 0; x < 4; x++){
			found = 0;
			for(y = 0; y < 16; y++){
				if(pRow[x] == testArray[y]){
					found = 1;
					break;
				}
			}
			if(!found){
				testArray[numUniqueBytes++] = pRow[x];
			}
		}
	}

	/* This Method will not work */
	if(numUniqueBytes > 4)
		return 0;


	/* If the 4 bytes are common, see if the existing local word contains those bytes    */
	/* If so, produce 4 1-byte masks (every 2 bits used to select the part of the 32-bit */
	/* word to write to the output stream */
	/* If the 4 bytes are common, but the existing local word does not fit, write a new  */
	/* 32-bit word followed by the 4 1-byte masks                                        */
	pRow = NULL;
	for(x = 0; x < numUniqueBytes; x++){
		found = 0;
		for(y = 0; y < 4; y++){
			if(G_LocalWd32[y] == testArray[x]){
				found = 1;
				break;
			}
		}

		/* The existing word will not work */
		if(!found)
			break;
	}

	/* Write a new 32-bit word to the buffer if required */
	shiftValue = 0;
	if(!found){
		memcpy(&G_LocalWd32[0],testArray,4);
		memcpy(&literalBuffer[literalBufferSize],&G_LocalWd32[0],4);
		literalBufferSize+=4;
		shiftValue = 1;
	}

	/* Update the Selection Bit Array for Updating the Local 32-bit Wd */
	updateLclWd32BitArray[updateLclWd32BitArraySize] |= (shiftValue << updateLclWd32BitArrayShift);
	if(updateLclWd32BitArrayShift == 0){
		updateLclWd32BitArrayShift = 7;
		updateLclWd32BitArraySize++;
	}
	else{
		updateLclWd32BitArrayShift--;
	}

	/*********************************************************************/
	/* Now create and write the 4 bytes worth of bit-masks to the buffer */
	/*********************************************************************/
	for(i = 0; i < 4; i++){
		unsigned char maskByte = 0x00;
		int shiftValue = 6;

		if(i == 0)
			pRow = row0;
		else if(i == 1)
			pRow = row1;
		else if(i == 2)
			pRow = row2;
		else
			pRow = row3;

		for(x = 0; x < 4; x++){

			found = 0;
			for(y = 0; y < 4; y++){
				if(pRow[x] == G_LocalWd32[y]){
					found = 1;
					break;
				}
			}

			if(!found){
				printf("Warning:  should not get here.\n");
				return -1;  /* Should not get here */
			}
			else{
				maskByte |= ((unsigned char)y) << shiftValue;
				shiftValue -= 2;
			}
		}

		memcpy(&literalBuffer[literalBufferSize++],&maskByte,1);
	}


	return 1;
}




/*********************************************************************************************/
/* performDirectLiteralRead - Copies 16 bytes of data from the uncompressed stream to a      */
/*                            global buffer with no special encoding.                        */
/*********************************************************************************************/
int performDirectLiteralRead(unsigned char* uncmprData, int width){

	int x;

	/* Read the 4x4 section of tile data straight from the uncompressed buffer */
	/* And write to a Buffer that holds only literals                          */
	/* Total of 16 bytes */
	for(x = 0; x < 4; x++){
		memcpy(&literalBuffer[literalBufferSize],uncmprData+width*x,4);
		literalBufferSize += 4;
	}

	return 0;
}
#endif
