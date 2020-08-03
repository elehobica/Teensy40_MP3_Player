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
    size_t size;
    char* data;
    struct _id32frame* next;
} id32frame;

typedef struct _id322frame {
    char ID[3];
    unsigned char sizebytes[3];
    size_t size;
    char* data;
    struct _id322frame* next;
} id322frame;

typedef struct _id32 {
    char version[2];
    char flags;
    size_t size;
    id32frame* firstframe;
} id32;

typedef struct _id32flat {
    size_t size;
    char* buffer;
} id32flat;

typedef enum _mime_t {
    non = 0,
    jpeg,
    png
} mime_t;

typedef enum _ptype_t {
    other = 0x00,
    icon32x32 = 0x01,
    icon = 0x02,
    front_cover = 0x03,
    back_cover = 0x04,
    leaflet = 0x05,
    media = 0x06,
    lead_artist = 0x07,
    artist = 0x08,
    conductor = 0x09,
    band = 0x0a,
    composer = 0x0b,
    lyrcist = 0x0c,
    location = 0x0d,
    recording = 0x0e,
    performance = 0x0f,
    capture = 0x10,
    fish = 0x11,
    illustration = 0x12,
    band_logo = 0x13,
    pub_logo = 0x14
} ptype_t;

const size_t frame_size_limit = 100*1024;

class ID3Read
{
public:
    ID3Read();
    ~ID3Read();
    int loadFile(FsBaseFile* infile);
    int getUTF8Title(char *str, size_t size);
    int getUTF8Album(char *str, size_t size);
    int getUTF8Artist(char *str, size_t size);
    int getPicturePtr(mime_t *mime, ptype_t *ptype, char **ptr, size_t *size);

private:
    FsBaseFile file;
    id31 *id3v1;
    id32 *id3v2;
    int GetID3HeadersFull(FsBaseFile* infile, int testfail, id31** id31save, id32** id32save);
    id32* ID32Detect(FsBaseFile* infile);
    int GetID32(const char *id3v22, const char *id3v23, char **str, size_t *size);
    int GetID32(const char *id3v22, const char *id3v23, char *str, size_t size);
    void ID32Print(id32* id32header);
    void ID32Free(id32* id32header);
    id32flat* ID32Create();
    void ID32AddTag(id32flat* gary, const char* ID, char* data, char* flags, size_t size);
    void ID32Finalise(id32flat* gary);
    int ID32Append(id32flat* gary, char* filename);
    id32flat* ID3Copy1to2(id31* bonar);
    id31* ID31Detect(char* header);
    void ID31Print(id31* id31header);
    void ID31Free(id31* id31header);
};

#endif //_ID3READ_H_