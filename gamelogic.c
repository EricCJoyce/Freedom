/*

Game logic module for the human player.

sudo docker run --rm -v $(pwd):/src -u $(id -u):$(id -g) --mount type=bind,source=$(pwd),target=/home/src c-wasm emcc -Os -s STANDALONE_WASM -s INITIAL_HEAP=1048576 -s EXPORTED_FUNCTIONS="['_getCurrentState','_getMovesBuffer','_sideToMove_client','_isWhite_client','_isBlack_client','_isEmpty_client','_isLive_client','_whiteScore_client','_blackScore_client','_anchor_client','_getMoves_client','_makeMove_client','_isTerminal_client','_isWin_client','_draw']" -Wl,--no-entry "gamelogic.c" -o "gamelogic.wasm"

*/

#include "gamestate.h"

/**************************************************************************************************
 Typedefs  */


/**************************************************************************************************
 Prototypes  */

__attribute__((import_module("env"), import_name("_printRow"))) void printRow(char a, char b, char c, char d, char e, char f, char g, char h, char i, char j, unsigned char anchor);
__attribute__((import_module("env"), import_name("_printGameStateData"))) void printGameStateData(bool wToMove, unsigned char prevMove);
unsigned char* getCurrentState(void);
unsigned char* getMovesBuffer(void);
void serialize(GameState*);
void deserialize(GameState*);

unsigned char sideToMove_client(void);
bool isWhite_client(unsigned char);
bool isBlack_client(unsigned char);
bool isEmpty_client(unsigned char);
bool isLive_client(unsigned char);
unsigned char whiteScore_client(void);
unsigned char blackScore_client(void);
unsigned char anchor_client(void);

unsigned int getMoves_client(void);
void makeMove_client(unsigned char);
bool isTerminal_client(void);
unsigned char isWin_client(void);

void draw(void);

/**************************************************************************************************
 Globals  */

unsigned char currentState[_GAMESTATE_BYTE_SIZE];                   //  Global array containing the serialized game state.
unsigned char movesBuffer[_MAX_NUM_TARGETS];                        //  Global array containing the unique destination-indices
                                                                    //  (not necessarily the number of unique moves) available.

/**************************************************************************************************
 Functions  */

/* Expose the global array declared here to JavaScript.  */
unsigned char* getCurrentState(void)
  {
    return &currentState[0];
  }

/* Expose the global array declared here to JavaScript.  */
unsigned char* getMovesBuffer(void)
  {
    return &movesBuffer[0];
  }

/* Game State Encoding & Decoding

   Byte [     0] = White, indices [ 0,  7]: [7][6][5][4][3][2][1][0]
                                             ^  ^  ^  ^  ^  ^  ^  ^
                                             |  |  |  |  |  |  |  +--- 7
                                             |  |  |  |  |  |  +------ 6
                                             |  |  |  |  |  +--------- 5
                                             |  |  |  |  +------------ 4
                                             |  |  |  +--------------- 3
                                             |  |  +------------------ 2
                                             |  +--------------------- 1
                                             +------------------------ 0
   Byte [     1] = White, indices [ 8, 15]
   Byte [     2] = White, indices [16, 23]
   Byte [     3] = White, indices [24, 31]
   Byte [     4] = White, indices [32, 39]
   Byte [     5] = White, indices [40, 47]
   Byte [     6] = White, indices [48, 55]
   Byte [     7] = White, indices [56, 63]
   Byte [     8] = White, indices [64, 71]
   Byte [     9] = White, indices [72, 78]
   Byte [    10] = White, indices [79, 85]
   Byte [    11] = White, indices [86, 93]
   Byte [    12] = White, indices [94, 99] and side to move:
                                            [7][6][5][4][3][2][1][0]
                                             ^  ^  ^  ^  ^  ^  ^  ^
                                             |  |  |  |  |  |  |  +--- reserved
                                             |  |  |  |  |  |  +------ reserved
                                             |  |  |  |  |  +--------- White, index 99
                                             |  |  |  |  +------------ White, index 98
                                             |  |  |  +--------------- White, index 97
                                             |  |  +------------------ White, index 96
                                             |  +--------------------- White, index 95
                                             +------------------------ White, index 94
   Byte [    13] = Black, indices [ 0,  7]
   Byte [    14] = Black, indices [ 8, 15]
   Byte [    15] = Black, indices [16, 23]
   Byte [    16] = Black, indices [24, 31]
   Byte [    17] = Black, indices [32, 39]
   Byte [    18] = Black, indices [40, 47]
   Byte [    19] = Black, indices [48, 55]
   Byte [    20] = Black, indices [56, 63]
   Byte [    21] = Black, indices [64, 71]
   Byte [    22] = Black, indices [72, 78]
   Byte [    23] = Black, indices [79, 85]
   Byte [    24] = Black, indices [86, 93]
   Byte [    25] = Black, indices [94, 99]:
                                            [7][6][5][4][3][2][1][0]
                                             ^  ^  ^  ^  ^  ^  ^  ^
                                             |  |  |  |  |  |  |  +--- White to move
                                             |  |  |  |  |  |  +------ reserved
                                             |  |  |  |  |  +--------- Black, index 99
                                             |  |  |  |  +------------ Black, index 98
                                             |  |  |  +--------------- Black, index 97
                                             |  |  +------------------ Black, index 96
                                             |  +--------------------- Black, index 95
                                             +------------------------ Black, index 94
   Bytes[    26] = Anchor index           */

/* Pack a GameState into the unsigned-char buffer "currentState". */
void serialize(GameState* gs)
  {
    unsigned char i, j, k, ctr, ch, mask;

    ctr = 0;                                                        //  Iterate over bytes in the encoded game-state array.

    j = 0;                                                          //  Iterate over board indices.

    for(i = 0; i < 13; i++)  /////////////////////////////////////////  Encode white stones.
      {
        mask = 128;                                                 //  Restore high bit.
        ch = 0;                                                     //  Blank out the byte.
        for(k = 0; k < 8; k++)
          {
            if(j < _NONE && isWhite(j, gs))
              ch |= mask;
            j++;
            mask >>= 1;
          }
        currentState[ctr++] = ch;
      }

    j = 0;                                                          //  Iterate over board indices.

    for(i = 0; i < 13; i++)  /////////////////////////////////////////  Encode black stones.
      {
        mask = 128;                                                 //  Restore high bit.
        ch = 0;                                                     //  Blank out the byte.
        for(k = 0; k < 8; k++)
          {
            if(j < _NONE && isBlack(j, gs))
              ch |= mask;
            j++;
            mask >>= 1;
          }
        currentState[ctr++] = ch;
      }

    if(gs->whiteToMove)  /////////////////////////////////////////////  Encode side to move.
      currentState[25] |= 1;

    currentState[26] = gs->prevMove;  ////////////////////////////////  Encode the previous move/anchor index.

    return;                                                         //  TOTAL: 27 bytes.
  }

/* Recover a GameState from the unsigned-char buffer "currentState". */
void deserialize(GameState* gs)
  {
    unsigned char i, j, k, ctr, ch, mask;

    for(j = 0; j < _NONE; j++)                                      //  Blank out.
      gs->board[j] = _EMPTY;
    gs->whiteToMove = false;
    gs->prevMove = _NONE;

    ctr = 0;                                                        //  Iterate over bytes in the encoded game-state array.

    j = 0;                                                          //  Iterate over board indices.

    for(i = 0; i < 13; i++)  /////////////////////////////////////////  Decode white stones.
      {
        ch = currentState[ctr++];                                   //  Load the byte.
        mask = 128;                                                 //  Restore high bit.
        for(k = 0; k < 8; k++)
          {
            if(j < _NONE && (ch & mask) == mask)
              gs->board[j] = _WHITE_STONE;
            j++;
            mask >>= 1;
          }
      }

    j = 0;                                                          //  Iterate over board indices.

    for(i = 0; i < 13; i++)  /////////////////////////////////////////  Decode black stones.
      {
        ch = currentState[ctr++];                                   //  Load the byte.
        mask = 128;                                                 //  Restore high bit.
        for(k = 0; k < 8; k++)
          {
            if(j < _NONE && (ch & mask) == mask)
              gs->board[j] = _BLACK_STONE;
            j++;
            mask >>= 1;
          }
      }

    if((currentState[25] & 1) == 1)  /////////////////////////////////  Decode side to move.
      gs->whiteToMove = true;

    gs->prevMove = currentState[26];  ////////////////////////////////  Decode the previous move/anchor index.
    if(gs->prevMove > _NONE)
      gs->prevMove = _NONE;

    return;                                                         //  TOTAL: 27 bytes.
  }

/* Answer the client-side question, Whose turn is it? */
unsigned char sideToMove_client(void)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return (gs.whiteToMove) ? _WHITE_TO_MOVE : _BLACK_TO_MOVE;
  }

bool isWhite_client(unsigned char index)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return isWhite(index, &gs);
  }

bool isBlack_client(unsigned char index)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return isBlack(index, &gs);
  }

bool isEmpty_client(unsigned char index)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return isEmpty(index, &gs);
  }

bool isLive_client(unsigned char index)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return isLive(index, &gs);
  }

unsigned char whiteScore_client(void)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return scoreWhite(&gs);
  }

unsigned char blackScore_client(void)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return scoreBlack(&gs);
  }

unsigned char anchor_client(void)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return gs.prevMove;
  }

bool isTerminal_client(void)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return terminal(&gs);
  }

/* Returns unsigned char in {GAME_ONGOING         = 0,
                             GAME_OVER_WHITE_WINS = 1,
                             GAME_OVER_BLACK_WINS = 2,
                             GAME_OVER_DRAW       = 3}. */
unsigned char isWin_client(void)
  {
    GameState gs;
    deserialize(&gs);                                               //  Recover GameState from buffer.
    return isWin(&gs);
  }

/* This function is intended to answer queries from the front end. */
unsigned int getMoves_client(void)
  {
    GameState gs;
    Move moves[_MAX_NUM_TARGETS];                                   //  Generous upper bound assumes that a single piece could reach half of all squares.
    unsigned int len, i;

    deserialize(&gs);                                               //  Recover GameState from buffer.
    len = getMoves(&gs, moves);

    for(i = 0; i < len; i++)
      movesBuffer[i] = moves[i].index;

    return len;
  }

/* Update "currentState" according to the given move data (if those data are indeed valid!) */
void makeMove_client(unsigned char to)
  {
    GameState gs;
    Move moves[_MAX_MOVES];                                         //  Generous assumption that every square is reachable.
    Move move;
    unsigned int len, i;

    deserialize(&gs);                                               //  Recover GameState from buffer.
    len = getMoves(&gs, moves);                                     //  Make sure that this move is legal.
    i = 0;                                                          //  Otherwise, ignore it. Cheaters lose their turns!
    while(i < len && moves[i].index != to)
      i++;
    if(i < len)
      {
        move.index = to;
        makeMove(&move, &gs);
      }

    serialize(&gs);                                                 //  Write updated GameState back to buffer.

    return;
  }

/* Draw the board to the JavaScript console.
   . . . . . . . . . .      . . . . . . . . . .
   . . . . . . . . . .      . . . . . . . . . .
   . . . . . . . . . .      . . . . . . . . . .
   . . . . . . . . . .      . . . . . . W . . .
   . . . . . . . . . .  or  . . . . . . . B . .
   . . . . . . . . . .      . . . . . .[W]. . .
   . . . . . . . . . .      . . . . . . . . . .
   . . . . . . . . . .      . . . . . . . . . .
   . . . . . . . . . .      . . . . . . . . . .
   . . . . . . . . . .      . . . . . . . . . .
   When there is freedom    When an anchor point exists  */
void draw(void)
  {
    GameState gs;
    signed char y;

    deserialize(&gs);                                               //  Recover GameState from buffer.

    for(y = 9; y >= 0; y--)
      {
        if(gs.prevMove < _NONE && (row(gs.prevMove) == (unsigned char)y))
          printRow(gs.board[y * 10], gs.board[y * 10 + 1], gs.board[y * 10 + 2], gs.board[y * 10 + 3], gs.board[y * 10 + 4], gs.board[y * 10 + 5], gs.board[y * 10 + 6], gs.board[y * 10 + 7], gs.board[y * 10 + 8], gs.board[y * 10 + 9], col(gs.prevMove) + 1);
        else
          printRow(gs.board[y * 10], gs.board[y * 10 + 1], gs.board[y * 10 + 2], gs.board[y * 10 + 3], gs.board[y * 10 + 4], gs.board[y * 10 + 5], gs.board[y * 10 + 6], gs.board[y * 10 + 7], gs.board[y * 10 + 8], gs.board[y * 10 + 9], 0);
      }

    printGameStateData(gs.whiteToMove, gs.prevMove);
    return;
  }
