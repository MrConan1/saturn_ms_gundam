/******************************************************************************/
/* main.c - Main execution file for MS Gundam Decompressor                    */
/* =========================================================================  */
/* Usage:  gund_decmpr.exe InputFname                                         */
/******************************************************************************/
#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

/* Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gundam_decompress.h"
//#include "gundam_compress.h"


/* Defines */
#define VER_MAJ    1
#define VER_MIN    0

#define MODE_DECOMPRESS 0
#define MODE_COMPRESS   1


/* Globals */





int main(int argc, char** argv){

    static char inFileName[300];
	int args_error;
	int mode = MODE_DECOMPRESS;
	int extractMode = 1;

    printf("MS Gundam .CG Decompressor v%d.%02d\n", VER_MAJ, VER_MIN);

    /**************************/
    /* Check input parameters */
    /**************************/

    /* Check for valid # of args */
	args_error = 0;
	if (argc == 2){
		args_error = 0;
		mode = MODE_DECOMPRESS;
	}
    else if (argc == 5){
		if(strcmp(argv[1],"-u") != 0)
			args_error = 1;
		mode = MODE_COMPRESS;
	}
	else{
		args_error = 1;
	}

	if(args_error){
        printf("Decomrpession Use: gund_cg.exe CG_inputName\n");
//		printf("Update File Use: gund_cg.exe -u Orig_VLY_Filename 8bppFilenameToUse New_VLY_Filename\n");
        return -1;
    }


	/**********************************************/
	/* Compress or Decompress based on Input Args */
	/**********************************************/

	if(mode == MODE_COMPRESS){
		printf("Update Mode\n");
	//	if( updateVLYFile(argv[2], argv[3], argv[4]) < 0){	
	//		printf("Error creating updated VLY file.\n");
	//		return -1;
	//	}
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
