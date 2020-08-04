/**
 * This file includes the implementation for a buffer. It is intended to interface with files so that they 
 *  are efficiently read. Note, all reads must be less than BUF_SIZE (hopefully significantly less).
 **/

#include "Buf.h"
#include "ReaderUtils.h"

/**
 * Function: loadN
 * Description: Checks to see if n bytes can be read from the current position (pos) in the buffer. If 
 *  there is not enough space remaining in the buffer, the bytes before pos will be discarded and BUF_SIZE
 *  data will be read from the file with the data previously pointed to by pos now at the beginning of the
 *  buffer. If there is enough space in the buffer, nothing needs to be done.
 * Assumptions: Buf has been initialized and pos points to a memory location within buf->buf
 * Side effects: The state of the buffer is updated when more data is loaded from a file
 * Output: NULL if the readLimit has been reached. Otherwise the position of the data the user requested
 **/
uint8_t *loadN(Buf *buf, uint8_t *pos, uint16_t n) {
    uint16_t dataInd = pos - buf->buf;
    if ((dataInd + n) > buf->bufSize) {
        //check if we've already finished reading
        if (buf->bytesRead == buf->readLimit) {
            return NULL;
        }

        //determine how much we have left in the buffer
        uint16_t inBufAmt = BUF_SIZE - dataInd;

        //from that determine how much new data we can read
        uint16_t newDataSize = (dataInd < (buf->readLimit - buf->bytesRead)) 
            ? dataInd : buf->readLimit - buf->bytesRead;

        //read size might not be BUF_SIZE if we are near the end of the read region
        uint16_t readSize = newDataSize + inBufAmt;

        //go to appropriate location, note we are going to reread some data so we don't have to copy
        if (fseek(buf->file, buf->filePos - inBufAmt, SEEK_SET)) {
            throwError("Invalid file read position");
        }

        //read the data into the buffer
        uint32_t readAmt = fread(buf->buf, sizeof(uint8_t), readSize, buf->file);

        if (readAmt < readSize) {
            throwError("Unable to read expected data size from file");
        }

        if ((buf->filePos = ftell(buf->file)) == -1) {
            throwError("Invalid file position after read");
        }
        
        buf->bufSize = readSize;
        //adjust our count
        buf->bytesRead += newDataSize;

        return buf->buf;
    }

    return pos;
}

/**
 * Function: getBytesRemaining
 * Description: Calculates the number of bytes remaining in the buffered section of the file based on the
 *  current position (pos)
 * Assumptions: buf has been initialized and pos points to a location within buf->buf
 * Output: Number of bytes remaining in Buf until we reach limit
 **/
uint64_t getBytesRemaining(Buf *buf, uint8_t *pos) {
    return buf->readLimit - buf->bytesRead + buf->bufSize - (pos - buf->buf);
}

/**
 * Function: createBuf
 * Description: Creates a Buf, which is a buffered section of a file. It requires a file, a position in the
 *  file that marks the beginning of the buffered position and a limit on the amount of data to read from
 *  the file. Note, limit implies a limit on the number of byte to read, not an ending position.
 * Output: A buffer for the specified portion of the file
 **/
Buf *createBuf(FILE *file, uint64_t filePos, uint64_t readLimit) {
    Buf *buf = malloc(sizeof(Buf));

    if (buf == NULL) {
        return NULL;
    }

    //setup buf
    buf->file = file;
    buf->filePos = filePos;
    buf->bytesRead = 0;
    buf->readLimit = readLimit;
    buf->bufSize = 0;

    //load information into buf
    loadN(buf, buf->buf + BUF_SIZE, 1);

    return buf;
}

/**
 * Function: freeBuf
 * Description: Frees the memory from the buf, but does not close the file.
 * Output: None
 **/
void freeBuf(Buf *buf) {
    //free the data structure
    free(buf);
}
