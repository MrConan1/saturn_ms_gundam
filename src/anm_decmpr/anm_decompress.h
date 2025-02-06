/*****************************************************************************/
/* ANM_DECOMPRESS.H - Functions to decompress ANM files.                     */
/*****************************************************************************/
#ifndef ANM_DECOMPRESS_H
#define ANM_DECOMPRESS_H




/* Function Prototypes */
int extractANMData(char* pBuffer, char* baseFilename);
int extractANMData_staff(char* pBuffer, char* baseFilename);
int decompressRLEData(char* inputStream, char** outputStream, 
	unsigned int* outSize);


#endif
