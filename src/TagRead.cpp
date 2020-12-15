/*------------------------------------------------------/
/ TagRead
/-------------------------------------------------------/
/ Copyright (c) 2020, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include "TagRead.h"
#include <Arduino.h>
#include <ff_util.h>
#include <utf_conv.h>

TagRead::TagRead()
{
    id3v1 = NULL;
    id3v2 = NULL;
    clearMP4_ilst();
    flac_tags = NULL;
    flac_pics = NULL;
    flac_pics_count = 0;
}

TagRead::~TagRead()
{
    if (id3v1) ID31Free(id3v1);
    if (id3v2) ID32Free(id3v2);
    flacFree();
}

int TagRead::loadFile(uint16_t file_idx)
{
    if (id3v1) { ID31Free(id3v1); id3v1 = NULL; }
    if (id3v2) { ID32Free(id3v2); id3v2 = NULL; }
    clearMP4_ilst();
    flacFree();
    flac_tags = NULL;
    flac_pics = NULL;
    
    file_menu_get_obj(file_idx, &file);

    // try MP4 first (and try ID3 next because ID3 header objects need to be generated)
    getMP4Box(&file);

    // try ID3v1 or ID3v2
    if (GetID3HeadersFull(&file, 1 /* 1: no-debug display, 0: debug display */, &id3v1, &id3v2) == 0) { file.close(); return 1; }

    // If ID3 failed, try to read FLAC tag
    if (getFlacMetadata(&file)) { file.close(); return 1; }

    // If all failed, try to read LIST chunk (for WAV file)
    if (getListChunk(&file)) { file.close(); return 1; }

    file.close();
    return 0;
}

int TagRead::GetID3HeadersFull(MutexFsBaseFile *infile, int testfail, id31** id31save, id32** id32save)
{
    int result;
    char* input;
    id31* id31header;
    id32* id32header;
    int fail = 0;
    // seek to start of header
    //result = fseek(infile, 0 - sizeof(id31), SEEK_END);
    infile->seekSet(infile->size() - sizeof(id31)); // seekEnd doesn't work. if return value, seekSet fails anyhow
    /*
    if (result) {
        Serial.print("Error seeking to header");
        return -1;
    }
    */
  
    // read in to buffer
    input = (char *) malloc(sizeof(id31));
    //result = fread(input, 1, sizeof(id31), infile);
    result = infile->read(input, sizeof(id31));
    if (result != sizeof(id31)) {
        char str[256];
        sprintf(str, "Read fail: expected %d bytes but got %d", sizeof(id31), result);
        Serial.println(str);
        return -1;
    }
  
    // now call id3print
  
    int flg = ID31Detect(input, &id31header);
    // do we win?
    if (flg) {
        if (!testfail) {
            Serial.println("Valid ID3v1 header detected");
            ID31Print(id31header);
        }
    } else {
        if (!testfail) {
            Serial.println("ID3v1 header not found");
        } else {
            fail=1;
        }
    }
    free(input);
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
    *id31save=id31header;
    *id32save=id32header;
    return fail;
}

id32* TagRead::ID32Detect(MutexFsBaseFile *infile)
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
    //fseek(infile, 0, SEEK_SET);
    infile->seekSet(0);
    // read in first 10 bytes
    buffer = (unsigned char *) calloc(1, 11);
    id32header = (id32 *) calloc(1, sizeof(id32));
    //result = fread(buffer, 1, 10, infile);
    result = infile->read(buffer, 10);
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
    if (id32header->version[0] != 3) { // not ID3v2.3
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
            //result = fread(frame, 1, 6, infile);
            result = infile->read(frame, 6);
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
            frame->pos = infile->position();
            if (frame->size < frame_size_limit) {
                // read in the data
                frame->data = (char *) calloc(1, frame->size);
                //result = fread(frame->data, 1, frame->size, infile);
                result = infile->read(frame->data, frame->size);
                frame->hasFullData = true;
            } else { // give up to get full contents due to size over
                /*
                char str[256];
                sprintf(str, "frameID: %c%c%c size over %d", frame->ID[0], frame->ID[1], frame->ID[2], frame->size);
                Serial.println(str);
                */
                frame->data = (char *) calloc(1, frame_start_bytes); // for frame parsing
                infile->read(frame->data, frame_start_bytes);
                if (infile->seekCur(frame->size - frame_start_bytes)) {
                    result = frame->size;
                } else {
                    result = 0;
                }
                frame->hasFullData = false;
            }
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
    } else { // ID3v2.3
        // start reading frames
        while (filepos-10 < (int) id32header->size) {
            // make space for new frame
            int i;
            id32frame* frame = (id32frame *) calloc(1, sizeof(id32frame));
            frame->next = NULL;
            // populate from file
            //result = fread(frame, 1, 10, infile);
            result = infile->read(frame, 10);
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
                //fclose(infile);
                infile->close();
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


            frame->pos = infile->position();
            if (frame->size < frame_size_limit) {
                // read in the data
                frame->data = (char *) calloc(1, frame->size);
                //result = fread(frame->data, 1, frame->size, infile);
                result = infile->read(frame->data, frame->size);
                frame->hasFullData = true;
            } else { // give up to get full contents due to size over
                /*
                char str[256];
                sprintf(str, "frameID: %c%c%c%c size over %d", frame->ID[0], frame->ID[1], frame->ID[2], frame->ID[3], frame->size);
                Serial.println(str);
                */
                frame->data = (char *) calloc(1, frame_start_bytes); // for frame parsing
                infile->read(frame->data, frame_start_bytes);
                if (infile->seekCur(frame->size - frame_start_bytes)) {
                    result = frame->size;
                } else {
                    result = 0;
                }
                frame->hasFullData = false;
            }
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

int TagRead::getUTF8Track(char* str, size_t size)
{
    if (GetMP4BoxUTF8("\xa9""trk", str, size)) { return 1; }
    if (GetMP4BoxUTF8("trkn", str, size)) { return 1; }
    if (GetID32UTF8("TRK", "TRCK", str, size)) { return 1; }
    if (GetFlacTagUTF8("tracknumber", 11, str, size)) { return 1; }
    if (strlen(id3v1->title) && size >= 4) { // check titlte because track is unsigned char
        sprintf(str, "%d", id3v1->tracknum);
        return 1;
    }
    return 0;
}

int TagRead::getUTF8Title(char* str, size_t size)
{
    if (GetMP4BoxUTF8("\xa9""nam", str, size)) { return 1; }
    if (GetID32UTF8("TT2", "TIT2", str, size)) { return 1; }
    if (GetFlacTagUTF8("title", 5, str, size)) { return 1; }
    if (strlen(id3v1->title)) {
        memset(str, 0, size);
        strncpy(str, id3v1->title, (strlen(id3v1->title) < size - 1) ? strlen(id3v1->title) : size - 1);
        return 1;
    }
    return 0;
}

int TagRead::getUTF8Album(char* str, size_t size)
{
    if (GetMP4BoxUTF8("\xa9""alb", str, size)) { return 1; }
    if (GetID32UTF8("TAL", "TALB", str, size)) { return 1; }
    if (GetFlacTagUTF8("album", 5, str, size)) { return 1; }
    if (strlen(id3v1->album)) {
        memset(str, 0, size);
        strncpy(str, id3v1->album, (strlen(id3v1->album) < size - 1) ? strlen(id3v1->album) : size - 1);
        return 1;
    }
    return 0;
}

int TagRead::getUTF8Artist(char* str, size_t size)
{
    if (GetMP4BoxUTF8("\xa9""ART", str, size)) { return 1; }
    if (GetID32UTF8("TP1", "TPE1", str, size)) { return 1; }
    if (GetFlacTagUTF8("artist", 6, str, size)) { return 1; }
    if (strlen(id3v1->artist)) {
        memset(str, 0, size);
        strncpy(str, id3v1->artist, (strlen(id3v1->artist) < size - 1) ? strlen(id3v1->artist) : size - 1);
        return 1;
    }
    return 0;
}

int TagRead::getUTF8Year(char* str, size_t size)
{
    if (GetMP4BoxUTF8("\xa9""day", str, size)) { return 1; }
    if (GetID32UTF8("TYE", "TYER", str, size)) { return 1; }
    if (GetFlacTagUTF8("date", 4, str, size)) { return 1; }
    if (strlen(id3v1->year)) {
        memset(str, 0, size);
        strncpy(str, id3v1->year, (strlen(id3v1->year) < size - 1) ? strlen(id3v1->year) : size - 1);
        return 1;
    }
    return 0;
}

int TagRead::getPictureCount()
{
    return GetMP4TypeCount("covr") + GetID3IDCount("PIC", "APIC") + GetFlacPictureCount();
}

int TagRead::getPicturePos(int idx, mime_t *mime, ptype_t *ptype, uint64_t *pos, size_t *size)
{
    int i = idx;
    // for MP4 Picture

    if (i < GetMP4TypeCount("covr")) {
        getMP4Picture(i, mime, ptype, pos, size);
        return (*size != 0);
    }
    i -= GetMP4TypeCount("covr");

    // for MP4 Picture
    if (i >= 0 && i < GetID3IDCount("PIC", "APIC")) {
        getID3Picture(i, mime, ptype, pos, size);
        return (*size != 0);
    }
    i -= GetID3IDCount("PIC", "APIC");

    // for FLAC Picture
    if (i >= 0 && i < GetFlacPictureCount()) {
        getFlacPicture(i, mime, ptype, pos, size);
        return (*size != 0);
    }

    return 0;
}

int TagRead::getID3Picture(int idx, mime_t *mime, ptype_t *ptype, uint64_t *pos, size_t *size)
{
    int count = 0;
    *mime = non;
    *ptype = other;
    *size = 0;
    bool hasFullData = false;
    if (!id3v2) { return 0; }
    id32frame* thisframe;
    int ver = id3v2->version[0];
    // loop through tags and process
    thisframe = id3v2->firstframe;
    while (thisframe != NULL) {
        if (id3v2->version[0] == 3) {
            if (!strncmp("APIC", thisframe->ID, 4) && thisframe->data != NULL) {
                if (idx == count) {
                    // encoding(1) + mime(\0) + ptype(1) + desc(1) + binary
                    //if (thisframe->data[0] == 0) {
                    if (thisframe->data[0] == 0 || thisframe->data[0] == 1) { // normally 'encoding' must be ISO-8859-1 (00h), but found some (01h) examples
                        if (!strncmp("image/jpeg", &thisframe->data[1], 10)) {
                            *mime = jpeg;
                            *ptype = static_cast<ptype_t>(thisframe->data[1+11]);
                            if (thisframe->data[1+11+1] == 0) { // normally 'desc' is needed, but ... (see below case)
                                // *ptr = &thisframe->data[1+11+1+1];
                                *size = thisframe->size - (1+11+1+1);
                                *pos = thisframe->pos + (1+11+1+1);
                            } else if (thisframe->data[1+11+1+0] == 0xFF && thisframe->data[1+11+1+1] == 0xFE) { // found some illegal files have no 'desc'
                                // *ptr = &thisframe->data[1+11+1];
                                *size = thisframe->size - (1+11+1);
                                *pos = thisframe->pos + (1+11+1);
                            }
                            hasFullData = thisframe->hasFullData;
                        } else if (!strncmp("image/png", &thisframe->data[1], 9)) {
                            *mime = png;
                            *ptype = static_cast<ptype_t>(thisframe->data[1+10]);
                            if (thisframe->data[1+10+1] == 0) { // normally 'desc' is needed, but ... (see below case)
                                // *ptr = &thisframe->data[1+10+1+1];
                                *size = thisframe->size - (1+10+1+1);
                                *pos = thisframe->pos + (1+10+1+1);
                            } else if (thisframe->data[1+10+1+0] == 0x89 && thisframe->data[1+10+1+1] == 0x50) { // found some illegal files have no 'desc'
                                //*ptr = &thisframe->data[1+10+1];
                                *size = thisframe->size - (1+10+1);
                                *pos = thisframe->pos + (1+10+1);
                            }
                            hasFullData = thisframe->hasFullData;
                        }
                    }
                    break;
                }
                count++;
            }
        } else if (id3v2->version[0] == 2) {
            id322frame* tframe = (id322frame*) thisframe;
            if (!strncmp("PIC", tframe->ID, 3) && tframe->data != NULL) {
                if (idx == count) {
                    // encoding(1) + mime(3) + ptype(1) + desc(1) + binary
                    if (tframe->data[0] == 0) { // ISO-8859-1
                        if (!strncmp("JPG", &tframe->data[1], 3)) {
                            *mime = jpeg;
                            *ptype = static_cast<ptype_t>(tframe->data[1+3]);
                            if (tframe->data[1+3+1] == 0) {
                                //*ptr = &tframe->data[1+3+1+1];
                                *size = tframe->size - (1+3+1+1);
                                *pos = tframe->pos + (1+3+1+1);
                            }
                            hasFullData = tframe->hasFullData;
                        } else if (!strncmp("PNG", &tframe->data[1], 3)) {
                            *mime = png;
                            *ptype = static_cast<ptype_t>(tframe->data[1+3]);
                            if (tframe->data[1+3+1] == 0) {
                                //*ptr = &tframe->data[1+3+1+1];
                                *size = tframe->size - (1+3+1+1);
                                *pos = tframe->pos + (1+3+1+1);
                            }
                            hasFullData = tframe->hasFullData;
                        }
                    }
                    break;
                }
                count++;
            }
        }
        thisframe = (ver == 3) ? thisframe->next : (id32frame*) ((id322frame*) thisframe)->next;
    }
    return (hasFullData == true);
}

int TagRead::GetID32UTF8(const char *id3v22, const char *id3v23, char *str, size_t size)
{
    int flg = 0;
    if (!id3v2) { return 0; }
    id32frame* thisframe;
    int ver = id3v2->version[0];
    // loop through tags and process
    thisframe = id3v2->firstframe;
    while (thisframe != NULL) {
        if (id3v2->version[0] == 3) {
            if (!strncmp(id3v23, thisframe->ID, 4)) {
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
            if (!strncmp(id3v22, tframe->ID, 3)) {
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

int TagRead::GetID3IDCount(const char *id3v22, const char *id3v23)
{
    int count = 0;
    if (!id3v2) { return 0; }
    id32frame* thisframe;
    int ver = id3v2->version[0];
    // loop through tags and process
    thisframe = id3v2->firstframe;
    while (thisframe != NULL) {
        if (id3v2->version[0] == 3) {
            if (!strncmp(id3v23, thisframe->ID, 4)) {
                count++;
            }
        } else if (id3v2->version[0] == 2) {
            id322frame* tframe = (id322frame*) thisframe;
            if (!strncmp(id3v22, tframe->ID, 3)) {
                count++;
            }
        }
        thisframe = (ver == 3) ? thisframe->next : (id32frame*) ((id322frame*) thisframe)->next;
    }
    return count;
}

void TagRead::ID32Print(id32* id32header)
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

void TagRead::ID32Free(id32* id32header)
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

id32flat* TagRead::ID32Create()
{
    id32flat* gary = (id32flat *) calloc(1, sizeof(id32flat));
    // allocate 10 bytes for the main header
    gary->buffer = (char *) calloc(1, 10);
    gary->size = 10;
    return gary;
}

void TagRead::ID32AddTag(id32flat* gary, const char* ID, char* data, char* flags, size_t size)
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

void TagRead::ID32Finalise(id32flat* gary)
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

int TagRead::ID32Append(id32flat* gary, char* filename)
{
    char* mp3;
    unsigned long size;
    MutexFsBaseFile infile;
    char str[256];
    // then read in, and write to file :D
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
    return 0;
}

id32flat* TagRead::ID3Copy1to2(id31* bonar)
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

int TagRead::ID31Detect(char* header, id31 **id31header)
{
    char test[4];
    *id31header = (id31 *) calloc(sizeof(id31), 1);
    memcpy(test, header, 3);
    test[3] = 0;
    // make sure TAG is present
    if (!strcmp("TAG", test)) {
        // successrar
        memcpy(*id31header, header, sizeof(id31));
        return 1;
    }
    // otherwise fail
    return 0;
}

void TagRead::ID31Print(id31* id31header)
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

void TagRead::ID31Free(id31* id31header)
{
    free(id31header);
}

// ========================
// LIST parsing Start
// ========================
// int TagRead::findNextChunk(MutexFsBaseFile *file, uint32_t end_pos, char chunk_id[4], uint32_t *pos, uint32_t *size)
// end_pos: chunk search stop position
// chunk_id: out: chunk id deteced`
// *pos: in: chunk search start position, out: next chunk search start position
// *size: out: chunk size detected
int TagRead::findNextChunk(MutexFsBaseFile *file, uint32_t end_pos, char chunk_id[4], uint32_t *pos, uint32_t *size)
{
    if (end_pos <= *pos + 8) { return 0; }
    file->seekSet(*pos);
    file->read(chunk_id, 4);
    file->read(size, sizeof(uint32_t));
    *size = (*size + 1)/2*2; // next offset must be even number
    *pos += *size + 8;
    if (end_pos < *pos) { return 0; }
    return 1;
}

int TagRead::getListChunk(MutexFsBaseFile *file)
{
    char str[256];
    file->rewind();
    file->read(str, 12);
	if (str[ 0]=='R' && str[ 1]=='I' && str[ 2]=='F' && str[ 3]=='F' &&
	    str[ 8]=='W' && str[ 9]=='A' && str[10]=='V' && str[11]=='E')
	{
        uint32_t wav_end_pos;
        memcpy(&wav_end_pos, &str[4], 4);
        wav_end_pos += 8;
        char chunk_id[4];
        uint32_t pos = 12;
        uint32_t size;
        while (findNextChunk(file, wav_end_pos, chunk_id, &pos, &size)) { // search from 'RIFF' + size + 'WAVE' for every chunk
			if (memcmp(chunk_id, "LIST", 4) == 0) {
                file->seekSet(pos-size);
                char info_id[4];
                file->read(info_id, sizeof(info_id));
                if (memcmp(info_id, "INFO", 4) == 0) { // info is not chunk element
                    uint32_t list_end_pos = pos;
                    pos = pos - size + 4;
                    while (findNextChunk(file, list_end_pos, chunk_id, &pos, &size)) { // search from 'LIST' + size + 'INFO' for every chunk
                        file->seekSet(pos-size);
                        file->read(str, size);
                        // copy LIST INFO data to ID3v1 member (note that ID3v1 members don't finish with '\0')
                        if (memcmp(chunk_id, "IART", 4) == 0) { // Artist
                            strncpy(id3v1->artist, str, sizeof(id3v1->artist));
                        } else if (memcmp(chunk_id, "INAM", 4) == 0) { // Title
                            strncpy(id3v1->title, str, sizeof(id3v1->title));
                        } else if (memcmp(chunk_id, "IPRD", 4) == 0) { // Album
                            strncpy(id3v1->album, str, sizeof(id3v1->album));
                        } else if (memcmp(chunk_id, "ICRD", 4) == 0) { // Year
                            strncpy(id3v1->year, str, sizeof(id3v1->year));
                        } else if (memcmp(chunk_id, "IPRT", 4) == 0) { // Track  ==> type not matched
                            id3v1->tracknum = (unsigned char) atoi(str); // atoi stops conversion if non-number appeared
                        //} else if (memcmp(chunk_id, "IGNR", 4) == 0) { // Genre  ==> type not matched
                        }
                    }
                    return 1;
                }
            }
        }
    }
    return 0;
}
// ========================
// LIST parsing End
// ========================

// ========================
// MP4 parsing Start
// ========================
void TagRead::clearMP4_ilst()
{
    MP4_ilst_item *mp4_ilst_item = mp4_ilst.first;
    while (mp4_ilst_item) {
        if (mp4_ilst_item->data_buf) free(mp4_ilst_item->data_buf);
        MP4_ilst_item *temp = mp4_ilst_item->next;
        free(mp4_ilst_item);
        mp4_ilst_item = temp;
    }
    mp4_ilst.first = NULL;
    mp4_ilst.last = NULL;
}

// Big Endian read for char[4]
uint32_t TagRead::getMP4BoxBE32(unsigned char c[4])
{
    uint32_t be32 = 0;
    be32 = 0;
    for (int i = 0; i < 4; i++) {
        be32 += (uint32_t) c[i] << ((3-i)*8);
    }
    return be32;
}

int TagRead::findNextMP4Box(MutexFsBaseFile *file, uint32_t end_pos, char type[4], uint32_t *pos, uint32_t *size)
{
    unsigned char c[8]; // size(4) + type(4)
    if (end_pos <= *pos + 8) { return 0; }
    file->seekSet(*pos);
    file->read(c, sizeof(c));
    *size = getMP4BoxBE32(c);
    memcpy(type, &c[4], 4);
    if (*size < 8) { return 0; } // size is out of 32bit range
    *pos += *size;
    if (end_pos < *pos) { return 0; } // size overflow
    /*
    { // DEBUG
        char str[256];
        sprintf(str, "%c%c%c%c: %lu Byte", type[0], type[1], type[2], type[3], *size);
        Serial.println(str);
    }
    */
    // for sub container holders
    if (strncmp(type, "moov", 4) == 0 ||
        strncmp(type, "trak", 4) == 0 ||
        strncmp(type, "mdia", 4) == 0 ||
        strncmp(type, "minf", 4) == 0 ||
        strncmp(type, "stbl", 4) == 0 ||
        strncmp(type, "dinf", 4) == 0 ||
        strncmp(type, "udta", 4) == 0 ||
        strncmp(type, "ilst", 4) == 0 ||
        0) {
        uint32_t pos_next = *pos - *size + 8;
        uint32_t size_next;
        findNextMP4Box(file, *pos, type, &pos_next, &size_next);
    } else if (strncmp(type, "meta", 4) == 0) {
        uint32_t pos_next = *pos - *size + 8 + 4; // meta is illegal size position somehow
        uint32_t size_next;
        findNextMP4Box(file, *pos, type, &pos_next, &size_next);

    // below are end leaves of optional APPLE item list box
    } else if (type[0] == 0xa9 || strncmp(type, "covr", 4) == 0 ||strncmp(type, "trkn", 4) == 0) {
        /*
        { // DEBUG
            char str[256];
            sprintf(str, "%c%c%c%c: %lu Byte", type[0], type[1], type[2], type[3], *size);
            Serial.println(str);
        }
        */
        unsigned char data[8];
        file->seekSet(*pos - *size + 8);
        file->read(data, sizeof(data));
        uint32_t data_size = getMP4BoxBE32(data) - 8 - 8; // - 8 - 8: - (size(4) + 'data'(4)) - (data_type(4) + data_locale(4))
        if (data[4] == 'd' && data[5] == 'a' && data[6] == 't' && data[7] == 'a') {
            unsigned char data_type[4];
            unsigned char data_locale[4];
            file->read(data_type, sizeof(data_type));
            file->read(data_locale, sizeof(data_locale));
            MP4_ilst_item *mp4_ilst_item = (MP4_ilst_item *) calloc(1, sizeof(MP4_ilst_item));
            if (mp4_ilst.first == NULL) {
                mp4_ilst.first = mp4_ilst.last = mp4_ilst_item;
            } else {
                mp4_ilst.last->next = mp4_ilst_item;
                mp4_ilst.last = mp4_ilst.last->next;
            }
            mp4_ilst.last = mp4_ilst_item;
            memcpy(mp4_ilst.last->type, type, 4);
            mp4_ilst.last->data_type = static_cast<mp4_data_t>(getMP4BoxBE32(data_type));
            mp4_ilst.last->data_size = data_size;
            mp4_ilst.last->pos = file->position();
            if (data_size < frame_size_limit) {
                mp4_ilst.last->hasFullData = true;
                mp4_ilst.last->data_buf = (char *) calloc(1, data_size);
                file->read(mp4_ilst.last->data_buf, data_size);
            } else {
                mp4_ilst.last->hasFullData = false;
                mp4_ilst.last->data_buf = (char *) calloc(1, frame_start_bytes);
                file->read(mp4_ilst.last->data_buf, frame_start_bytes);
            }
            mp4_ilst.last->next = NULL;
        }
    }
    // for next leaf
    findNextMP4Box(file, end_pos, type, pos, size);
    return 1;
}

int TagRead::getMP4Box(MutexFsBaseFile *file)
{
    char type[4];
    uint32_t end_pos = file->fileSize();
    uint32_t pos = 0;
    uint32_t size;
    return findNextMP4Box(file, end_pos, type, &pos, &size);
}

int TagRead::GetMP4BoxUTF8(const char *mp4_type, char *str, size_t size)
{
    MP4_ilst_item *mp4_ilst_item = mp4_ilst.first;
    size_t max_size;

    while (mp4_ilst_item) {
        /*
        { // DEBUG
            char str[256];
            sprintf(str, "%c%c%c%c: %lu Byte", mp4_ilst_item->type[0], mp4_ilst_item->type[1], mp4_ilst_item->type[2], mp4_ilst_item->type[3], mp4_ilst_item->data_size);
            Serial.println(str);
        }
        */
        if (memcmp(mp4_ilst_item->type, mp4_type, 4) == 0) {
            //Serial.println("  matched");
            if (mp4_ilst_item->data_type == reserved) { // for track number
                unsigned char c[4];
                memcpy(c, mp4_ilst_item->data_buf, 4);
                sprintf(str, "%lu", getMP4BoxBE32(c));
                return 1;
            } else if (mp4_ilst_item->data_type == UTF8) {
                max_size = (mp4_ilst_item->data_size <= size - 1) ? mp4_ilst_item->data_size : size - 1;
                memcpy(str, mp4_ilst_item->data_buf, max_size);
                str[max_size] = '\0';
                return 1;
            } else if (mp4_ilst_item->data_type == UTF16) {
                char _str[256*2] = {};
                max_size = (mp4_ilst_item->data_size - 3 <= 256*2-2) ? mp4_ilst_item->data_size - 3 : 256*2-2;
                memcpy(_str, &mp4_ilst_item->data_buf[3], max_size);
                std::string utf8_str = utf16_to_utf8((const char16_t *) _str);
                max_size = (utf8_str.length() <= size - 1) ? utf8_str.length() : size - 1;
                memcpy(str, utf8_str.c_str(), max_size);
                str[max_size] = '\0';
                return 1;
            }
        }
        mp4_ilst_item = mp4_ilst_item->next;
    }
    return 0;
}

int TagRead::GetMP4TypeCount(const char *mp4_type)
{
    int count = 0;
    MP4_ilst_item *mp4_ilst_item = mp4_ilst.first;
    while (mp4_ilst_item) {
        if (memcmp(mp4_ilst_item->type, mp4_type, 4) == 0) { count++; }
        mp4_ilst_item = mp4_ilst_item->next;
    }
    return count;
}

int TagRead::getMP4Picture(int idx, mime_t *mime, ptype_t *ptype, uint64_t *pos, size_t *size)
{
    int count = 0;
    MP4_ilst_item *mp4_ilst_item = mp4_ilst.first;
    while (mp4_ilst_item) {
        if (memcmp(mp4_ilst_item->type, "covr", 4) == 0) {
            if (idx == count++) {
                *mime = (mp4_ilst_item->data_type == JPEG) ? jpeg : (mp4_ilst_item->data_type == PNG) ? png : non;
                *ptype = front_cover; // no info in MP4 'covr'
                *pos = mp4_ilst_item->pos;
                *size = (size_t) mp4_ilst_item->data_size;
                return 1;
            }
        }
        mp4_ilst_item = mp4_ilst_item->next;
    }
    return 0;
}

// ========================
// MP4 parsing End
// ========================

// ========================
// FLAC handling Start
// ========================
int TagRead::getFlacMetadata(MutexFsBaseFile *file)
{
    FLAC__bool has_tags;

    // for FLAC Tags
    file->rewind();
    has_tags = FLAC__metadata_get_tags_sd_file(file, &flac_tags);

    // for FLAC Pictures
    file->rewind();
    flac_pics_count = FLAC__metadata_get_picture_count_sd_file(file, /*type*/-1, /*mime_type*/NULL, /*description*/NULL, /*max_width*/(unsigned) -1, /*max_height*/(unsigned) -1, /*max_depth*/(unsigned) -1, /*max_colors*/(unsigned) -1);
    flac_pics = (FLAC__StreamMetadata **) calloc(flac_pics_count, sizeof(FLAC__StreamMetadata *));
    for (int i = 0; i < flac_pics_count; i++) {
        file->rewind();
        //file_menu_get_obj(file_idx, &file);
        FLAC__metadata_get_picture_sd_file(file, /*idx*/i, /*picture*/&flac_pics[i], /*type*/-1, /*mime_type*/NULL, /*description*/NULL, /*max_width*/(unsigned) -1, /*max_height*/(unsigned) -1, /*max_depth*/(unsigned) -1, /*max_colors*/(unsigned) -1);
    }

    if (has_tags || flac_pics_count > 0) { return 1; }

    return 0;
}

int TagRead::GetFlacTagUTF8(const char *lc_key, size_t key_size, char *str, size_t size)
{
    if (flac_tags == NULL) return 0;
    for (int i = 0; i < (int) flac_tags->data.vorbis_comment.num_comments; i++) {
        //Serial.println((char *) flac_tags->data.vorbis_comment.comments[i].entry);
        // comment format example: "album=Schumann:Fantasiestucke, Op.12"
        char* comment_pair = (char *) flac_tags->data.vorbis_comment.comments[i].entry;
        char* ext_pos = strchr(comment_pair, '=');
        if (ext_pos != NULL && ext_pos - comment_pair < 16) {
            char key[16];
            memcpy(key, comment_pair, ext_pos - comment_pair);
            for (int j = 0; j < (int) (ext_pos - comment_pair); j++) key[j] = tolower(key[j]); // key to lower
            key[ext_pos - comment_pair] = '\0';
            char *value = ext_pos + 1;
            if (strncmp(key, lc_key, key_size) == 0) {
                //Serial.println((char *) value);
                memset(str, 0, size);
                strncpy(str, value, (strlen(value) < size - 1) ? strlen(value) : size - 1);
                return 1;
            }
        }
    }
    return 0;
}

int TagRead::GetFlacPictureCount()
{
    return flac_pics_count;
}

int TagRead::getFlacPicture(int idx, mime_t *mime, ptype_t *ptype, uint64_t *pos, size_t *size)
{
    if (idx >= flac_pics_count) { return 0; }
    if (flac_pics[idx] == NULL) { return 0; }
    if (strncmp(flac_pics[idx]->data.picture.mime_type, "image/jpeg", 10) == 0) {
        *mime = jpeg;
    } else if (strncmp(flac_pics[idx]->data.picture.mime_type, "image/png", 9) == 0) {
        *mime = png;
    } else {
        *mime = non;
    }
    *ptype = static_cast<ptype_t>(flac_pics[idx]->data.picture.type); // FLAC picture type is as same as ID3's
    *pos = flac_pics[idx]->data.picture.position;
    *size = flac_pics[idx]->data.picture.data_length;
    return 1;
}

void TagRead::flacFree()
{
    if (flac_tags) FLAC__metadata_object_delete(flac_tags);
    for (int i = 0; i < flac_pics_count; i++) {
        if (flac_pics[i]) FLAC__metadata_object_delete(flac_pics[i]);
    }
    if (flac_pics) free(flac_pics);
    flac_pics_count = 0;
}

// ========================
// FLAC handling End
// ========================