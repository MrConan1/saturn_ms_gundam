/******************************************************************************/
/* main.c - Main execution file for MS Gundam Anm file Decompressor/Compressor*/
/* =========================================================================  */
/* Decompression Use:  anm.exe InputFname                                     */
/* Compress File Use:                                                         */
/*            anm.exe -c Old_ANM_Filename New_RLE_SECTION New_ANM_Filename    */
/******************************************************************************/
#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

/* Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "anm_decompress.h"
//#include "anm_compress.h"


/* Defines */
#define VER_MAJ    1
#define VER_MIN    0

#define MODE_DECOMPRESS 0
#define MODE_COMPRESS   1





int main(int argc, char** argv){

    static char inFileName[300];
//	static char outFileName[300];
	int args_error;
	int mode = MODE_DECOMPRESS;

    printf("MS Gundam .ANM Decompressor/Compressor v%d.%02d\n", VER_MAJ, VER_MIN);

    /**************************/
    /* Check input parameters */
    /**************************/

    /* Check for valid # of args */
	args_error = 0;
	if (argc == 2){
		args_error = 0;
		mode = MODE_DECOMPRESS;
	}
    else if (argc == 4){
		if(strcmp(argv[1],"-c") != 0)
			args_error = 1;
		mode = MODE_COMPRESS;
	}
	else{
		args_error = 1;
	}

	if(args_error){
        printf("Decomrpession Use: anm.exe ANM_inputName\n");
		printf("Compress File Use: anm.exe -c ANM_Filename New_RLE_SECTION New_ANM_Filename\n");
        return -1;
    }


	/**********************************************/
	/* Compress or Decompress based on Input Args */
	/**********************************************/

	if(mode == MODE_COMPRESS){

		//Handle Input Parameters
//		memset(inFileName, 0, 300);
//		strcpy(inFileName,argv[2]);
//		memset(outFileName, 0, 300);
//		strcpy(outFileName,argv[3]);
//		printf("Compression Mode, Input = %s, Output = %s\n",inFileName, outFileName);

//		if( compressCG(inFileName, outFileName) < 0){	
//			printf("Error creating compressed CG file.\n");
//	  		return -1;
//	  	}
	}
	else
	{
		/*****************/
		/* Decompression */
		/*****************/
		FILE* inputFile;
		char* inputBuffer;

		//Handle Input Parameters
		memset(inFileName, 0, 300);
		strcpy(inFileName,argv[1]);
		printf("Decompression Mode, Input = %s\n",inFileName);

		/* Put the input file straight into memory */
		inputBuffer = (char*)malloc(1024*1024);
		if(inputBuffer == NULL){
			printf("Error allocating memory to read input file.\n");
			return -1;
		}
		inputFile = fopen(inFileName,"rb");
		if(inputFile == NULL){
			printf("Error from reading from input file %s\n",inFileName);
			return -1;
		}
		fread(inputBuffer,1,1024*1024,inputFile);
		fclose(inputFile);

		if(extractANMData(inputBuffer, inFileName) < 0){
			printf("File decompression failed.\n");
			return -1;
		}
		else{
			printf("File decompression successful.\n");
		}
		free(inputBuffer);
	}

	return 0;
}
