/*

sudo docker run --rm -v $(pwd):/src -u $(id -u):$(id -g) --mount type=bind,source=$(pwd),target=/home/src c-wasm em++ -I ./ -O3 -msimd128 -I/usr/include/eigen3 -DEIGEN_NO_DEBUG -DEIGEN_DONT_PARALLELIZE -s STANDALONE_WASM -s EXPORTED_FUNCTIONS="['_getInputGameStateBuffer','_getInputMoveBuffer','_getOutputGameStateBuffer','_getOutputMovesBuffer','_getWeightsBuffer','_weightsBufferSize','_sideToMove_eval','_isTerminal_eval','_makeMove_eval','_makeNullMove_eval','_evaluate_eval','_getMoves_eval']" -Wl,--no-entry zinn.cpp -o eval.wasm

*/

#include <Eigen/Dense>
#include <math.h>
#include <stdint.h>
#include "gamestate.h"

#define ZINN_INPUT_PLANES              8
#define ZINN_SQUARES                 100
#define ZINN_CHANNELS                 32
#define ZINN_ACTIVATIONS            3200
#define ZINN_HIDDEN                   64
#define ZINN_BLOCKS                    4

#define ZINN_PARAMETER_FLOATS     216225
#define ZINN_PARAMETER_BYTES      (ZINN_PARAMETER_FLOATS * sizeof(float))

#define ZINN_STEM_WEIGHT_OFFSET        0
#define ZINN_STEM_BIAS_OFFSET        256

#define ZINN_BLOCK0_OFFSET           288
#define ZINN_BLOCK_FLOATS           2752

#define ZINN_HIDDEN_WEIGHT_OFFSET  11296
#define ZINN_HIDDEN_BIAS_OFFSET   216096

#define ZINN_VALUE_WEIGHT_OFFSET  216160
#define ZINN_VALUE_BIAS_OFFSET    216224

#define MOVEFLAG_QUIET                 0                            /* The move is quiet. */
#define MOVEFLAG_NOISY                 1                            /* The move is noisy. */

/**************************************************************************************************
 Typedefs  */


/**************************************************************************************************
 Prototypes  */

extern "C"
  {
    unsigned char* getInputGameStateBuffer(void);
    unsigned char* getInputMoveBuffer(void);
    unsigned char* getOutputGameStateBuffer(void);
    unsigned char* getOutputMovesBuffer(void);
    uintptr_t getWeightsBuffer(void);
    unsigned int weightsBufferSize(void);

    unsigned char sideToMove_eval(void);
    bool isTerminal_eval(void);
    void makeMove_eval(void);
    void makeNullMove_eval(void);
    float evaluate_eval(void);
    unsigned int getMoves_eval(void);
  }

void serializeGameStateToBuffer(GameState*, unsigned char*);
void serializeMoveToBuffer(Move*, unsigned char*);
void deserializeGameState(GameState*);
void deserializeMove(Move*);

static float zinnForward(const float*);
static void zinnStem(const float*, float*);
static void zinnDepthwise3x3(const float*, float*, const float*, const float*);
static void zinnPointwise(const float*, float*, const float*, const float*);
static void zinnPointwiseResidual(const float*, float*, const float*, const float*, const float*);
static void zinnHiddenLayer(const float*);
static float zinnValueHead(void);

/**************************************************************************************************
 Globals  */

unsigned char inputGameStateBuffer[_GAMESTATE_BYTE_SIZE];           //  Global array containing the serialized INPUT game state.

unsigned char inputMoveBuffer[_MOVE_BYTE_SIZE];                     //  Global array containing the serialized INPUT move.

unsigned char outputGameStateBuffer[_GAMESTATE_BYTE_SIZE];          //  Global array containing the serialized OUTPUT game state.

                                                                    //  Global array containing up to _MAX_MOVES moves.
                                                                    //  Rather than encode the number of moves in the array itself, we return an integer.
                                                                    //  Each move is represented as a byte sub-array encoding:
                                                                    //    _MOVE_BYTE_SIZE  :  bytes encoding a single move,
                                                                    //    4                :  bytes for signed integer, which is rough score.
unsigned char outputMovesBuffer[_MAX_MOVES * (_MOVE_BYTE_SIZE + 5)];//    1                :  byte (should be Boolean) indicating whether move is "quiet".

alignas(16) static float zinnWeights[ZINN_PARAMETER_FLOATS];

alignas(16) static float zinnInput[800];

alignas(16) static float zinnActivationA[ZINN_ACTIVATIONS];
alignas(16) static float zinnActivationB[ZINN_ACTIVATIONS];
alignas(16) static float zinnActivationC[ZINN_ACTIVATIONS];

alignas(16) static float zinnHidden[ZINN_HIDDEN];

/**************************************************************************************************
 Using  */

using InputMatrix = Eigen::Matrix<float, 8, 100, Eigen::RowMajor>;
using ActivationMatrix = Eigen::Matrix<float, 32, 100, Eigen::RowMajor>;
using StemWeightMatrix = Eigen::Matrix<float, 32, 8, Eigen::RowMajor>;
using PointwiseWeightMatrix = Eigen::Matrix<float, 32, 32, Eigen::RowMajor>;
using Bias32 = Eigen::Matrix<float, 32, 1>;
using HiddenWeightMatrix = Eigen::Matrix<float, 64, 3200, Eigen::RowMajor>;
using HiddenVector = Eigen::Matrix<float, 64, 1>;
using FlatVector = Eigen::Matrix<float, 3200, 1>;

/**************************************************************************************************
 Functions  */

/* Expose the global array declared here to JavaScript.  */
unsigned char* getInputGameStateBuffer(void)
  {
    return &inputGameStateBuffer[0];
  }

/* Expose the global array declared here to JavaScript.  */
unsigned char* getInputMoveBuffer(void)
  {
    return &inputMoveBuffer[0];
  }

/* Expose the global array declared here to JavaScript.  */
unsigned char* getOutputGameStateBuffer(void)
  {
    return &outputGameStateBuffer[0];
  }

/* Expose the global array declared here to JavaScript.  */
unsigned char* getOutputMovesBuffer(void)
  {
    return &outputMovesBuffer[0];
  }

uintptr_t getWeightsBuffer(void)
  {
    return (uintptr_t)zinnWeights;
  }

unsigned int weightsBufferSize(void)
  {
    return ZINN_PARAMETER_BYTES;
  }

/* Write the given game state to the given buffer. */
void serializeGameStateToBuffer(GameState* gs, unsigned char* buffer)
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
        buffer[ctr++] = ch;
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
        buffer[ctr++] = ch;
      }

    if(gs->whiteToMove)  /////////////////////////////////////////////  Encode side to move.
      buffer[25] |= 1;

    buffer[26] = gs->prevMove;  //////////////////////////////////////  Encode the previous move/anchor index.

    return;                                                         //  TOTAL: 27 bytes.
  }

/* Write the given move to the given buffer. */
void serializeMoveToBuffer(Move* move, unsigned char* buffer)
  {
    buffer[0] = move->index;
    return;
  }

/* Recover a GameState from the unsigned-char buffer "inputGameStateBuffer". */
void deserializeGameState(GameState* gs)
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
        ch = inputGameStateBuffer[ctr++];                           //  Load the byte.
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
        ch = inputGameStateBuffer[ctr++];                           //  Load the byte.
        mask = 128;                                                 //  Restore high bit.
        for(k = 0; k < 8; k++)
          {
            if(j < _NONE && (ch & mask) == mask)
              gs->board[j] = _BLACK_STONE;
            j++;
            mask >>= 1;
          }
      }

    if((inputGameStateBuffer[25] & 1) == 1)  /////////////////////////  Decode side to move.
      gs->whiteToMove = true;

    gs->prevMove = inputGameStateBuffer[26];  ////////////////////////  Decode the previous move/anchor index.
    if(gs->prevMove > _NONE)
      gs->prevMove = _NONE;

    return;                                                         //  TOTAL: 27 bytes.
  }

/* Recover a Move from the unsigned-char buffer "inputMoveBuffer". */
void deserializeMove(Move* move)
  {
    move->index = inputMoveBuffer[0];
    return;
  }

/* Answer the Negamax Module's query, "Which side is to move in the GameState in the query buffer?"
   Return an unsigned char in {_WHITE_TO_MOVE, _BLACK_TO_MOVE}. */
unsigned char sideToMove_eval(void)
  {
    GameState gs;
    deserializeGameState(&gs);                                      //  Recover GameState from buffer.
    return gs.whiteToMove ? _WHITE_TO_MOVE : _BLACK_TO_MOVE;
  }

/* Answer the Negamax Module's query, "Is the GameState in the query buffer terminal?" */
bool isTerminal_eval(void)
  {
    GameState gs;
    deserializeGameState(&gs);                                      //  Recover GameState from buffer.
    return terminal(&gs);
  }

/* Answer the Negamax Module's query, "What GameState results from making the move in the input-move buffer in the game state in the input-gamestate buffer?"
   Writes to "outputGameStateBuffer". */
void makeMove_eval(void)
  {
    GameState gs;
    Move move;

    deserializeGameState(&gs);                                      //  Recover GameState from buffer.
    deserializeMove(&move);                                         //  Recover Move from buffer.

    makeMove(&move, &gs);                                           //  Make the move.

    serializeGameStateToBuffer(&gs, outputGameStateBuffer);         //  Write updated GameState to output-gamestate buffer.

    return;
  }

void makeNullMove_eval(void)
  {
    GameState gs;

    deserializeGameState(&gs);                                      //  Recover GameState from buffer.

    makeNullMove(&gs);                                              //  Make a null move.

    serializeGameStateToBuffer(&gs, outputGameStateBuffer);         //  Write updated GameState to output-gamestate buffer.

    return;
  }

/* Answer the Negamax Module's query, "What is the evaluation of the GameState in the input-gamestate buffer?" */
float evaluate_eval(void)
  {
    GameState gs;
    unsigned int white, black;

    deserializeGameState(&gs);                                      //  Recover GameState from buffer.

    if(terminal(&gs))
      {
        white = scoreWhite(&gs);
        black = scoreBlack(&gs);

        if(white == black)
          return 0.0f;

        if(gs.whiteToMove)
          return white > black ? 1.0f : -1.0f;
        else
          return black > white ? 1.0f : -1.0f;
      }

    interpret(&gs, zinnInput);

    return zinnForward(zinnInput);                                  //  Observe the Negamax Rule: always evaluate for the side that is now to move.
  }

/* Answer the Negamax Module's query, "What are all the moves that can be made from the GameState in the input-gamestate buffer?"
   Writes to "outputMovesBuffer":
     [_MOVE_BYTE_SIZE bytes of move, 4 bytes of a signed int, 1 byte indicating whether the move is "quiet"],
     [_MOVE_BYTE_SIZE bytes of move, 4 bytes of a signed int, 1 byte indicating whether the move is "quiet"],
                                                         . . .
     [_MOVE_BYTE_SIZE bytes of move, 4 bytes of a signed int, 1 byte indicating whether the move is "quiet"] */
unsigned int getMoves_eval(void)
  {
    GameState gs, tmp;
    Move moves[_MAX_NUM_TARGETS];                                   //  Generous upper bound assumes that a single piece could reach half of all squares.
    Move replies[_MAX_NUM_TARGETS];
    unsigned int len, replyCount, i, ctr, before, after;
    signed int delta, score;
    bool givesFreedom, quiet;
                                                                    //  Make sure these things are true!
    static_assert(INTERPRETATION_VECTOR_LENGTH == ZINN_INPUT_PLANES * ZINN_SQUARES);
    static_assert(sizeof(float) == 4);
    static_assert(sizeof(signed int) == 4);

    ctr = 0;
    deserializeGameState(&gs);                                      //  Recover GameState from buffer.

    before = gs.whiteToMove ? scoreWhite(&gs) : scoreBlack(&gs);    //  Record the score for the side now to move.
    len = getMoves(&gs, moves);

    for(i = 0; i < len; i++)
      {
        copyGameState(&gs, &tmp);                                   //  Copy the game state.
        makeMove(moves + i, &tmp);                                  //  Make the candidate move.
        after = gs.whiteToMove ? scoreWhite(&tmp) : scoreBlack(&tmp);
        delta = (signed int)after - (signed int)before;             //  Difference between after and before for the side now to move.

        givesFreedom = (tmp.prevMove == _NONE);
        replyCount = terminal(&tmp) ? 0 : getMoves(&tmp, replies);  //  How many replies are there AFTER the move?
        quiet = (delta == 0 && !givesFreedom);                      //  Move is "quiet" if mover's score doesn't change,
                                                                    //  and if the move does not give the opponent freedom.
        score = 1024 * delta - 64 * (givesFreedom ? 1 : 0) - (signed int)replyCount;

        outputMovesBuffer[ctr++] = moves[i].index;
        memcpy(outputMovesBuffer + ctr, &score, sizeof(score));     //  Force the signed int into a 4-byte temp buffer.
        ctr += sizeof(score);
        outputMovesBuffer[ctr++] = quiet ? MOVEFLAG_QUIET : MOVEFLAG_NOISY;
      }

    return len;
  }

/**************************************************************************************************
 Network Forward  */

static float zinnForward(const float* input)
  {
    unsigned int block;
    float* oldCurrent;
    float* current  = zinnActivationA;
    float* scratch1 = zinnActivationB;
    float* scratch2 = zinnActivationC;

    zinnStem(input, current);

    for(block = 0; block < ZINN_BLOCKS; ++block)
      {
        const unsigned int base = ZINN_BLOCK0_OFFSET + block * ZINN_BLOCK_FLOATS;
                                                                    //  Packed block:
                                                                    //    +0      depthwise1.weight   288
                                                                    //    +288    depthwise1.bias      32
                                                                    //    +320    pointwise1.weight  1024
                                                                    //    +1344   pointwise1.bias      32
                                                                    //    +1376   depthwise2.weight   288
                                                                    //    +1664   depthwise2.bias      32
                                                                    //    +1696   pointwise2.weight  1024
                                                                    //    +2720   pointwise2.bias      32
        zinnDepthwise3x3(current, scratch1, zinnWeights + base, zinnWeights + base + 288);
        zinnPointwise(scratch1, scratch2, zinnWeights + base + 320, zinnWeights + base + 1344);
        zinnDepthwise3x3(scratch2, scratch1, zinnWeights + base + 1376, zinnWeights + base + 1664);
        zinnPointwiseResidual(scratch1, scratch2, current, zinnWeights + base + 1696, zinnWeights + base + 2720);
                                                                    //  scratch2 is now this block's output.
                                                                    //  The old current buffer becomes spare.
        oldCurrent = current;
        current = scratch2;
        scratch2 = oldCurrent;
      }

    zinnHiddenLayer(current);

    return zinnValueHead();
  }

static void zinnStem(const float* input, float* output)
  {
    Eigen::Map<const InputMatrix> x(input);
    Eigen::Map<const StemWeightMatrix> w(zinnWeights + ZINN_STEM_WEIGHT_OFFSET);
    Eigen::Map<const Bias32> b(zinnWeights + ZINN_STEM_BIAS_OFFSET);
    Eigen::Map<ActivationMatrix> y(output);

    y.noalias() = w * x;
    y.colwise() += b;
    y.array() = y.array().max(0.0f);

    return;
  }

static void zinnDepthwise3x3(const float* input, float* output, const float* weights, const float* bias)
  {
    int channel, row, col;
    int kr, kc, ir, ic;
    float sum;

    for(channel = 0; channel < ZINN_CHANNELS; channel++)
      {
        for(row = 0; row < 10; row++)
          {
            for(col = 0; col < 10; col++)
              {
                sum = bias[channel];

                for(kr = 0; kr < 3; kr++)
                  {
                    ir = row + kr - 1;
                    if(ir < 0 || ir >= 10)
                      continue;

                    for(kc = 0; kc < 3; kc++)
                      {
                        ic = col + kc - 1;
                        if(ic < 0 || ic >= 10)
                          continue;

                        sum += input[channel * 100 + ir * 10 + ic] * weights[channel * 9 + kr * 3 + kc];
                      }
                  }

                output[channel * 100 + row * 10 + col] = sum > 0.0f ? sum : 0.0f;
              }
          }
      }

    return;
  }

static void zinnPointwise(const float* input, float* output, const float* weights, const float* bias)
  {
    Eigen::Map<const ActivationMatrix> x(input);
    Eigen::Map<const PointwiseWeightMatrix> w(weights);
    Eigen::Map<const Bias32> b(bias);
    Eigen::Map<ActivationMatrix> y(output);

    y.noalias() = w * x;
    y.colwise() += b;
    y.array() = y.array().max(0.0f);

    return;
  }

static void zinnPointwiseResidual(const float* input, float* output, const float* residual, const float* weights, const float* bias)
  {
    Eigen::Map<const ActivationMatrix> x(input);
    Eigen::Map<const ActivationMatrix> r(residual);
    Eigen::Map<const PointwiseWeightMatrix> w(weights);
    Eigen::Map<const Bias32> b(bias);
    Eigen::Map<ActivationMatrix> y(output);

    y.noalias() = w * x;
    y.colwise() += b;
    y += r;
    y.array() = y.array().max(0.0f);

    return;
  }

static void zinnHiddenLayer(const float* input)
  {
    Eigen::Map<const FlatVector> x(input);
    Eigen::Map<const HiddenWeightMatrix> w(zinnWeights + ZINN_HIDDEN_WEIGHT_OFFSET);
    Eigen::Map<const HiddenVector> b(zinnWeights + ZINN_HIDDEN_BIAS_OFFSET);
    Eigen::Map<HiddenVector> y(zinnHidden);

    y.noalias() = w * x;
    y += b;
    y.array() = y.array().max(0.0f);

    return;
  }

static float zinnValueHead(void)
  {
    Eigen::Map<const HiddenVector> hidden(zinnHidden);
    Eigen::Map<const Eigen::Matrix<float, 64, 1>> weights(zinnWeights + ZINN_VALUE_WEIGHT_OFFSET);
    float raw = weights.dot(hidden) + zinnWeights[ZINN_VALUE_BIAS_OFFSET];

    return tanhf(raw);
  }