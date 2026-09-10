#ifndef __TRANSPOSITION_H
#define __TRANSPOSITION_H

#include <string.h>                                                 /* Needed for memcpy(). */

#define _MOVE_BYTE_SIZE                1                            /* Number of bytes needed to store a Move structure. */
#define _NONE                        100                            /* Don't include gamestate just to get a constant. */

#define _TRANSPO_RECORD_BYTE_SIZE     16                            /* Number of bytes needed to store a TranspoRecord object. */
#define _TRANSPO_TABLE_SIZE       524288                            /* Number of TranspoRecords, each _TRANSPO_RECORD_BYTE_SIZE bytes.
                                                                       524,288 = 2^19.
                                                                       1 + _TRANSPO_TABLE_SIZE * _TRANSPO_RECORD_BYTE_SIZE == 8 MB,
                                                                       deemed sensible for Mobile/Tablets */
#define _TRANSPO_AGE_THRESHOLD        40                            /* Old enough to be replaced. */

#define NODE_TYPE_NONE                 0                            /* No entry. */
#define NODE_TYPE_PV                   1                            /* Score is exact. */
#define NODE_TYPE_ALL                  2                            /* Score is an upper bound. */
#define NODE_TYPE_CUT                  3                            /* Score is a lower bound. */

/**************************************************************************************************
 Typedefs  */

typedef struct TranspoRecordType                                    //  TOTAL: 16 = _TRANSPO_RECORD_BYTE_SIZE
  {
    unsigned long long lock;                                        //  (8 bytes) A copy of the Zobrist hash to match against.
                                                                    //            (MUCH cheaper than storing the game state's bytes!)
    unsigned char bestMove[_MOVE_BYTE_SIZE];                        //  (1 byte)  The best move for this record, stored as a byte array.
    signed char depth;                                              //  (1 byte)  Search depth ratifying this record.
                                                                    //            Positive = ordinary search;
                                                                    //            0/-1/... = successive quiescence depths.
    float score;                                                    //  (4 bytes) 32 bits are plenty.
    unsigned char type;                                             //  (1 byte)  In {NODE_TYPE_NONE, NODE_TYPE_PV, NODE_TYPE_ALL, NODE_TYPE_CUT} as
                                                                    //            score is exact, an upper bound, or lower bound, respectively.
    unsigned char age;                                              //  (1 byte)  Used to determine when a record should be removed.
                                                                    //            If age == 0, then this entry is free to be overwritten.
  } TranspoRecord;

/**************************************************************************************************
 Prototypes  */

bool fetchRecord(unsigned int, TranspoRecord*);
unsigned char getGeneration(void);
void incGeneration(void);
unsigned int hashIndex(unsigned long long);
void serializeTranspoRecord(TranspoRecord*, unsigned char*);
void deserializeTranspoRecord(unsigned char*, TranspoRecord*);

/**************************************************************************************************
 Globals  */
                                                                    //  8,388,609 bytes = 1 + 524,288 * 16.
                                                                    //  Global array containing the serialized transposition table:
                                                                    //  Generation-Byte + sizeof(TranspoRecord) * size-of-table.
unsigned char transpositionTableBuffer[1 + _TRANSPO_TABLE_SIZE * _TRANSPO_RECORD_BYTE_SIZE];

/**************************************************************************************************
 Functions  */

/* Load whatever is at the given index and return whether its 'age' field is greater than zero. */
bool fetchRecord(unsigned int index, TranspoRecord* ttRecord)
  {
    deserializeTranspoRecord(1 + transpositionTableBuffer + index * _TRANSPO_RECORD_BYTE_SIZE, ttRecord);
    return (ttRecord->age > 0);
  }

/* The first byte of the transposition table's byte array is the "generation" for all records created at a given time. */
unsigned char getGeneration(void)
  {
    return transpositionTableBuffer[0];
  }

/* Enforce wrap-around from 255 to 1. */
void incGeneration(void)
  {
    if(transpositionTableBuffer[0] == 255)
      {
                                                                    //  At roll-over, nuke the table.
        memset(transpositionTableBuffer + 1, 0, sizeof(transpositionTableBuffer) - 1);
        transpositionTableBuffer[0] = 1;                            //  Roll over to 1 because 0 means "empty slot".
      }
    else
      transpositionTableBuffer[0]++;
    return;
  }

/* Return an index into the transposition table buffer. */
unsigned int hashIndex(unsigned long long h)
  {
    return h % _TRANSPO_TABLE_SIZE;
  }

/* Encode the given TranspoRecord to the given byte array.
   When "buffer" is the global byte array "transpositionTableBuffer", it is at an offset into that array, treated locally. */
void serializeTranspoRecord(TranspoRecord* ttRecord, unsigned char* buffer)
  {
    unsigned int i = 0;

    memcpy(buffer + i, &ttRecord->lock, 8);                         //  Copy lock, as bytes, to serial buffer.
    i += 8;

    memcpy(buffer + i, ttRecord->bestMove, _MOVE_BYTE_SIZE);        //  Copy best-move's byte array to the serial buffer.
    i += _MOVE_BYTE_SIZE;

    memcpy(buffer + i, &ttRecord->depth, 1);                        //  Copy the depth to the serial buffer.
    i++;

    memcpy(buffer + i, &ttRecord->score, 4);                        //  Copy score, as bytes, to serial buffer.
    i += 4;

    buffer[i++] = ttRecord->type;                                   //  Copy the node's type to the serial buffer.
    buffer[i++] = ttRecord->age;                                    //  Copy the node's age to the serial buffer.

    return;
  }

/* Decode, from the given byte array, a TranspoRecord.
   When "buffer" is the global byte array "transpositionTableBuffer", it is at an offset into that array, treated locally. */
void deserializeTranspoRecord(unsigned char* buffer, TranspoRecord* ttRecord)
  {
    unsigned int i = 0;

    memcpy(&ttRecord->lock, buffer + i, 8);                         //  Restore the lock to the TranspoRecord.
    i += 8;

    memcpy(ttRecord->bestMove, buffer + i, _MOVE_BYTE_SIZE);        //  Restore bestMove to the TranspoRecord.
    i += _MOVE_BYTE_SIZE;

    memcpy(&ttRecord->depth, buffer + i, 1);                        //  Restore depth to the TranspoRecord.
    i++;

    memcpy(&ttRecord->score, buffer + i, 4);                        //  Restore score to the TranspoRecord.
    i += 4;

    ttRecord->type = buffer[i++];                                   //  Restore type to the TranspoRecord.
    ttRecord->age = buffer[i++];                                    //  Restore age to the TranspoRecord.

    return;
  }

#endif