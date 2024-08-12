/*****************************************************************************/
/* GUNDAM_DECOMPRESS.H - Functions to decompress CG files.                   */
/*****************************************************************************/
#ifndef GUNDAM_DECOMPRESS_H
#define GUNDAM_DECOMPRESS_H


//Retain Palette Data in Memory
//struct ColorData{
//	unsigned char red;
//	unsigned char green;
//	unsigned char blue;
//};
//typedef struct ColorData ColorData;


/* Function Prototypes */
int analyzeCGHeader(char* cmprFname);
int performCGDecompression(int cmprFlag, unsigned int comprSrcAddr, 
	unsigned int dstAddr, unsigned int dstBufferSize, unsigned int* dstSize);
unsigned short shar_calc(unsigned short inputWd, int numArithmeticShifts);
int runInnerDecmpression(unsigned int* p_addrCmprInput, 
	unsigned int* p_addrDecmprBuf, unsigned int addrDecmprBufEnd, int loopCtr);


#endif
