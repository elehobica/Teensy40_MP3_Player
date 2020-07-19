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

int GetID3Headers(FILE*, int);
int GetID3HeadersFull(FILE*, int, id31**, id32**);
id31* ID31Detect(char*);
id32* ID32Detect(FILE*);
void ID32Print(id32*);
void ID31Print(id31*);
void ID32Free(id32*);
void ID31Free(id31*);
id32flat* ID32Create();
void ID32AddTag(id32flat*, char* ID, char* data, char* flags, int size);
void ID32Finalise(id32flat*);
id32flat* ID3Copy1to2(id31*);
int ID32Append(id32flat*, char*);