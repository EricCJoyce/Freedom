#ifndef __GAMESTATE_H
#define __GAMESTATE_H

#include <ctype.h>
#include <math.h>                                                   /* Needed for INFINITY and tanh. */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#define _NONE                         100

#define _WHITE_TO_MOVE                  0
#define _BLACK_TO_MOVE                  1

#define GAME_ONGOING                    0
#define GAME_OVER_WHITE_WINS            1
#define GAME_OVER_BLACK_WINS            2
#define GAME_OVER_DRAW                  3

#define _EMPTY                       0x00
#define _WHITE_STONE                 0x01
#define _BLACK_STONE                 0x02

#define PLANE_OWN                       0
#define PLANE_ENEMY                     1
#define PLANE_PREVIOUS                  2
#define PLANE_LEGAL                     3
#define PLANE_OWN_FOUR                  4
#define PLANE_ENEMY_FOUR                5
#define PLANE_OWN_OVER                  6
#define PLANE_ENEMY_OVER                7
#define FEATURE(buffer, plane, index) (buffer[(plane) * _NONE + (index)])
#define INTERPRETATION_VECTOR_LENGTH  800                           /* Eight 100-square boards. */

#define _GAMESTATE_BYTE_SIZE           27                           /* Number of bytes needed to store a GameState structure. */
#define _MOVE_BYTE_SIZE                 1                           /* Number of bytes needed to store a Move structure. */
#define _MAX_NUM_TARGETS              100                           /* A tight upper bound on how many distinct indices may be available to a team in a single turn. */
#define _MAX_MOVES                    100                           /* A tight upper bound on how many moves may be available to a team in a single turn. */

/**************************************************************************************************
 Typedefs  */

typedef struct GameStateType                                        //  TOTAL: 102 bytes.
  {
    bool whiteToMove;                                               //  True: white to move. False: black to move.
    unsigned char board[_NONE];                                     //  Array of characters.
    unsigned char prevMove;                                         //  The index around which a stone may be placed.
                                                                    //  If this is _NONE, then any unoccupied index is available (as at the beginning of the game).
  } GameState;

typedef struct MoveType                                             //  TOTAL: 1 byte.
  {
    unsigned char index;                                            //  Index in [0, 100).
  } Move;

/**************************************************************************************************
 Prototypes  */

void copyGameState(GameState*, GameState*);

void makeMove(Move*, GameState*);
void makeNullMove(GameState*);
unsigned int getMoves(GameState*, Move*);

unsigned int scoreWhite(GameState*);
unsigned int scoreBlack(GameState*);
unsigned char isWin(GameState*);
bool terminal(GameState*);

unsigned int interpret(GameState*, float*);

bool isEmpty(unsigned char, GameState*);
bool isWhite(unsigned char, GameState*);
bool isBlack(unsigned char, GameState*);
bool sameSide(unsigned char, unsigned char, GameState*);
bool opposed(unsigned char, unsigned char, GameState*);
bool isNeighbor(unsigned char, unsigned char);
bool isLive(unsigned char, GameState*);

unsigned char vertSet(unsigned char, bool, GameState*, unsigned char*);
unsigned char horzSet(unsigned char, bool, GameState*, unsigned char*);
unsigned char dlurSet(unsigned char, bool, GameState*, unsigned char*);
unsigned char uldrSet(unsigned char, bool, GameState*, unsigned char*);

unsigned char u(unsigned char);
unsigned char d(unsigned char);
unsigned char l(unsigned char);
unsigned char r(unsigned char);
unsigned char ul(unsigned char);
unsigned char ur(unsigned char);
unsigned char dl(unsigned char);
unsigned char dr(unsigned char);
unsigned char row(unsigned char);
unsigned char col(unsigned char);

/**************************************************************************************************
 Globals  */


/**************************************************************************************************
 Functions  */

void copyGameState(GameState* src, GameState* dst)
  {
    unsigned char i;

    dst->whiteToMove = src->whiteToMove;

    for(i = 0; i < _NONE; i++)
      dst->board[i] = src->board[i];

    dst->prevMove = src->prevMove;

    return;
  }

/**************************************************************************************************
 Move generation and Application  */

void makeMove(Move* move, GameState* gs)
  {
    unsigned char freedomCtr = 0;
                                                                    //  Plant the stone.
    gs->board[move->index] = gs->whiteToMove ? _WHITE_TO_MOVE : _BLACK_TO_MOVE;
                                                                    //  Now sweep the area: everything occupied?
    if(u(move->index) < _NONE && isEmpty(u(move->index), gs))
      freedomCtr++;
    if(ur(move->index) < _NONE && isEmpty(ur(move->index), gs))
      freedomCtr++;
    if(r(move->index) < _NONE && isEmpty(r(move->index), gs))
      freedomCtr++;
    if(dr(move->index) < _NONE && isEmpty(dr(move->index), gs))
      freedomCtr++;
    if(d(move->index) < _NONE && isEmpty(d(move->index), gs))
      freedomCtr++;
    if(dl(move->index) < _NONE && isEmpty(dl(move->index), gs))
      freedomCtr++;
    if(l(move->index) < _NONE && isEmpty(l(move->index), gs))
      freedomCtr++;
    if(ul(move->index) < _NONE && isEmpty(ul(move->index), gs))
      freedomCtr++;

    if(freedomCtr == 0)                                             //  Nothing around previous move? Total freedom.
      gs->prevMove = _NONE;
    else
      gs->prevMove = move->index;

    gs->whiteToMove = !gs->whiteToMove;                             //  Flip the flag.

    return;
  }

void makeNullMove(GameState* gs)
  {
    unsigned char freedomCtr = 0;
                                                                    //  Now sweep the area: everything occupied?
    if(u(gs->prevMove) < _NONE && isEmpty(u(gs->prevMove), gs))
      freedomCtr++;
    if(ur(gs->prevMove) < _NONE && isEmpty(ur(gs->prevMove), gs))
      freedomCtr++;
    if(r(gs->prevMove) < _NONE && isEmpty(r(gs->prevMove), gs))
      freedomCtr++;
    if(dr(gs->prevMove) < _NONE && isEmpty(dr(gs->prevMove), gs))
      freedomCtr++;
    if(d(gs->prevMove) < _NONE && isEmpty(d(gs->prevMove), gs))
      freedomCtr++;
    if(dl(gs->prevMove) < _NONE && isEmpty(dl(gs->prevMove), gs))
      freedomCtr++;
    if(l(gs->prevMove) < _NONE && isEmpty(l(gs->prevMove), gs))
      freedomCtr++;
    if(ul(gs->prevMove) < _NONE && isEmpty(ul(gs->prevMove), gs))
      freedomCtr++;

    if(freedomCtr == 0)                                             //  Nothing around previous move? Total freedom.
      gs->prevMove = _NONE;
    else
      gs->prevMove = gs->prevMove;

    gs->whiteToMove = !gs->whiteToMove;                             //  Flip the flag.

    return;
  }

unsigned int getMoves(GameState* gs, Move* buffer)
  {
    unsigned int len = 0;
    unsigned char i;

    if(gs->prevMove == _NONE)                                       //  Total freedom.
      {
        for(i = 0; i < _NONE; i++)
          {
            if(isEmpty(i, gs))
              {
                buffer[len].index = i;
                len++;
              }
          }
      }
    else                                                            //  Constrained by the previous move.
      {
        if(u(gs->prevMove) < _NONE && isEmpty(u(gs->prevMove), gs)) //  U
          {
            buffer[len].index = u(gs->prevMove);
            len++;
          }
                                                                    //  UR
        if(ur(gs->prevMove) < _NONE && isEmpty(ur(gs->prevMove), gs))
          {
            buffer[len].index = ur(gs->prevMove);
            len++;
          }
        if(r(gs->prevMove) < _NONE && isEmpty(r(gs->prevMove), gs)) //  R
          {
            buffer[len].index = r(gs->prevMove);
            len++;
          }
                                                                    //  DR
        if(dr(gs->prevMove) < _NONE && isEmpty(dr(gs->prevMove), gs))
          {
            buffer[len].index = dr(gs->prevMove);
            len++;
          }
        if(d(gs->prevMove) < _NONE && isEmpty(d(gs->prevMove), gs)) //  D
          {
            buffer[len].index = d(gs->prevMove);
            len++;
          }
                                                                    //  DL
        if(dl(gs->prevMove) < _NONE && isEmpty(dl(gs->prevMove), gs))
          {
            buffer[len].index = dl(gs->prevMove);
            len++;
          }
        if(l(gs->prevMove) < _NONE && isEmpty(l(gs->prevMove), gs)) //  L
          {
            buffer[len].index = l(gs->prevMove);
            len++;
          }
                                                                    //  UL
        if(ul(gs->prevMove) < _NONE && isEmpty(ul(gs->prevMove), gs))
          {
            buffer[len].index = ul(gs->prevMove);
            len++;
          }
      }

    return len;
  }

/**************************************************************************************************
 Scoring and Terminal Criteria  */

unsigned int scoreWhite(GameState* gs)
  {
    unsigned int score = 0;
    unsigned char i;
    for(i = 0; i < _NONE; i++)
      {
        if(isWhite(i, gs) && isLive(i, gs))
          score++;
      }
    return score;
  }

unsigned int scoreBlack(GameState* gs)
  {
    unsigned int score = 0;
    unsigned char i;
    for(i = 0; i < _NONE; i++)
      {
        if(isBlack(i, gs) && isLive(i, gs))
          score++;
      }
    return score;
  }

/* Given a game state, return:
     GAME_ONGOING          if the state is not yet terminal.
     GAME_OVER_WHITE_WINS  if the state is terminal and white leads in points.
     GAME_OVER_BLACK_WINS  if the state is terminal and black leads in points.
     GAME_OVER_DRAW        if the state is terminal and white and black are tied for points.

   In FREEDOM, black will always play last and has the right to abstain from placing a stone if doing to would harm black's score. */
unsigned char isWin(GameState* gs)
  {
    unsigned char i;
    unsigned char len = 0;                                          //  Count up the number of empty cells.
    unsigned char empty = _NONE;                                    //  If there is a single empty cell, its index is here.
    unsigned int wScore, bScore, originalbScore;
    GameState tmp;
    Move mv;

    for(i = 0; i < _NONE; i++)
      {
        if(isEmpty(i, gs))
          {
            len++;                                                  //  Increase the number of empty cells.
            empty = i;                                              //  Save the previous empty cell's index.
          }
      }

    if(len == 0)                                                    //  If the board is filled up, then the game is over.
      {
        wScore = scoreWhite(gs);
        bScore = scoreBlack(gs);
        if(wScore > bScore)
          return GAME_OVER_WHITE_WINS;
        if(wScore < bScore)
          return GAME_OVER_BLACK_WINS;
        return GAME_OVER_DRAW;
      }
    else if(len == 1)                                               //  If a single empty cell remains, determine whether playing into it would harm black.
      {
        originalbScore = scoreBlack(gs);                            //  Save black's current, real score.
        copyGameState(gs, &tmp);                                    //  Copy the given game state.
        mv.index = empty;                                           //  Prepare to place a black stone into the single empty space.
        makeMove(&mv, &tmp);                                        //  Place the last black stone into the hypothetical board's single empty space.
        bScore = scoreBlack(&tmp);                                  //  Compute what black's score would be if this last stone were played.
        if(originalbScore > bScore)                                 //  Placing this stone WOULD harm black's score. Therefore, the game is over NOW.
          {
            wScore = scoreWhite(gs);
            bScore = scoreBlack(gs);
            if(wScore > bScore)
              return GAME_OVER_WHITE_WINS;
            if(wScore < bScore)
              return GAME_OVER_BLACK_WINS;
            return GAME_OVER_DRAW;
          }
      }

    return GAME_ONGOING;
  }

bool terminal(GameState* gs)
  {
    unsigned char win;
    win = isWin(gs);
    return (win != GAME_ONGOING);
  }

/**************************************************************************************************
 Deep network input

 Eight 10 x 10 planes:
   1. Own stones
   2. Opponent stones
   3. Previous move
   4. Currently legal placements
   5. Own stones belonging to at least one exact-four
   6. Opponent stones belonging to at least one exact-four
   7. Own stones belonging to a run of five or more
   8. Opponent stones belonging to a run of five or more
*/
unsigned int interpret(GameState* gs, float* buffer)
  {
    unsigned char index, piece, i;
    unsigned char set[10];
    unsigned char runs[4];
    bool own, white;

    const unsigned char mover = gs->whiteToMove ? _WHITE_STONE : _BLACK_STONE;
    const unsigned char previous = gs->prevMove;
    const bool freedom = (gs->prevMove == _NONE);                   //  Determine this once.

    memset(buffer, 0, sizeof(float) * INTERPRETATION_VECTOR_LENGTH);

    for(index = 0; index < _NONE; index++)
      {
        piece = gs->board[index];
        white = (piece == _WHITE_STONE);

        if(index == previous)                                       //  Write to the previous-move plane.
          FEATURE(buffer, PLANE_PREVIOUS, index) = 1.0f;

        if(piece == _EMPTY)                                         //  Empty squares: only the legal-placement plane can contain anything here.
          {
            if(freedom || isNeighbor(index, previous))
              FEATURE(buffer, PLANE_LEGAL, index) = 1.0f;
            continue;
          }

        own = (piece == mover);                                     //  Prepare to address the occupancy plane.
        FEATURE(buffer, own ? PLANE_OWN : PLANE_ENEMY, index) = 1.0f;
                                                                    //  Scoring-status planes.
        runs[0] = vertSet(index, white, gs, set);
        runs[1] = horzSet(index, white, gs, set);
        runs[2] = dlurSet(index, white, gs, set);
        runs[3] = uldrSet(index, white, gs, set);
        for(i = 0; i < 4; i++)
          {
            if(runs[i] == 4)                                        //  Live stone.
              FEATURE(buffer, own ? PLANE_OWN_FOUR : PLANE_ENEMY_FOUR, index) = 1.0f;
            else if(runs[i] > 4)                                    //  Spoiled stone.
              FEATURE(buffer, own ? PLANE_OWN_OVER : PLANE_ENEMY_OVER, index) = 1.0f;
          }
      }

    return INTERPRETATION_VECTOR_LENGTH;
  }

/**************************************************************************************************
 Identities and Tests  */

/*  Is the given index i vacant? */
bool isEmpty(unsigned char i, GameState* gs)
  {
    return (gs->board[i] == _EMPTY);
  }

/*  Is the given index i occupied by a white stone? */
bool isWhite(unsigned char i, GameState* gs)
  {
    return (gs->board[i] == _WHITE_STONE);
  }

/*  Is the given index i occupied by a black stone? */
bool isBlack(unsigned char i, GameState* gs)
  {
    return (gs->board[i] == _BLACK_STONE);
  }

/*  Are the board contents at index i the same as at index j? */
bool sameSide(unsigned char i, unsigned char j, GameState* gs)
  {
    return ((isWhite(i, gs) && isWhite(j, gs)) || (isBlack(i, gs) && isBlack(j, gs)));
  }

/*  Are the board contents at index i and at index j on opposite teams? */
bool opposed(unsigned char i, unsigned char j, GameState* gs)
  {
    return ((isWhite(i, gs) && isBlack(j, gs)) || (isBlack(i, gs) && isWhite(j, gs)));
  }

/* Is index j one of the (at most)8-neighbors of index i? */
bool isNeighbor(unsigned char i, unsigned char j)
  {
    return (u(i) == j || ur(i) == j || r(i) == j || dr(i) == j || d(i) == j || dl(i) == j || l(i) == j || ul(i) == j);
  }

/*  Is the stone at given index i alive? */
bool isLive(unsigned char i, GameState* gs)
  {
    unsigned char set[10];
    unsigned char setlen;

    if(!isEmpty(i, gs))
      {
        setlen = vertSet(i, isWhite(i, gs), gs, set);
        if(setlen == 4)
          return true;

        setlen = horzSet(i, isWhite(i, gs), gs, set);
        if(setlen == 4)
          return true;

        setlen = dlurSet(i, isWhite(i, gs), gs, set);
        if(setlen == 4)
          return true;

        setlen = uldrSet(i, isWhite(i, gs), gs, set);
        if(setlen == 4)
          return true;
      }

    return false;
  }

/**************************************************************************************************
 Set builders
   index:  starting position
   white:  true = grow along white stones
           false = grow along black stones
   buffer: provided by the parent function, the set built ends up here, with length returned */

/* Build list of squares vertically from given index */
unsigned char vertSet(unsigned char index, bool white, GameState* gs, unsigned char* buffer)
  {
    unsigned char len = 0;
    unsigned char dst;

    buffer[len++] = index;                                          //  Include index

    dst = u(index);                                                 //  Scan up.
    while(dst < _NONE)
      {
        if(isWhite(dst, gs) && white)
          {
            buffer[len++] = dst;
            dst = u(dst);
          }
        else if(isBlack(dst, gs) && !white)
          {
            buffer[len++] = dst;
            dst = u(dst);
          }
        else
          break;
      }

    dst = d(index);                                                 //  Scan down.
    while(dst < _NONE)
      {
        if(isWhite(dst, gs) && white)
          {
            buffer[len++] = dst;
            dst = d(dst);
          }
        else if(isBlack(dst, gs) && !white)
          {
            buffer[len++] = dst;
            dst = d(dst);
          }
        else
          break;
      }

    return len;
  }

/* Build list of squares horizontally from given index */
unsigned char horzSet(unsigned char index, bool white, GameState* gs, unsigned char* buffer)
  {
    unsigned char len = 0;
    unsigned char dst;

    buffer[len++] = index;                                          //  Include index

    dst = l(index);                                                 //  Scan left.
    while(dst < _NONE)
      {
        if(isWhite(dst, gs) && white)
          {
            buffer[len++] = dst;
            dst = l(dst);
          }
        else if(isBlack(dst, gs) && !white)
          {
            buffer[len++] = dst;
            dst = l(dst);
          }
        else
          break;
      }

    dst = r(index);                                                 //  Scan right.
    while(dst < _NONE)
      {
        if(isWhite(dst, gs) && white)
          {
            buffer[len++] = dst;
            dst = r(dst);
          }
        else if(isBlack(dst, gs) && !white)
          {
            buffer[len++] = dst;
            dst = r(dst);
          }
        else
          break;
      }

    return len;
  }

/* Build list of squares diagonally, lower-left, upper-right, from given index */
unsigned char dlurSet(unsigned char index, bool white, GameState* gs, unsigned char* buffer)
  {
    unsigned char len = 0;
    unsigned char dst;

    buffer[len++] = index;                                          //  Include index

    dst = dl(index);                                                //  Scan down-left.
    while(dst < _NONE)
      {
        if(isWhite(dst, gs) && white)
          {
            buffer[len++] = dst;
            dst = dl(dst);
          }
        else if(isBlack(dst, gs) && !white)
          {
            buffer[len++] = dst;
            dst = dl(dst);
          }
        else
          break;
      }

    dst = ur(index);                                                //  Scan up-right.
    while(dst < _NONE)
      {
        if(isWhite(dst, gs) && white)
          {
            buffer[len++] = dst;
            dst = ur(dst);
          }
        else if(isBlack(dst, gs) && !white)
          {
            buffer[len++] = dst;
            dst = ur(dst);
          }
        else
          break;
      }

    return len;
  }

/* Build list of squares diagonally, upper-left, lower-right, from given index */
unsigned char uldrSet(unsigned char index, bool white, GameState* gs, unsigned char* buffer)
  {
    unsigned char len = 0;
    unsigned char dst;

    buffer[len++] = index;                                          //  Include index

    dst = ul(index);                                                //  Scan up-left.
    while(dst < _NONE)
      {
        if(isWhite(dst, gs) && white)
          {
            buffer[len++] = dst;
            dst = ul(dst);
          }
        else if(isBlack(dst, gs) && !white)
          {
            buffer[len++] = dst;
            dst = ul(dst);
          }
        else
          break;
      }

    dst = dr(index);                                                //  Scan down-right.
    while(dst < _NONE)
      {
        if(isWhite(dst, gs) && white)
          {
            buffer[len++] = dst;
            dst = dr(dst);
          }
        else if(isBlack(dst, gs) && !white)
          {
            buffer[len++] = dst;
            dst = dr(dst);
          }
        else
          break;
      }

    return len;
  }

/**************************************************************************************************
 Board logic  */

/*  Return the index UP from the given i */
unsigned char u(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i + 10) == row(i) + 1)
          return i + 10;
      }
    return _NONE;
  }

/*  Return the index DOWN from the given i */
unsigned char d(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i - 10) == row(i) - 1 && row(i) != _NONE)
          return i - 10;
      }
    return _NONE;
  }

/*  Return the index LEFT from the given i */
unsigned char l(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i - 1) == row(i))
          return i - 1;
      }
    return _NONE;
  }

/*  Return the index RIGHT from the given i */
unsigned char r(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i + 1) == row(i))
          return i + 1;
      }
    return _NONE;
  }

/*  Return the index UP-LEFT from the given i */
unsigned char ul(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i + 9) == row(i) + 1)
          return i + 9;
      }
    return _NONE;
  }

/*  Return the index UP-RIGHT from the given i */
unsigned char ur(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i + 11) == row(i) + 1)
          return i + 11;
      }
    return _NONE;
  }

/*  Return the index DOWN-LEFT from the given i */
unsigned char dl(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i - 11) == row(i) - 1 && row(i) != _NONE)
          return i - 11;
      }
    return _NONE;
  }

/*  Return the index DOWN-RIGHT from the given i */
unsigned char dr(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i - 9) == row(i) - 1 && row(i) != _NONE)
          return i - 9;
      }
    return _NONE;
  }

/*  Compute the COLUMN in which given index is included */
unsigned char col(unsigned char i)
  {
    if(i < _NONE)
      return i % 10;
    return _NONE;
  }

/*  Compute the ROW in which given index is included */
unsigned char row(unsigned char i)
  {
    if(i < _NONE)
      return (i - (i % 10)) / 10;
    return _NONE;
  }

#endif
