#ifndef _TAGREAD_H_
#define _TAGREAD_H_

#include <ff_util.h>

#include <FLAC/metadata.h>

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
    uint64_t pos; // position in the file
    size_t size;
    char* data;
    bool hasFullData;
    struct _id32frame* next;
} id32frame;

typedef struct _id322frame {
    char ID[3];
    unsigned char sizebytes[3];
    uint64_t pos; // position in the file
    size_t size;
    char* data;
    bool hasFullData;
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

typedef enum _mp4_data_t {
    reserved = 0x00000000,
    UTF8 = 0x00000001,
    UTF16 = 0x00000002,
    JPEG = 0x0000000d,
    PNG = 0x0000000e
} mp4_data_t;

typedef struct _MP4_ilst_item {
    char type[4];
    mp4_data_t data_type;
    uint32_t data_size;
    uint64_t pos; // position in the file
    char *data_buf;
    bool hasFullData;
    struct _MP4_ilst_item* next;
} MP4_ilst_item;

typedef struct _MP4_ilst {
    MP4_ilst_item *first;
    MP4_ilst_item *last;
} MP4_ilst;

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

const size_t frame_size_limit = 1024;
const size_t frame_start_bytes = 16;

class TagRead
{
public:
    TagRead();
    ~TagRead();
    int loadFile(uint16_t file_idx);
    int getUTF8Track(char *str, size_t size);
    int getUTF8Title(char *str, size_t size);
    int getUTF8Album(char *str, size_t size);
    int getUTF8Artist(char *str, size_t size);
    int getUTF8Year(char *str, size_t size);
    int getPictureCount();
    int getPicturePos(int idx, mime_t *mime, ptype_t *ptype, uint64_t *pos, size_t *size);

private:
    MutexFsBaseFile file;

    id31 *id3v1;
    id32 *id3v2;
    int GetID3HeadersFull(MutexFsBaseFile *infile, int testfail, id31** id31save, id32** id32save);
    id32* ID32Detect(MutexFsBaseFile *infile);
    int GetID32UTF8(const char *id3v22, const char *id3v23, char *str, size_t size);
    int GetID3IDCount(const char *id3v22, const char *id3v23);
    void ID32Print(id32* id32header);
    void ID32Free(id32* id32header);
    id32flat* ID32Create();
    void ID32AddTag(id32flat* gary, const char* ID, char* data, char* flags, size_t size);
    void ID32Finalise(id32flat* gary);
    int ID32Append(id32flat* gary, char* filename);
    id32flat* ID3Copy1to2(id31* bonar);
    int ID31Detect(char* header, id31 **id31header);
    void ID31Print(id31* id31header);
    void ID31Free(id31* id31header);
    int getID3Picture(int idx, mime_t *mime, ptype_t *ptype, uint64_t *pos, size_t *size);

    int getListChunk(MutexFsBaseFile *file);
    int findNextChunk(MutexFsBaseFile *file, uint32_t end_pos, char chunk_id[4], uint32_t *pos, uint32_t *size);

    MP4_ilst mp4_ilst;
    void clearMP4_ilst();
    int getMP4Box(MutexFsBaseFile *file);
    int findNextMP4Box(MutexFsBaseFile *file, uint32_t end_pos, char chunk_id[4], uint32_t *pos, uint32_t *size);
    uint32_t getMP4BoxBE32(unsigned char c[4]);
    int GetMP4BoxUTF8(const char *mp4_type, char *str, size_t size);
    int GetMP4TypeCount(const char *mp4_type);
    int getMP4Picture(int idx, mime_t *mime, ptype_t *ptype, uint64_t *pos, size_t *size);

    FLAC__StreamMetadata *flac_tags;
    FLAC__StreamMetadata **flac_pics;
    int flac_pics_count;
    int getFlacMetadata(MutexFsBaseFile *file);
    int GetFlacTagUTF8(const char *lc_key, size_t key_size, char *str, size_t size);
    int GetFlacPictureCount();
    int getFlacPicture(int idx, mime_t *mime, ptype_t *ptype, uint64_t *pos, size_t *size);
    void flacFree();
};

#endif //_TAGREAD_H_