#if 0

/*****************************************************************************/
/* VLY_COMPRESS.H - Functions to compress VLY tile data and replace that     */
/*                  data in an existing VLY file.                            */
/*****************************************************************************/
#ifndef VLY_COMPRESS_H
#define VLY_COMPRESS_H


/* Function Prototypes */
int updateVLYFile(char* origVLYFile, char* updatedTiledataFile, char* newVLYFilename);
int compressVLYtiledata(unsigned char* ptr8bppData, int fsize, int width, int height);
void initDictionary();
int updateDictionary(unsigned int offset);
int tryDictionary(unsigned char* uncmprDataBase, unsigned char* uncmprData, int width);
int tryReadLocalWd32AndLocalMask(unsigned char* uncmprData, int width);
int performDirectLiteralRead(unsigned char* uncmprData, int width);



#endif

#endif
