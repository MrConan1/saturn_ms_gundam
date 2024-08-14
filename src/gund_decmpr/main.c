/******************************************************************************/
/* main.c - Main execution file for MS Gundam Decompressor/Compressor         */
/* =========================================================================  */
/* Decompression Use:  gund_decmpr.exe InputFname                             */
/* Compress File Use: gund_cg.exe -c CGX_Filename New_CG_Filename             */
/******************************************************************************/
#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

/* Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gundam_decompress.h"
#include "gundam_compress.h"


/* Defines */
#define VER_MAJ    2
#define VER_MIN    0

#define MODE_DECOMPRESS 0
#define MODE_COMPRESS   1





int main(int argc, char** argv){

    static char inFileName[300];
	static char outFileName[300];
	int args_error;
	int mode = MODE_DECOMPRESS;

    printf("MS Gundam .CG Decompressor/Compressor v%d.%02d\n", VER_MAJ, VER_MIN);

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
        printf("Decomrpession Use: gund_cg.exe CG_inputName\n");
		printf("  File extraction only supports DEMO* files at this time.\n");
		printf("Compress File Use: gund_cg.exe -c CGX_Filename New_CG_Filename\n");
        return -1;
    }


	/**********************************************/
	/* Compress or Decompress based on Input Args */
	/**********************************************/

	if(mode == MODE_COMPRESS){

		//Handle Input Parameters
		memset(inFileName, 0, 300);
		strcpy(inFileName,argv[2]);
		memset(outFileName, 0, 300);
		strcpy(outFileName,argv[3]);
		printf("Compression Mode, Input = %s, Output = %s\n",inFileName, outFileName);

		if( compressCG(inFileName, outFileName) < 0){	
			printf("Error creating compressed CG file.\n");
	  		return -1;
	  	}
	}
	else
	{
		/* Decompression */

		//Handle Input Parameters
		memset(inFileName, 0, 300);
		strcpy(inFileName,argv[1]);
		printf("Decompression Mode, Input = %s\n",inFileName);

		if(analyzeCGHeader(inFileName) < 0){
			printf("File decompression failed.\n");
			return -1;
		}
		else{
			printf("File decompression successful.\n");
		}
	}

	return 0;
}
