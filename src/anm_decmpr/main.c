/******************************************************************************/
/* main.c - Main execution file for MS Gundam Anm file Extractor/Creator      */
/* =========================================================================  */
/* Extraction Use:  anm.exe InputFname                                        */
/* Create File Use:  anm.exe -c ANM_Cfg_File.txt New_ANM_Filename             */
/*                                                                            */
/* Some ANM files use RLE to compress the first large image in the file.      */
/******************************************************************************/
#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

/* Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "anm_decompress.h"
#include "anm_create.h"


/* Defines */
#define VER_MAJ    1
#define VER_MIN    0

#define MODE_EXTRACT  0
#define MODE_CREATE   1





int main(int argc, char** argv){

    static char inFileName[300];
	static char outFileName[300];
	int args_error;
	int mode = MODE_EXTRACT;
	int staff_flg = 0;

    printf("MS Gundam .ANM Extractor/Creator v%d.%02d\n", VER_MAJ, VER_MIN);

    /**************************/
    /* Check input parameters */
    /**************************/

    /* Check for valid # of args */
	staff_flg = 0;
	args_error = 0;
	if (argc == 2){
		args_error = 0;
		mode = MODE_EXTRACT;
	}
	else if (argc == 3){
		if(strcmp(argv[1],"-s") != 0)
			args_error = 1;
		mode = MODE_EXTRACT;
		staff_flg = 1;
	}
    else if (argc == 4){
		if(strcmp(argv[1],"-c") != 0)
			args_error = 1;
		mode = MODE_CREATE;
	}
	else{
		args_error = 1;
	}

	if(args_error){
        printf("Extraction Use: anm.exe ANM_inputName\n");
		printf("Extraction Use (Staff File): anm.exe -s ANM_inputName\n");
		printf("Create File Use: anm.exe -c ANM_Cfg_File.txt New_ANM_Filename\n");
        return -1;
    }


	/**********************************************/
	/* Compress or Decompress based on Input Args */
	/**********************************************/

	if(mode == MODE_CREATE){

		//Handle Input Parameters
		memset(inFileName, 0, 300);
		strcpy(inFileName,argv[2]);
		memset(outFileName, 0, 300);
		strcpy(outFileName,argv[3]);
		printf("Create File Mode, Cfg_Input = %s, Output = %s\n",inFileName, outFileName);

		if( createANM(inFileName, outFileName) < 0){	
			printf("Error creating ANM file.\n");
	  		return -1;
	  	}
		printf("File creation successful.\n");
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
		if(staff_flg){
			strcpy(inFileName,argv[2]);
		}
		else
			strcpy(inFileName,argv[1]);
		printf("Extraction Mode, Input = %s\n",inFileName);

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

		if(staff_flg == 1){
			extractANMData_staff(inputBuffer, inFileName);
			if(extractANMData_staff(inputBuffer, inFileName) < 0){
				printf("File extraction failed.\n");
				return -1;
			}
		}
		else if(extractANMData(inputBuffer, inFileName) < 0){
			printf("File extraction failed.\n");
			return -1;
		}
		
		printf("File extraction successful.\n");
		free(inputBuffer);
	}

	return 0;
}
