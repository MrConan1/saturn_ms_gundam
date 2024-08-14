/*****************************************************************************/
/* GUNDAM_DECOMPRESS.H - Functions to decompress CG files.                   */
/*****************************************************************************/
#ifndef GUNDAM_DECOMPRESS_H
#define GUNDAM_DECOMPRESS_H




/* Function Prototypes */
int analyzeCGHeader(char* cmprFname);
int performCGDecompression(int cmprFlag, unsigned int comprSrcAddr, 
	unsigned int dstAddr, unsigned int dstBufferSize, unsigned int* dstSize);
int runInnerDecmpression(unsigned int* p_addrCmprInput, 
	unsigned int* p_addrDecmprBuf, unsigned int addrDecmprBufEnd, int loopCtr);


#endif
