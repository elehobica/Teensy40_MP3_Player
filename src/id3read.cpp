#include "id3read.h"
#include <Arduino.h>
#include <TeensyThreads.h>
#include "utf_conv.h"

extern Threads::Mutex mylock;

ID3Read::ID3Read()
{
    id3v1 = NULL;
    id3v2 = NULL;
}

ID3Read::~ID3Read()
{
    if (id3v1) ID31Free(id3v1);
    if (id3v2) ID32Free(id3v2);
}

int ID3Read::loadFile(FsBaseFile* infile)
{
    int result;
    if (id3v1) ID31Free(id3v1);
    if (id3v2) ID32Free(id3v2);
    file = FsBaseFile(*infile);
    result = GetID3HeadersFull(&file, 1, &id3v1, &id3v2); // 1: no-debug display, 0: debug display
    mylock.lock();
    infile->rewind();
    mylock.unlock();
    return result;
}

int ID3Read::GetID3HeadersFull(FsBaseFile* infile, int testfail, id31** id31save, id32** id32save)
{
    int result;
    char* input;
    id31* id3header;
    id32* id32header;
    int fail = 0;
    // seek to start of header
    mylock.lock();
    //result = fseek(infile, 0 - sizeof(id31), SEEK_END);
    infile->seekSet(infile->size() - sizeof(id31)); // seekEnd doesn't work. if return value, seekSet fails anyhow
    mylock.unlock();
    /*
    if (result) {
        Serial.print("Error seeking to header");
        return -1;
    }
    */
  
    // read in to buffer
    input = (char *) malloc(sizeof(id31));
    mylock.lock();
    //result = fread(input, 1, sizeof(id31), infile);
    result = infile->read(input, sizeof(id31));
    mylock.unlock();
    if (result != sizeof(id31)) {
        char str[256];
        sprintf(str, "Read fail: expected %d bytes but got %d", sizeof(id31), result);
        Serial.println(str);
        return -1;
    }
  
    // now call id3print
  
    id3header = ID31Detect(input);
    // do we win?
    if (id3header) {
        if (!testfail) {
            Serial.println("Valid ID3v1 header detected");
            ID31Print(id3header);
        }
    } else {
        if (!testfail) {
            Serial.println("ID3v1 header not found");
        } else {
            fail=1;
        }
    }
    id32header=ID32Detect(infile);
    if (id32header) {
        if (!testfail) {
            Serial.println("Valid ID3v2 header detected");
            ID32Print(id32header);
            ID32Print(id32header);
        }
    } else {
        if (!testfail) {
            Serial.println("ID3v2 header not found");
        } else {
            fail+=2;
        }
    }
    *id31save=id3header;
    *id32save=id32header;
    return fail;
}

id32* ID3Read::ID32Detect(FsBaseFile* infile)
{
    unsigned char* buffer;
    int result;
    int i = 0;
    id32* id32header;
    int filepos = 0;
    int size = 0;
    id32frame* lastframe = NULL;
    id322frame* lastframev2 = NULL;
    // seek to start
    mylock.lock();
    //fseek(infile, 0, SEEK_SET);
    infile->seekSet(0);
    mylock.unlock();
    // read in first 10 bytes
    buffer = (unsigned char *) calloc(1, 11);
    id32header = (id32 *) calloc(1, sizeof(id32));
    mylock.lock();
    //result = fread(buffer, 1, 10, infile);
    result = infile->read(buffer, 10);
    mylock.unlock();
    filepos += result;
    // make sure we have 10 bytes
    if (result != 10) {
        char str[256];
        sprintf(str, "ID3v2 read failed, expected 10 bytes, read only %d", result);
        Serial.println(str);
        return NULL;
    }
    // do the comparison
    /*
    An ID3v2 tag can be detected with the following pattern:
    $49 44 33 yy yy xx zz zz zz zz
    Where yy is less than $FF, xx is the 'flags' byte and zz is less than $80.
    */
    result=1;
    for (i = 6; i < 10; i++) {
        if (buffer[i] >= 0x80) result = 0;
    }
    if (buffer[0] == 0x49 && buffer[1] == 0x44 && buffer[2] == 0x33 && buffer[3] < 0xff && buffer[4] < 0xff && result) {
        // its ID3v2 :D
        for (i = 6; i < 10; i++) {
            size = size<<7;
            size += buffer[i];
        }
        //char str[256];
        //sprintf(str, "Header size: %d");
        //Serial.println(str);
    } else {
        // cleanup
        free(buffer);
        free(id32header);
        return NULL;
    }
    // set up header
    id32header->version[0] = buffer[3];
    id32header->version[1] = buffer[4];
    id32header->flags = buffer[5];
    id32header->size = size;
    id32header->firstframe = NULL;

    // done with buffer - be sure to free...
    free(buffer);

    // test for flags - die on all for the time being

    if (id32header->flags) {
        //Serial.println("Flags set, can't handle :(");
        return NULL;
    }
    // make sure its version 3
    if (id32header->version[0] != 3) {
        //char str[256];
        //sprintf(str, "Want version 3, have version %d", id32header->version[0]);
        //Serial.println(str);
        //return NULL;
        // this code is modified from version 3, ideally we should reuse some
        while (filepos-10 < (int) id32header->size) {
            // make space for new frame
            int i;
            id322frame* frame = (id322frame *) calloc(1, sizeof(id322frame));
            frame->next = NULL;
            // populate from file
            mylock.lock();
            //result = fread(frame, 1, 6, infile);
            result = infile->read(frame, 6);
            mylock.unlock();
            if (result != 6) {
                char str[256];
                sprintf(str, "Expected to read 6 bytes, only got %d, from point %d in the file", result, filepos);
                Serial.println(str);
                // not freeing this time, but we should deconstruct in future
                return NULL;
            }
            // make sure we haven't got a blank tag
            if (frame->ID[0] == 0) {
                break;
            }
            // update file cursor
            filepos += result;
            // convert size to little endian
            frame->size = 0;
            for (i = 0; i < 3; i++) {
                frame->size = frame->size<<8;
                frame->size += frame->sizebytes[i];
            }
            // debug
            buffer = (unsigned char *) calloc(1,4);
            memcpy(buffer, frame->ID, 3);
            //{
            //    char str[256];
            //    sprintf(str, "Processing frame %s, size %d filepos %d", buffer, frame->size, filepos);
            //    Serial.println(str);
            //}
            free(buffer);
            // read in the data
            frame->data = (char *) calloc(1, frame->size);
            mylock.lock();
            //result = fread(frame->data, 1, frame->size, infile);
            result = infile->read(frame->data, frame->size);
            mylock.unlock();
            filepos += result;
            if (result != (int) frame->size) {
                char str[256];
                sprintf(str, "Expected to read %d bytes, only got %d", frame->size, result);
                Serial.println(str);
                return NULL;
            }
            // add to end of queue
            if (id32header->firstframe == NULL) {
                id32header->firstframe=(id32frame*)frame;
            } else {
                lastframev2->next=frame;
            }
            lastframev2=frame;
            // then loop until we've filled the struct
        }
    } else {
        // start reading frames
        while (filepos-10 < (int) id32header->size) {
            // make space for new frame
            int i;
            id32frame* frame = (id32frame *) calloc(1, sizeof(id32frame));
            frame->next = NULL;
            // populate from file
            mylock.lock();
            //result = fread(frame, 1, 10, infile);
            result = infile->read(frame, 10);
            mylock.unlock();
            if (result != 10) {
                char str[256];
                sprintf(str, "Expected to read 10 bytes, only got %d, from point %d in the file", result, filepos);
                Serial.println(str);
                // not freeing this time, but we should deconstruct in future
                return NULL;
            }
            // make sure we haven't got a blank tag
            if (frame->ID[0] == 0) {
                break;
            }
            if((frame->ID[0] & 0xff) == 0xff) {
                Serial.println("Size is wrong :(");

                // fix size
                /*
                Serial.print("fix size?");
                char c = getchar();
                while(getchar()!= '\n');
                if(c=='y' || c=='Y'){
                // kill the frame
                free(frame);
                // break the file - this is a trigger for later :P
                mylock.lock();
                //fclose(infile);
                infile->close();
                mylock.unlock();
                break;
                }
                */
            }
            // update file cursor
            filepos += result;
            // convert size to little endian
            frame->size = 0;
            for (i = 0; i < 4; i++) {
                frame->size = frame->size << 8;
                frame->size += frame->sizebytes[i];
            }
            // debug
            buffer = (unsigned char *) calloc(1,5);
            memcpy(buffer, frame->ID, 4);
            //{
            //    char str[256];
            //    sprintf(str, "Processing size %d filepos %d frame %s", frame->size, filepos, buffer);
            //    Serial.println(str);
            //}
            free(buffer);
            // read in the data
            frame->data = (char *) calloc(1, frame->size);
            mylock.lock();
            //result = fread(frame->data, 1, frame->size, infile);
            result = infile->read(frame->data, frame->size);
            mylock.unlock();
            filepos += result;
            if (result != (int) frame->size) {
                char str[256];
                sprintf(str, "Expected to read %d bytes, only got %d", frame->size, result);
                Serial.println(str);
                return NULL;
            }
            // add to end of queue
            if (id32header->firstframe == NULL) {
                // read in to buffer
                id32header->firstframe=frame;
            } else {
                lastframe->next=frame;
            }
            lastframe=frame;
            // then loop until we've filled the struct
        }
    }
    return id32header;
}

int ID3Read::getUTF8Title(char* str, size_t size)
{
    return GetID32("TT2", "TIT2", str, size);
}

int ID3Read::getUTF8Album(char* str, size_t size)
{
    return GetID32("TAL", "TALB", str, size);
}

int ID3Read::getUTF8Artist(char* str, size_t size)
{
    return GetID32("TP1", "TPE1", str, size);
}

int ID3Read::GetID32(const char *id3v22, const char *id3v23, char* str, size_t size)
{
    int flg = 0;
    if (!id3v2) { return 0; }
    id32frame* thisframe;
    int ver = id3v2->version[0];
    // loop through tags and process
    thisframe = id3v2->firstframe;
    while (thisframe != NULL) {
        if (id3v2->version[0] == 3) {
            char buffer[5] = {};
            memcpy(buffer, thisframe->ID, 4);
            if (!strcmp(id3v23, buffer)) {
                size_t max_size;
                switch (thisframe->data[0]) { // data has no '\0' termination
                    case 0: // ISO-8859-1
                        max_size = (thisframe->size - 1 <= size - 1) ? thisframe->size - 1 : size - 1;
                        strncpy(str, &thisframe->data[1], max_size);
                        str[max_size] = '\0';
                        break;
                    case 1: { // UTF-16 (w/BOM)
                        char _str[256*2] = {};
                        max_size = (thisframe->size - 3 <= 256*2-2) ? thisframe->size - 3 : 256*2-2;
                        memcpy(_str, &thisframe->data[3], max_size);
                        std::string utf8_str = utf16_to_utf8((const char16_t *) _str);
                        max_size = (utf8_str.length() <= size - 1) ? utf8_str.length() : size - 1;
                        memcpy(str, utf8_str.c_str(), max_size);
                        str[max_size] = '\0';
                        break;
                    }
                    default:
                        break;
                }
                flg = 1;
                break;
            }
        } else if (id3v2->version[0] == 2) {
            id322frame* tframe = (id322frame*) thisframe;
            char buffer[4] = {};
            memcpy(buffer, thisframe->ID, 3);
            if (!strcmp(id3v22, buffer)) {
                size_t max_size;
                switch (tframe->data[0]) { // data has no '\0' termination
                    case 0: // ISO-8859-1
                        max_size = (tframe->size - 1 <= size - 1) ? tframe->size - 1 : size - 1;
                        strncpy(str, &tframe->data[1], max_size);
                        str[max_size] = '\0';
                        break;
                    case 1: { // UTF-16 (w/BOM)
                        char _str[256*2] = {};
                        max_size = (tframe->size - 3 <= 256*2-2) ? tframe->size - 3 : 256*2-2;
                        memcpy(_str, &tframe->data[3], max_size);
                        std::string utf8_str = utf16_to_utf8((const char16_t *) _str);
                        max_size = (utf8_str.length() <= size - 1) ? utf8_str.length() : size - 1;
                        memcpy(str, utf8_str.c_str(), max_size);
                        str[max_size] = '\0';
                        break;
                    }
                    default:
                        break;
                }
                flg = 1;
                break;
            }
        }
        thisframe = (ver == 3) ? thisframe->next : (id32frame*) ((id322frame*) thisframe)->next;
    }
    return flg;
}

void ID3Read::ID32Print(id32* id32header)
{
    id32frame* thisframe;
    int ver = id32header->version[0];
    // loop through tags and process
    thisframe = id32header->firstframe;
    while (thisframe != NULL) {
        char* buffer;
        if (id32header->version[0] == 3) {
            buffer = (char *) calloc(5,1);
            memcpy(buffer, thisframe->ID, 4);
            if (!strcmp("TIT2", buffer) || !strcmp("TT2", buffer)) {
                char str[256];
                //Serial.println("TIT2 tag detected");
                sprintf(str, "Title: %s", thisframe->data+1);
                Serial.println(str);
            } else if (!strcmp("TALB", buffer) || !strcmp("TAL", buffer)) {
                char str[256];
                //Serial.println("TALB tag detected");
                sprintf(str, "Album: %s", thisframe->data+1);
                Serial.println(str);
            } else if (!strcmp("TPE1", buffer) || !strcmp("TP1", buffer)) {
                char str[256];
                //Serial.println("TPE1 tag detected");
                sprintf(str, "Artist: %s", thisframe->data+1);
                Serial.println(str);
            }
            free(buffer);
        } else if (id32header->version[0] == 2) {
            id322frame* tframe = (id322frame*) thisframe;
            buffer = (char *) calloc(4,1);
            memcpy(buffer, thisframe->ID, 3);
            if (!strcmp("TIT2", buffer) || !strcmp("TT2", buffer)) {
                char str[256];
                //Serial.println("TIT2 tag detected");
                sprintf(str, "Title: %s", tframe->data+1);
                Serial.println(str);
            } else if (!strcmp("TALB", buffer) || !strcmp("TAL", buffer)) {
                char str[256];
                //Serial.println("TALB tag detected");
                sprintf(str, "Album: %s", tframe->data+1);
                Serial.println(str);
            } else if (!strcmp("TPE1", buffer) || !strcmp("TP1", buffer)) {
                char str[256];
                //Serial.println("TPE1 tag detected");
                sprintf(str, "Artist: %s", tframe->data+1);
                Serial.println(str);
            }
            free(buffer);
        }
        thisframe = (ver == 3) ? thisframe->next : (id32frame*) ((id322frame*) thisframe)->next;
    }
}

void ID3Read::ID32Free(id32* id32header)
{
    if (id32header->version[0] == 3) {
        id32frame* bonar=id32header->firstframe;
        while (bonar != NULL) {
            id32frame* next = bonar->next;
            free(bonar->data);
            free(bonar);
            bonar=next;
        }
    } else if (id32header->version[0] == 2) {
        id322frame* bonar=(id322frame*)id32header->firstframe;
        while (bonar != NULL) {
            id322frame* next = bonar->next;
            free(bonar->data);
            free(bonar);
            bonar = next;
        }
    }
    free(id32header);
}

id32flat* ID3Read::ID32Create()
{
    id32flat* gary = (id32flat *) calloc(1, sizeof(id32flat));
    // allocate 10 bytes for the main header
    gary->buffer = (char *) calloc(1, 10);
    gary->size = 10;
    return gary;
}

void ID3Read::ID32AddTag(id32flat* gary, const char* ID, char* data, char* flags, size_t size)
{
    // resize the buffer
    int i;
    int killsize;
    //Serial.println("Resizing the buffer");
    gary->buffer = (char *) realloc(gary->buffer, gary->size + size + 10);
    // copy header in
    //Serial.println("Copying the ID");
    for (i = 0; i < 4; i++) {
        gary->buffer[gary->size+i] = ID[i];
    }
    killsize = size;
    // convert to sizebytes - endian independent
    //Serial.println("Copying size");
    for (i = 7; i > 3; i--) {
        gary->buffer[gary->size+i] = killsize & 0xff;
        killsize = killsize>>8;
    }
    // copy flags
    //Serial.println("Setting flags");
    gary->buffer[gary->size+8] = flags[0];
    gary->buffer[gary->size+9] = flags[1];
    // then the data comes through
    memcpy(gary->buffer+gary->size+10, data, size);
    // then update size
    gary->size+= size + 10;
    // done :D
}

void ID3Read::ID32Finalise(id32flat* gary)
{
    int killsize;
    int i;
    // set tag
    gary->buffer[0] = 'I';
    gary->buffer[1] = 'D';
    gary->buffer[2] = '3';
    // set version
    gary->buffer[3] = 3;
    gary->buffer[4] = 0;
    // set flags
    gary->buffer[5] = 0;
    // get size
    killsize = gary->size-10;
    // convert to bytes for header
    for (i = 9; i > 5; i--) {
        gary->buffer[i] = killsize & 0x7f;
        killsize = killsize>>7;
    }
    // done :D
}

int ID3Read::ID32Append(id32flat* gary, char* filename)
{
    char* mp3;
    unsigned long size;
    FsFile infile;
    char str[256];
    // then read in, and write to file :D
    mylock.lock();
    //infile = fopen(filename, "r");
    infile.open(filename, O_RDONLY);
    //fseek(infile, 0, SEEK_END);
    infile.seekEnd(0);
    //size = ftell(infile);
    size = infile.curPosition();
    sprintf(str, "File size: %ld", size);
    Serial.println(str);
    // this can only read in 2gb of file
    //fseek(infile, 0, SEEK_SET);
    infile.seekSet(0);
    mp3 = (char *) calloc(1, size);
    //fread(mp3, 1, size, infile);
    infile.read(mp3, size);
    // then close, reopen for reading
    //fclose(infile);
    infile.close();
    //infile = fopen(filename, "w");
    infile.open(filename, O_WRONLY);
    //infile=NULL;
    //if (!infile) {
    if (!infile.available()) {
        Serial.println("Can't open file for writing :(");
        return -1;
    }
    //fwrite(gary->buffer, 1, gary->size, infile);
    infile.write(gary->buffer, gary->size);
    //fwrite(mp3, 1, size, infile);
    infile.write(mp3, size);
    //fclose(infile);
    infile.close();
    mylock.unlock();
    return 0;
}

id32flat* ID3Read::ID3Copy1to2(id31* bonar)
{
    // todo: get rid of spaces on the end of padded ID3v1 :/
    Serial.println("Creating new ID3v2 header");
    id32flat* final = ID32Create();
    char* buffer;
    char flags[2];
    flags[0] = 0;
    flags[1] = 0;
    // copy title
    buffer = (char *) calloc(1, 31);
    memcpy(buffer+1, bonar->title, 30);
    Serial.println("Adding title");
    ID32AddTag(final, "TIT2", buffer, flags, strlen(buffer+1)+1);
    free(buffer);
    // copy artist
    buffer = (char *) calloc(1, 31);
    memcpy(buffer+1, bonar->artist, 30);
    Serial.println("Adding artist");
    ID32AddTag(final, "TPE1", buffer, flags, strlen(buffer+1)+1);
    free(buffer);
    // copy album
    buffer = (char *) calloc(1, 31);
    memcpy(buffer+1, bonar->album, 30);
    Serial.println("Adding album");
    ID32AddTag(final, "TALB", buffer, flags, strlen(buffer+1)+1);
    free(buffer);
    // then finalise
    Serial.println("Finalising...");
    ID32Finalise(final);
    Serial.println("done :D");
    // all done :D
    return final;
}

id31* ID3Read::ID31Detect(char* header)
{
    char test[4];
    id31* toreturn = (id31 *) calloc(sizeof(id31), 1);
    memcpy(test, header, 3);
    test[3] = 0;
    // make sure TAG is present
    if (!strcmp("TAG", test)) {
        // successrar
        memcpy(toreturn, header, sizeof(id31));
        return toreturn;
    }
    // otherwise fail
    return NULL;
}

void ID3Read::ID31Print(id31* id31header)
{
    char str[256];
    char* buffer = (char *) calloc(1, 31);
    memcpy(buffer, id31header->title, 30);
    sprintf(str, "Title: %s", buffer);
    Serial.println(str);
    memcpy(buffer, id31header->artist, 30);
    sprintf(str, "Artist: %s", buffer);
    Serial.println(str);
    memcpy(buffer, id31header->album, 30);
    sprintf(str, "Album: %s", buffer);
    Serial.println(str);
    free(buffer);
}

void ID3Read::ID31Free(id31* id31header)
{
    free(id31header);
}

