#ifndef _ID3READ_H_
#define _ID3READ_H_

#include <SdFat.h>

typedef struct _id31 {
    char header[3];
    char title[30];
    char artist[30];
    char album[30];
    char year[4];
    char comment[28];
    char zero;
    unsigned char tracknum;
    unsigned char genre;
} id31;

typedef struct _id32frame {
    char ID[4];
    unsigned char sizebytes[4];
    char flags[2];
    int size;
    char* data;
    struct _id32frame* next;
} id32frame;

typedef struct _id322frame {
    char ID[3];
    unsigned char sizebytes[3];
    int size;
    char* data;
    struct _id322frame* next;
} id322frame;

typedef struct _id32 {
    char version[2];
    char flags;
    int size;
    id32frame* firstframe;
} id32;

typedef struct _id32flat {
    int size;
    char* buffer;
} id32flat;

int GetID3Headers(FsBaseFile* infile, int testfail, id31** id31save, id32** id32save);
int GetID3Headers(FsBaseFile* infile, int testfail);
int GetID3HeadersFull(FsBaseFile* infile, int testfail, id31** id31save, id32** id32save);
id32* ID32Detect(FsBaseFile* infile);
int GetID32(id32* id32header, const char *id, char* str);
void ID32Print(id32* id32header);
void ID32Free(id32* id32header);
id32flat* ID32Create();
void ID32AddTag(id32flat* gary, const char* ID, char* data, char* flags, int size);
void ID32Finalise(id32flat* gary);
int ID32Append(id32flat* gary, char* filename);
id32flat* ID3Copy1to2(id31* bonar);
id31* ID31Detect(char* header);
void ID31Print(id31* id31header);
void ID31Free(id31* id31header);

#endif //_ID3READ_H_