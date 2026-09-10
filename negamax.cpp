/*

sudo docker run --rm -v $(pwd):/src -u $(id -u):$(id -g) --mount type=bind,source=$(pwd),target=/home/src c-wasm em++ -I ./ -Os -s STANDALONE_WASM -s INITIAL_MEMORY=16777216 -s STACK_SIZE=1048576 -s EXPORTED_FUNCTIONS="['_getMaxPly','_getInputBuffer','_getParametersBuffer','_getQueryGameStateBuffer','_getQueryMoveBuffer','_getAnswerGameStateBuffer','_getAnswerMovesBuffer','_getOutputBuffer','_getZobristHashBuffer','_getTranspositionTableBuffer','_getNegamaxSearchBuffer','_getNegamaxMovesBuffer','_getKillerMovesBuffer','_getHistoryTableBuffer','_getStatisticsBuffer','_setSearchId','_getSearchId','_getStatus','_setControlFlag','_unsetControlFlag','_getControlByte','_setTargetDepth','_getTargetDepth','_getDepthAchieved','_resetNodesSearched','_getNodesSearched','_finalDepthAchieved','_finalScore','_getNodeStackSize','_getMovesArenaSize','_initSearch','_incTranspoTableGeneration','_ageHistoryTable','_getTTProbes','_getTTHits','_getTTCutoffs','_getTTDepthQualified','_negamax']" -Wl,--no-entry "negamax.cpp" -o "negamax.wasm"

*/

#include <algorithm>                                                /* For std::max() and std::min(). */
#include <limits>                                                   /* For std::numeric_limits. */

#include "transposition.h"                                          /* Include the Transposition Table library. */
#include "zobrist.h"                                                /* Include the Zobrist hasher, which is an array of unsigned long longs (64-bit ints). */

#define _GAMESTATE_BYTE_SIZE                    27                  /* Number of bytes needed to encode a game state. */
#define _MOVE_BYTE_SIZE                          1                  /* Number of bytes needed to encode a move. */
#define _MAX_MOVES                             100                  /* A (tight) upper bound on how many moves may be made by a team in a single turn. */
#define _NONE                                  100                  /* Required as a "blank" value without #include "gamestate.h". */

#define _ANCHOR_BYTE_OFFSET                     26                  /* Index of the anchor cell in the game-state encoding. */

#define _WHITE_TO_MOVE                           0                  /* Copied from gamestate.h. */
#define _BLACK_TO_MOVE                           1                  /* Copied from gamestate.h. */

#define _MAX_PLY                                 6                  /* Deepest possible depth. */
#define _QUIESCENCE_MAX_PLY                      4                  /* Maximum extension for quiescence search. */

#define _PARAMETER_ARRAY_SIZE                   12                  /* Number of bytes needed to store search parameters. */

#define _NEGAMAX_NODE_STACK_CAPACITY            32                  /* Maximum simultaneously live DFS nodes. This size should be a power of 2
                                                                       that comfortably upper-bounds (_MAX_PLY + _QUIESCENCE_MAX_PLY + 1). */
#define _NEGAMAX_MOVE_ARENA_CAPACITY          4096                  /* Maximum move records owned by simultaneously live nodes.
                                                                       Should be a power of 2 that safely exceeds
                                                                         (_MAX_PLY + _QUIESCENCE_MAX_PLY + 1) * _MAX_MOVES. */
static_assert(_NEGAMAX_NODE_STACK_CAPACITY >= (_MAX_PLY + _QUIESCENCE_MAX_PLY + 1), "Negamax node stack is too small for configured search depth.");
static_assert(_NEGAMAX_MOVE_ARENA_CAPACITY >= (_MAX_PLY + _QUIESCENCE_MAX_PLY + 1) * _MAX_MOVES, "Negamax move arena is too small for worst-case live move lists.");

#define _NEGAMAX_NODE_BYTE_SIZE                 77                  /* Number of bytes needed to store a negamax node. */
#define _NEGAMAX_MOVE_BYTE_SIZE                  2                  /* Number of bytes needed to store a negamax move. */

#define _KILLER_MOVE_PER_PLY                     2                  /* Typical for other chess engines. */
#define _KILLER_MOVE_MAX_DEPTH                  64                  /* Simply something "comfortably large". */
#define KILLER_NOT_FOUND                         0
#define KILLER_FOUND_FIRST                       1
#define KILLER_FOUND_SECOND                      2
#define KILLER_DIRECTION_INVALID                 0
#define KILLER_DIRECTION_U                       1
#define KILLER_DIRECTION_UR                      2
#define KILLER_DIRECTION_R                       3
#define KILLER_DIRECTION_DR                      4
#define KILLER_DIRECTION_D                       5
#define KILLER_DIRECTION_DL                      6
#define KILLER_DIRECTION_L                       7
#define KILLER_DIRECTION_UL                      8

#define PARAM_BUFFER_SEARCHID_OFFSET          0x00                  /* Bytes into "inputParametersBuffer", where the search ID begins. */
#define PARAM_BUFFER_STATUS_OFFSET            0x04                  /* Bytes into "inputParametersBuffer", where the status byte exists. */
#define PARAM_BUFFER_COMMAND_OFFSET           0x05                  /* Bytes into "inputParametersBuffer", where the command byte exists. */
#define PARAM_BUFFER_TARGETDEPTH_OFFSET       0x06                  /* Bytes into "inputParametersBuffer", where the target depth byte exists. */
#define PARAM_BUFFER_DEPTHACHIEVED_OFFSET     0x07                  /* Bytes into "inputParametersBuffer", where the depth achieved byte exists. */
#define PARAM_BUFFER_NODESSEARCHED_OFFSET     0x08                  /* Bytes into "inputParametersBuffer", where the node count begins. */
                                                                    //  Meaning                                            Is root output trustworthy?
                                                                    //  =====================================================================================
#define STATUS_IDLE                           0x00                  /*  No search running. Awaiting instructions.          NO                               */
#define STATUS_RUNNING                        0x01                  /*  Search running. More pulses needed.                Not yet                          */
#define STATUS_DONE                           0x02                  /*  Search completed.                                  YES                              */
#define STATUS_STOP_REQUESTED                 0x03                  /*  Will halt the present search at                    Only previously completed result
                                                                        the next safe point. Clean voluntary stop.                                          */
#define STATUS_STOP_TIME                      0x04                  /*  Will halt the present search at
                                                                        the next safe point. Clean time-budget stop.       Only previously completed result */
#define STATUS_ABORTED                        0x05                  /*  Search was hard-killed.                            Don't trust partial work         */
#define STATUS_ERROR                          0xFF                  /*  An error has occurred.                             NO                               */

#define CTRL_STOP_REQUESTED                   0x01                  /* Set this byte in commandFlags to request that the present search stop. */
#define CTRL_HARD_ABORT                       0x02                  /* Set this byte in commandFlags to request that the present search abort. */
#define CTRL_STOP_TIME                        0x04                  /* Set this byte in commandFlags to indicate that search's time budget expired. */

#define _PHASE_ENTER_NODE                        0                  /* Go to enterNode_step()  when entering negamax(). */
#define _PHASE_GEN_AND_ORDER                     1                  /* Go to expansion_step()  when entering negamax(). */
#define _PHASE_NEXT_MOVE                         2                  /* Go to nextMove_step()   when entering negamax(). */
#define _PHASE_AFTER_CHILD                       3                  /* Go to afterChild_step() when entering negamax(). */
#define _PHASE_FINISH_NODE                       4                  /* Go to finishNode_step() when entering negamax(). */
#define _PHASE_COMPLETE                          5                  /* Write to output buffer when entering negamax() and signal search completion.  */

#define NN_FLAG_NULL_TRIED                    0x01                  /* Indicates that we already tried a null move here. */
#define NN_FLAG_NULL_IN_PROGRESS              0x02                  /* Indicates that there is currently a null-move child. */
#define NN_FLAG_IS_NULL_CHILD                 0x04                  /* Indicates that this node itself was reached via null move. */
#define NN_FLAG_IS_PV                         0x08                  /* Indicates a PV node. */
#define NN_FLAG_AT_ROOT                       0x10                  /* Indicates a root node. */
#define NN_FLAG_IN_CHECK                      0x20                  /* Indicates that the side to move is in check here (cached). */
#define NN_FLAG_REPETITION_DISABLED           0x40                  /* Indicates that repetition should NOT be checked for this node.
                                                                       (For instance, if this node DESCENDS from a null-move.) */
#define NN_FLAG_PATH_DEPENDENT_RESULT         0x80                  /* Indicates that this node's value was reached through a repetition path,
                                                                       so its result must NOT be stored in the transposition table. */
                                                                    /* Convenience macros. */
#define NN_SET_FLAG(node, f)                 ((node)->flags |=  (f))
#define NN_CLEAR_FLAG(node, f)               ((node)->flags &= ~(f))
#define NN_HAS_FLAG(node, f)          (((node)->flags &   (f)) != 0)

#define NULL_MOVE_BASE_REDUCTION                 2                  /*  */
#define NULL_MOVE_EXTRA_REDUCTION                1                  /*  */

#define MOVE_SORTING_TRANSPO_BEST_MOVE_BONUS 10000                  /* Huge bonus to the move already listed as the best one to make. */
#define MOVE_SORTING_KILLER_MOVE_1_BONUS       800                  /* Sizable bonus for being a (fresh) killer move. */
#define MOVE_SORTING_KILLER_MOVE_2_BONUS       700                  /* Slightly diminished bonus for being a (less fresh) killer move. */

#define MOVEFLAG_QUIET                           0                  /* The move is neither a capture, nor a promotion. */
#define MOVEFLAG_NOISY                           1                  /* The move is a capture, or a promotion (or both). */

#define _STATS_BUFFER_SIZE                      16                  /* Size of the buffer used to track statistics across pulses. */

#define STATS_TT_PROBES_OFFSET                   0                  /* Offset of the TT probe count. */
#define STATS_TT_HITS_OFFSET                     4                  /* Offset of the TT hit count. */
#define STATS_TT_DEPTH_QUALIFIED_OFFSET          8                  /* Offset of the TT qualified count. */
#define STATS_TT_CUTOFFS_OFFSET                 12                  /* Offset of the TT cutoffs count. */
                                                                    /* FREEDOM has no draw by repetition. It is impossible to repeat states. */
//#define _REPETITION_HASH_BYTE_SIZE              16                  /*  */
//#define _REPETITION_HISTORY_CAPACITY           150                  /* GameState.moveCtr == 2 * 75 forces a draw. */
//#define _REPETITION_PATH_CAPACITY      _NEGAMAX_NODE_STACK_CAPACITY /*  */
//#define _REPETITION_PATH_PREFIX_CAPACITY         1                  /*  */
//#define _REPETITION_PATH_HEADER_SIZE             1                  /*  */
//#define _REPETITION_STATE_BYTE_SIZE             66                  /* (A simplified form of the game state encoding.) */
//#define HISTORY_DRAW                             1                  /* Copied from "philadelphia.c" without having to #include "philadelphia".
//                                                                       The number of occurrences of the given game state DOES CAUSE draw by repetition. */

/**************************************************************************************************
 Typedefs  */

typedef struct NegamaxNodeType                                      //  TOTAL: 77 = _NEGAMAX_NODE_BYTE_SIZE bytes.
  {
    unsigned char gs[_GAMESTATE_BYTE_SIZE];                         //  (27 bytes) The given gamestate as a byte array.
    unsigned int parent;                                            //  (4 bytes) Index into "negamaxSearchBuffer" of this node's parent.
    unsigned char parentMove[_MOVE_BYTE_SIZE];                      //  (1 byte) Describe the move that led from "parent" to this node.
    unsigned char bestMove[_MOVE_BYTE_SIZE];                        //  (1 byte) Best move found so far from this node.

    unsigned int moveOffset;                                        //  (4 bytes) Index (inclusive) into a global move buffer where a chunk of child-moves STARTS.
    unsigned int moveCount;                                         //  (4 bytes) How many moves generated for this node.
    unsigned int moveNextPtr;                                       //  (4 bytes) The index of the MOVE TO TRY NEXT.

    signed char depth;                                              //  (1 byte) Counts down to 0, or to -1 if quiescence searching.
    unsigned char ply;                                              //  (1 byte) Distance from root (starts at 0).

    float originalAlpha;                                            //  (4 bytes) The alpha value saved at the top of the negamax call.
    float alpha;                                                    //  (4 bytes) Lower bound.
    float beta;                                                     //  (4 bytes) Upper bound.
    float value;                                                    //  (4 bytes) Value computed over this node's children and/or eventually returned.

    unsigned long long zhash;                                       //  (8 bytes) Zobrist hash of the given gamestate byte array.
    unsigned int hIndex;                                            //  (4 bytes) Zobrist hash modulo table length, adjusted as necessary using linear probing.

    unsigned char phase;                                            //  (1 byte) In {_PHASE_ENTER_NODE, _PHASE_GEN_AND_ORDER, _PHASE_NEXT_MOVE,
                                                                    //               _PHASE_AFTER_CHILD, _PHASE_FINISH_NODE}.
    unsigned char flags;                                            //  (1 byte) Covers [NN_FLAG_NULL_TRIED, NN_FLAG_NULL_IN_PROGRESS, NN_FLAG_IS_NULL_CHILD,
                                                                    //                   NN_FLAG_IS_PV, NN_FLAG_AT_ROOT, NN_FLAG_IN_CHECK].
  } NegamaxNode;

typedef struct NegamaxMoveType                                      //  TOTAL: 2 = _NEGAMAX_MOVE_BYTE_SIZE bytes.
  {
    unsigned char moveByteArray[_MOVE_BYTE_SIZE];                   //  (1 byte) Enough bytes to encode a move.
    unsigned char quietMove;                                        //  (1 byte)  (Should be a bool.) "Quiet" moves.
  } NegamaxMove;

/**************************************************************************************************
 Prototypes  */

 /*  Imported from the Player object's evaluationEngine module.
     This gives you linguistic independence: leave tree-search to C++. */
                                                                    //  Bridge between WebAssembly Modules:
                                                                    //  Prepare to query the Evaluation Engine by copying the Negamax Engine's
                                                                    //  query-gamestate-buffer contents into the Evaluation Engine's input-gamestate buffer.
__attribute__((import_module("env"), import_name("_copyQuery2EvalGSInput"))) void copyQuery2EvalGSInput();
                                                                    //  Bridge between WebAssembly Modules:
                                                                    //  Prepare to query the Evaluation Engine by copying the Negamax Engine's
                                                                    //  query-move-buffer contents into the Evaluation Engine's input-move buffer.
__attribute__((import_module("env"), import_name("_copyQuery2EvalMoveInput"))) void copyQuery2EvalMoveInput();
                                                                    //  Bridge between WebAssembly Modules:
                                                                    //  Copy the Evaluation Engine's output-gamestate
                                                                    //  to the Negamax Engine's answer-gamestate buffer.
__attribute__((import_module("env"), import_name("_copyEvalOutput2AnswerGSBuffer"))) void copyEvalOutput2AnswerGSBuffer();
                                                                    //  Bridge between WebAssembly Modules:
                                                                    //  Copy the Evaluation Engine's output-moves
                                                                    //  to the Negamax Engine's answer-moves buffer.
                                                                    //  (That is, copy the given number n of byte-chunks to the answer-moves buffer.)
__attribute__((import_module("env"), import_name("_copyEvalOutput2AnswerMovesBuffer"))) void copyEvalOutput2AnswerMovesBuffer(unsigned int);
//                                                                    //  Bridge between WebAssembly Modules:
//                                                                    //  Copy the Evaluation Engine's repetition-detection encoding output-buffer
//                                                                    //  to the Negamax Engine's repetition-detection answer-buffer.
//__attribute__((import_module("env"), import_name("_copyEvalRepetitionOutput2AnswerRepetitionBuffer"))) void copyEvalRepetitionOutput2AnswerRepetitionBuffer();
//                                                                    //  Bridge between WebAssembly Modules:
//                                                                    //  Query the Evaluation Engine.
//                                                                    //  Which side is to move in the GameState encoded in Evaluation Engine's input buffer?
//__attribute__((import_module("env"), import_name("_sideToMove"))) unsigned char sideToMove();
                                                                    //  Bridge between WebAssembly Modules:
                                                                    //  Query the Evaluation Engine.
                                                                    //  Is the GameState encoded in Evaluation Engine's input buffer terminal?
__attribute__((import_module("env"), import_name("_isTerminal"))) bool isTerminal();
                                                                    //  Bridge between WebAssembly Modules:
                                                                    //  Query the Evaluation Engine.
                                                                    //  Make Evaluation Engine write the child-state bytes resulting from the given move
                                                                    //  to the Evaluation Engine's output buffer.
__attribute__((import_module("env"), import_name("_makeMove"))) void makeMove();
                                                                    //  Bridge between WebAssembly Modules:
                                                                    //  Query the Evaluation Engine.
                                                                    //  Make Evaluation Engine write the child-state, bytes resulting from a null move
                                                                    //  to the Evaluation Engine's output buffer.
__attribute__((import_module("env"), import_name("_makeNullMove"))) void makeNullMove();
                                                                    //  Bridge between WebAssembly Modules:
                                                                    //  Query the Evaluation Engine.
                                                                    //  Evaluate the GameState encoded in Evaluation Engine's input buffer?
__attribute__((import_module("env"), import_name("_evaluate"))) float evaluate();
                                                                    //  Bridge between WebAssembly Modules:
                                                                    //  Query the Evaluation Engine.
                                                                    //  Make Evaluation Engine write a sorted list of (child-state, move) tuples
                                                                    //  to the Evaluation Engine's output buffer.
__attribute__((import_module("env"), import_name("_getMoves"))) unsigned int getMoves();
//                                                                    //  Bridge between WebAssembly Modules:
//                                                                    //  Query the Evaluation Engine.
//                                                                    //  Have Evaluation Engine encode the game state encoded in its input buffer
//                                                                    //  as a FIDE-compliant array in its repetition-detection output buffer.
//__attribute__((import_module("env"), import_name("_repetitionState"))) void repetitionState();
//                                                                    //  Bridge between WebAssembly Modules:
//                                                                    //  Query the Evaluation Engine.
//                                                                    //  Is the given occurrence count sufficient to force a draw?
//__attribute__((import_module("env"), import_name("_historyVerdict"))) unsigned char historyVerdict(unsigned char);

extern "C"
  {
    unsigned char getMaxPly(void);

    unsigned char* getInputBuffer(void);
    unsigned char* getParametersBuffer(void);
    unsigned char* getOutputBuffer(void);
    unsigned char* getQueryGameStateBuffer(void);
    unsigned char* getQueryMoveBuffer(void);
    unsigned char* getAnswerGameStateBuffer(void);
    unsigned char* getAnswerMovesBuffer(void);
    unsigned char* getZobristHashBuffer(void);
    unsigned char* getTranspositionTableBuffer(void);
    unsigned char* getNegamaxSearchBuffer(void);
    unsigned char* getNegamaxMovesBuffer(void);
    unsigned char* getKillerMovesBuffer(void);
    unsigned char* getHistoryTableBuffer(void);
//    unsigned char* getRepetitionHistoryBuffer(void);
//    unsigned char* getRepetitionPathBuffer(void);
//    unsigned char* getAnswerRepetitionStateBuffer(void);
    unsigned char* getStatisticsBuffer(void);

    void setSearchId(unsigned int);
    unsigned int getSearchId(void);
    unsigned char getStatus(void);
    void setControlFlag(unsigned char);
    void unsetControlFlag(unsigned char);
    unsigned char getControlByte(void);
    void setTargetDepth(unsigned char);
    unsigned char getTargetDepth(void);
    unsigned char getDepthAchieved(void);
    void resetNodesSearched(void);
    unsigned int getNodesSearched(void);
    unsigned char finalDepthAchieved(void);
    float finalScore(void);
    unsigned int getNodeStackSize(void);
    unsigned int getMovesArenaSize(void);

    void initSearch(void);
    void incTranspoTableGeneration(void);
    void ageHistoryTable(void);
    unsigned int getTTProbes(void);
    unsigned int getTTHits(void);
    unsigned int getTTCutoffs(void);
    unsigned int getTTDepthQualified(void);
    bool negamax(void);
  }

void enterNode_step(unsigned int, NegamaxNode*);
void transpoProbe(unsigned int, NegamaxNode*);
void expansion_step(unsigned int, NegamaxNode*);
void nextMove_step(unsigned int, NegamaxNode*);
void afterChild_step(unsigned int, NegamaxNode*);
void finishNode_step(unsigned int, NegamaxNode*);

void quicksort(bool, signed int*, NegamaxMove*, unsigned int, unsigned int);
unsigned int partition(bool, signed int*, NegamaxMove*, unsigned int, unsigned int);

unsigned int restoreNegamaxSearchBufferLength(void);
unsigned int restoreNegamaxMoveBufferLength(void);
void saveNegamaxSearchBufferLength(unsigned int);
void saveNegamaxMoveBufferLength(unsigned int);
void restoreNode(unsigned int, NegamaxNode* );
void saveNode(NegamaxNode*, unsigned int);
void restoreMove(unsigned int, NegamaxMove*);
void saveMove(NegamaxMove*, unsigned int);

unsigned long long hash(unsigned char*);

unsigned char killerLookup(unsigned char, unsigned char, unsigned char);
void killerAdd(unsigned char, unsigned char, unsigned char);
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

unsigned int historyLookup(unsigned char, unsigned char);
void historyUpdate(unsigned char, unsigned char, unsigned char);

void incrementNodeCtr(void);
unsigned int generationAge(unsigned char, unsigned char);
unsigned int statsGet(unsigned int);
void statsSet(unsigned int, unsigned int);
void statsIncrement(unsigned int);
void resetTTStats(void);

//unsigned char repetitionPathPrefixLength(void);
//unsigned int repetitionPathNodeOffset(unsigned int);
//void saveRepetitionState(unsigned int);
//unsigned int repetitionOccurrenceCount(unsigned int);
//unsigned int repetitionHistoryLength(void);

/**************************************************************************************************
 Globals  */
                                                                    //  27 bytes.
                                                                    //  Global array containing the serialized game state:
unsigned char inputGameStateBuffer[_GAMESTATE_BYTE_SIZE];           //  Input from Player.js to its negamaxEngine.

                                                                    //  12 bytes.
                                                                    //  Search ID:      4 bytes.
                                                                    //  Status:         1 byte.
                                                                    //  Control Flags:  1 byte.
                                                                    //  Target Depth:   1 byte.
                                                                    //  Depth Reached:  1 byte.
unsigned char inputParametersBuffer[_PARAMETER_ARRAY_SIZE];         //  Nodes Searched: 4 bytes.

                                                                    //  33 bytes.
                                                                    //  Global array containing: {serialized game state (sanity check),
                                                                    //                            1-byte uchar          (depth achieved),
                                                                    //                            serialized move       (move to make in this state),
                                                                    //                            4-byte float          (score)                      }
                                                                    //  Output from negamaxEngine to Player.js.
unsigned char outputBuffer[_GAMESTATE_BYTE_SIZE + 1 + _MOVE_BYTE_SIZE + 4];

                                                                    //  27 bytes.
                                                                    //  Global array containing the serialized (query) game state:
unsigned char queryGameStateBuffer[_GAMESTATE_BYTE_SIZE];           //  Input from negamaxEngine to evaluationEngine.

                                                                    //  1 byte.
                                                                    //  Global array containing the serialized (query) move:
unsigned char queryMoveBuffer[_MOVE_BYTE_SIZE];                     //  Input from negamaxEngine to evaluationEngine.

                                                                    //  27 bytes.
                                                                    //  Global array containing a serialized (answer) game state:
unsigned char answerGameStateBuffer[_GAMESTATE_BYTE_SIZE];          //  Output from evaluationEngine to negamaxEngine.

                                                                    //  600 bytes.
                                                                    //  Global array containing: {serialized (answer-move, rough score, quiet-flag)}:
                                                                    //  Output from evaluationEngine to negamaxEngine.
unsigned char answerMovesBuffer[_MAX_MOVES * (_MOVE_BYTE_SIZE + 5)];//  The actual number of moves is the unsigned char returned by this function.

                                                                    //  2,408 bytes.
unsigned char zobristHashBuffer[ZHASH_TABLE_SIZE * 8];              //  Global array containing the serialized Zobrist-hasher values (unsigned long longs).
                                                                    //  "Keys" are simply unisnged int values #defined above.

                                                                    //  8,388,609 bytes.
                                                                    //  For "transpositionTableBuffer" included in "transposition.h".

                                                                    //  2,468 bytes.
                                                                    //  Flat, global array that behaves like a DFS stack for negamax nodes.
                                                                    //  First four bytes are for an unsigned int: the length of the array.
unsigned char negamaxSearchBuffer[4 + _NEGAMAX_NODE_STACK_CAPACITY * _NEGAMAX_NODE_BYTE_SIZE];

                                                                    //  8,196 bytes.
                                                                    //  Flat, global array that accumulates all moves for all nodes.
                                                                    //  First four bytes are for an unsigned int: the length of the array.
unsigned char negamaxMovesBuffer[4 + _NEGAMAX_MOVE_ARENA_CAPACITY * _NEGAMAX_MOVE_BYTE_SIZE];

                                                                    //  128 bytes.
                                                                    //  Each entry is [direction_1, direction_2].
                                                                    //  Number per ply, times 2 bytes per move, times max depth.
                                                                    //  (By definition, killer moves are "quiet"--no score delta, no freedom.)
                                                                    //  This buffer is arranged as:
                                                                    //  [ (ply-0 direction)     (ply-0 direction),
                                                                    //    (ply-1 direction)     (ply-1 direction),
                                                                    //    (ply-2 direction)     (ply-2 direction),
                                                                    //                     . . .
                                                                    //    (ply-MAX-1 direction) (ply-MAX-1 tdirection) ]
unsigned char killerMovesTableBuffer[_KILLER_MOVE_PER_PLY * _KILLER_MOVE_MAX_DEPTH];

                                                                    //  10,100 bytes.
                                                                    //  This buffer is arranged as:
                                                                    //  [ Anchor-index 0,     To-indices 0 .. 99,
                                                                    //    Anchor-index 1,     To-indices 0 .. 99,
                                                                    //    Anchor-index 2,     To-indices 0 .. 99,
                                                                    //                     . . .
unsigned char historyTableBuffer[(_NONE + 1) * _NONE];              //    Anchor-index _NONE, To-indices 0 .. 99  ]

                                                                    //  N/A bytes.
                                                                    //  Actual positions preceding the current search root.
                                                                    //  First four bytes store the number of entries.
//unsigned char repetitionHistoryBuffer[4 + _REPETITION_HISTORY_CAPACITY * _REPETITION_STATE_BYTE_SIZE];

                                                                    //  N/A bytes.
                                                                    //    byte [0]: how many prefix states are active
                                                                    //  Current hypothetical DFS ancestry.
                                                                    //  Slot i corresponds directly to Negamax node-stack slot i.
//unsigned char repetitionPathBuffer[_REPETITION_PATH_HEADER_SIZE + (_REPETITION_PATH_PREFIX_CAPACITY + _NEGAMAX_NODE_STACK_CAPACITY) * _REPETITION_STATE_BYTE_SIZE];

                                                                    //  N/A bytes.
                                                                    //  Scratch answer returned by the game-specific module when Negamax asks
                                                                    //  for a canonical repetition state.
//unsigned char answerRepetitionStateBuffer[_REPETITION_STATE_BYTE_SIZE];

                                                                    //  16 bytes.
                                                                    //  TT probe count:      4 bytes
                                                                    //  TT hits count:       4 bytes
                                                                    //  TT qualified count:  4 bytes
unsigned char statsBuffer[_STATS_BUFFER_SIZE];                      //  TT cutoffs count:    4 bytes

                                                                    //  ===================================================================
                                                                    //  SUBTOTAL:  8,412,652 bytes.

                                                                    //  Give the stack 1,048,576 bytes.

                                                                    //  TOTAL:     9,461,228 bytes.
                                                                    //  Round to:  9,502,720 = 145 pages (cover units of 65,536).

                                                                    //  Compile:   16,777,216 = 256 pages (cover units of 65,536).

/**************************************************************************************************
 Maximum ply.  */

unsigned char getMaxPly(void)
  {
    return _MAX_PLY;
  }

/**************************************************************************************************
 Pointer-retrieval functions  */

/* Expose the global array declared here to JavaScript.
   Player.js writes bytes arrays here and then calls its "negamaxEngine". */
unsigned char* getInputBuffer(void)
  {
    return &inputGameStateBuffer[0];
  }

/* Expose the global array declared here to JavaScript.
   Player.js reads and writes byte arrays here. */
unsigned char* getParametersBuffer(void)
  {
    return &inputParametersBuffer[0];
  }

/* Expose the global array declared here to JavaScript.
   "negamaxEngine" writes its output here for Player.js to pick up: {gamestate as byte array, unsigned int as byte array, move as byte array}.
   This stands for {child state, depth searched, move to play in reply}. */
unsigned char* getOutputBuffer(void)
  {
    return &outputBuffer[0];
  }

/* Expose the global array declared here to JavaScript (just so we can address it).
   Player's "negamaxEngine" writes a game states as a byte array here and then calls Player.js's "evaluationEngine". */
unsigned char* getQueryGameStateBuffer(void)
  {
    return &queryGameStateBuffer[0];
  }

/* Expose the global array declared here to JavaScript (just so we can address it).
   Player's "negamaxEngine" writes a move as a byte array here and then calls Player.js's "evaluationEngine". */
unsigned char* getQueryMoveBuffer(void)
  {
    return &queryMoveBuffer[0];
  }

/* Expose the global array decalred here to JavaScript (just so we can address it). */
unsigned char* getAnswerGameStateBuffer(void)
  {
    return &answerGameStateBuffer[0];
  }

/* Expose the global array decalred here to JavaScript (just so we can address it). */
unsigned char* getAnswerMovesBuffer(void)
  {
    return &answerMovesBuffer[0];
  }

/* Expose the global array declared here to JavaScript (just so we can fill it).
   Filled once and read-only throughout a match. */
unsigned char* getZobristHashBuffer(void)
  {
    return &zobristHashBuffer[0];                                   //  Defined in zobrist.h.
  }

/* Expose the global array declared in "transposition.h" to JavaScript (just so we can address it). */
unsigned char* getTranspositionTableBuffer(void)
  {
    return &transpositionTableBuffer[0];
  }

/* Expose the global array decalred here to JavaScript (just so we can address it). */
unsigned char* getNegamaxSearchBuffer(void)
  {
    return &negamaxSearchBuffer[0];
  }

/* Expose the global array decalred here to JavaScript (just so we can address it). */
unsigned char* getNegamaxMovesBuffer(void)
  {
    return &negamaxMovesBuffer[0];
  }

/* Expose the global array decalred here to JavaScript (just so we can address it). */
unsigned char* getKillerMovesBuffer(void)
  {
    return &killerMovesTableBuffer[0];
  }

/* Expose the global array decalred here to JavaScript (just so we can address it). */
unsigned char* getHistoryTableBuffer(void)
  {
    return &historyTableBuffer[0];
  }

/* Expose the global array decalred here to JavaScript (just so we can address it).
unsigned char* getRepetitionHistoryBuffer(void)
  {
    return &repetitionHistoryBuffer[0];
  } */

/* Expose the global array decalred here to JavaScript (just so we can address it).
unsigned char* getRepetitionPathBuffer(void)
  {
    return &repetitionPathBuffer[0];
  } */

/* Expose the global array decalred here to JavaScript (just so we can address it).
unsigned char* getAnswerRepetitionStateBuffer(void)
  {
    return &answerRepetitionStateBuffer[0];
  } */

/* Expose the global array decalred here to JavaScript (just so we can address it). */
unsigned char* getStatisticsBuffer(void)
  {
    return &statsBuffer[0];
  }

/**************************************************************************************************
 Search parameter functions  */

/* Set the search ID. */
void setSearchId(unsigned int searchId)
  {
    unsigned char buffer4[4];
    unsigned char i;

    memcpy(buffer4, (unsigned char*)(&searchId), 4);                //  Force the unsigned int into a 4-byte temp buffer.
    for(i = 0; i < 4; i++)                                          //  Copy bytes to parameters buffer.
      inputParametersBuffer[PARAM_BUFFER_SEARCHID_OFFSET + i] = buffer4[i];

    return;
  }

/* Retrieve the search ID from the parameters buffer. */
unsigned int getSearchId(void)
  {
    unsigned int searchId;
    unsigned char buffer4[4];
    unsigned char i;

    for(i = 0; i < 4; i++)
      buffer4[i] = inputParametersBuffer[PARAM_BUFFER_SEARCHID_OFFSET + i];

    memcpy(&searchId, buffer4, 4);                                  //  Force the 4-byte buffer into an unsigned int.

    return searchId;
  }

/* Retrieve the search status from the parameters buffer. */
unsigned char getStatus(void)
  {
    return inputParametersBuffer[PARAM_BUFFER_STATUS_OFFSET];
  }

/* Set a bit (or bits) in the command byte. */
void setControlFlag(unsigned char flag)
  {
    inputParametersBuffer[PARAM_BUFFER_COMMAND_OFFSET] |= flag;
    return;
  }

/* Unset a bit (or bits) in the command byte. */
void unsetControlFlag(unsigned char flag)
  {
    inputParametersBuffer[PARAM_BUFFER_COMMAND_OFFSET] &= ~(flag);
    return;
  }

/* Retrieve the command byte. */
unsigned char getControlByte(void)
  {
    return inputParametersBuffer[PARAM_BUFFER_COMMAND_OFFSET];
  }

/* Set the target depth. */
void setTargetDepth(unsigned char depth)
  {
    inputParametersBuffer[PARAM_BUFFER_TARGETDEPTH_OFFSET] = depth;
    return;
  }

/* Retrieve the target depth. */
unsigned char getTargetDepth(void)
  {
    return inputParametersBuffer[PARAM_BUFFER_TARGETDEPTH_OFFSET];
  }

/* Retrieve the depth achieved by search (so far). */
unsigned char getDepthAchieved(void)
  {
    return inputParametersBuffer[PARAM_BUFFER_DEPTHACHIEVED_OFFSET];
  }

/* Reset the number of nodes searched to zero. */
void resetNodesSearched(void)
  {
    unsigned int ctr;
    unsigned char buffer4[4];
    unsigned char i;

    ctr = 0;

    memcpy(buffer4, (unsigned char*)(&ctr), 4);                     //  Force the unsigned int into a 4-byte temp buffer.
    for(i = 0; i < 4; i++)                                          //  Copy bytes to parameters buffer.
      inputParametersBuffer[PARAM_BUFFER_NODESSEARCHED_OFFSET + i] = buffer4[i];

    return;
  }

/* Retrieve the number of nodes searched. */
unsigned int getNodesSearched(void)
  {
    unsigned int ctr;
    unsigned char buffer4[4];
    unsigned char i;

    for(i = 0; i < 4; i++)
      buffer4[i] = inputParametersBuffer[PARAM_BUFFER_NODESSEARCHED_OFFSET + i];

    memcpy(&ctr, buffer4, 4);                                       //  Force the 4-byte buffer into an unsigned int.

    return ctr;
  }

/* Read the depth achieved from "outputBuffer" and return it as a value. */
unsigned char finalDepthAchieved(void)
  {
    return outputBuffer[_GAMESTATE_BYTE_SIZE];
  }

/* Read the score from "outputBuffer" and return it as a value. */
float finalScore(void)
  {
    float score;
    unsigned char buffer4[4];
    unsigned char i;

    for(i = 0; i < 4; i++)
      buffer4[i] = outputBuffer[_GAMESTATE_BYTE_SIZE + 1 + _MOVE_BYTE_SIZE + i];

    memcpy(&score, buffer4, 4);                                     //  Force the 4-byte buffer into a float.

    return score;
  }

/* (Diagnostic) */
unsigned int getNodeStackSize(void)
  {
    return restoreNegamaxSearchBufferLength();
  }

/* (Diagnostic) */
unsigned int getMovesArenaSize(void)
  {
    return restoreNegamaxMoveBufferLength();
  }

/**************************************************************************************************
 Negamax-search functions  */

/* Initialize the root node for (interrupatble) negamax search. */
void initSearch(void)
  {
    NegamaxNode root;
    unsigned char depth;
    unsigned int i;
                                                                    //  Set status to RUNNING.
    inputParametersBuffer[PARAM_BUFFER_STATUS_OFFSET] = STATUS_RUNNING;

    resetTTStats();                                                 //  Reset transposition-table statistics.

    for(i = 0; i < _GAMESTATE_BYTE_SIZE; i++)                       //  Copy root gamestate byte array from global "inputGameStateBuffer"
      root.gs[i] = inputGameStateBuffer[i];                         //  to negamax root node.

    root.parent = 0;                                                //  Set root's parent index to...

    root.parentMove[0] = _NONE;                                     //  Set the root's parent-move to a blank-move.

    root.bestMove[0] = _NONE;                                       //  Set the root's best-move to a blank-move.

    root.moveOffset = 0;                                            //  Set offset into moves buffer for the children of root.
    root.moveCount = 0;                                             //  Set the number of children root has.
    root.moveNextPtr = 0;                                           //  Set the index to which root's children iterator currently points.

    depth = inputParametersBuffer[PARAM_BUFFER_TARGETDEPTH_OFFSET]; //  Retrieve target-depth parameter, clamp minimum to 1, clamp maximum to _MAX_PLY.
    if(depth < 1)
      depth = 1;
    else if(depth > _MAX_PLY)
      depth = _MAX_PLY;
    inputParametersBuffer[PARAM_BUFFER_TARGETDEPTH_OFFSET] = depth;

    root.depth = (signed char)depth;                                //  Set root's depth to "depth".
    root.ply = 0;                                                   //  Set root's ply to zero.

    root.originalAlpha = -std::numeric_limits<float>::infinity();   //  Initialize root's originalAlpha.
    root.alpha = -std::numeric_limits<float>::infinity();           //  Initialize root's alpha.
    root.beta = std::numeric_limits<float>::infinity();             //  Initialize root's beta.
    root.value = -std::numeric_limits<float>::infinity();           //  Initialize root's value.

    root.zhash = 0L;                                                //  Initialize root's zhash.
    root.hIndex = 0;                                                //  Initialize root's hIndex.

    root.phase = _PHASE_ENTER_NODE;                                 //  Indicate that no work has been done on this node yet:
                                                                    //  the first thing to try is a transposition table lookup.
    root.flags = NN_FLAG_AT_ROOT;                                   //  Set root flag.

    saveNegamaxSearchBufferLength(1);                               //  Write number of NegamaxNodes in "negamaxSearchBuffer".
    saveNegamaxMoveBufferLength(0);                                 //  Blank out the move buffer.
    saveNode(&root, 0);                                             //  Write serialized node to head of "negamaxSearchBuffer".

    return;
  }

/* HEARTBEAT NEGAMAX

   Depth-first search for a two-player, perfect-information, zero-sum game.
   Uses alpha-beta pruning and a transposition table.

   So that tree search does not overwhelm the client-side CPU, negamax must be redesigned in a "heartbeat" manner. */
bool negamax(void)
  {
    unsigned char status;
    unsigned int stackLength;

    unsigned int gsIndex;
    NegamaxNode node;
                                                                    //  Retrieve control flags.
    unsigned char controlFlags = inputParametersBuffer[PARAM_BUFFER_COMMAND_OFFSET];

    unsigned char buffer4[4];
    unsigned int i, j;

    //////////////////////////////////////////////////////////////////  Is search halting?
    if(controlFlags & CTRL_HARD_ABORT)                              //  Bail out immediately.
      {
                                                                    //  Set status to aborted.
        inputParametersBuffer[PARAM_BUFFER_STATUS_OFFSET] = STATUS_ABORTED;
        return true;
      }

    //////////////////////////////////////////////////////////////////  Is search halting because of the time budget?
    if(controlFlags & CTRL_STOP_TIME)
      {
        inputParametersBuffer[PARAM_BUFFER_STATUS_OFFSET] = STATUS_STOP_TIME;
        return true;
      }

    //////////////////////////////////////////////////////////////////  Search was stopped deliberately at a heartbeat boundary.
    if(controlFlags & CTRL_STOP_REQUESTED)                          //  Internal buffers are structurally sound, but this particular search
      {                                                             //  did not necessarily produce a completed root result.
        inputParametersBuffer[PARAM_BUFFER_STATUS_OFFSET] = STATUS_STOP_REQUESTED;
        return true;
      }

    //////////////////////////////////////////////////////////////////  Proceed.
    stackLength = restoreNegamaxSearchBufferLength();
    if(stackLength == 0)                                            //  Stack guard.
      {
        inputParametersBuffer[PARAM_BUFFER_STATUS_OFFSET] = STATUS_ERROR;
        return true;
      }

    gsIndex = stackLength - 1;                                      //  Index for top of stack is length minus one.
    restoreNode(gsIndex, &node);                                    //  Restore the node at the top of the stack.

    switch(node.phase)
      {
        //////////////////////////////////////////////////////////////  First operations with a node.
        case _PHASE_ENTER_NODE:                                     //  - Compute the hash for this node.
                                                                    //  - Test whether the given game state is terminal.
                                                                    //  - Probe the transposition table: can we find a value or adjust bounds?
                                                                    //  - Attempt null-move pruning.
          enterNode_step(gsIndex, &node);
          break;

        //////////////////////////////////////////////////////////////  Generate moves/children.
        case _PHASE_GEN_AND_ORDER:                                  //  - Get moves for the given game state (scored fast-n-cheap by Evaluation Module).
                                                                    //  - Update those scores using information from the Negamax Module.
                                                                    //  - Sort moves by these scores.
                                                                    //  - Append sorted (best-first) moves to "negamaxMoveBuffer".
          expansion_step(gsIndex, &node);
          break;

        //////////////////////////////////////////////////////////////  Whichever child-move is next to try for the top node,
                                                                    //  apply it to the game state and make a new node out of it.
        case _PHASE_NEXT_MOVE:                                      //  - Check whether all moves of the given node have been searched already.
                                                                    //  - If not, apply the next move to the node's game state, get a new game state.
                                                                    //  - Make a new node out of this new game state and push it to the node stack.
          nextMove_step(gsIndex, &node);
          break;

        //////////////////////////////////////////////////////////////  Incorporate the node's data into its parent.
        case _PHASE_AFTER_CHILD:                                    //  - Update the node's parent's value.
                                                                    //  - Test for cutoffs.
                                                                    //  - Update killers and history heuristic.
          afterChild_step(gsIndex, &node);
          break;

        //////////////////////////////////////////////////////////////  Write to the Transpo Table.
        case _PHASE_FINISH_NODE:
          finishNode_step(gsIndex, &node);
          break;

        //////////////////////////////////////////////////////////////  Write to outputBuffer and signal search completion.
        case _PHASE_COMPLETE:
          i = 0;
          for(j = 0; j < _GAMESTATE_BYTE_SIZE; j++)                 //  Copy game state to output buffer.
            outputBuffer[i++] = node.gs[j];
                                                                    //  When the root completes, the depth achieved equals the depth requested.
          inputParametersBuffer[PARAM_BUFFER_DEPTHACHIEVED_OFFSET] = inputParametersBuffer[PARAM_BUFFER_TARGETDEPTH_OFFSET];
                                                                    //  Copy depth to output buffer.
          outputBuffer[i++] = inputParametersBuffer[PARAM_BUFFER_DEPTHACHIEVED_OFFSET];

          for(j = 0; j < _MOVE_BYTE_SIZE; j++)                      //  Copy best move for this state, as determined by depth, to output buffer.
            outputBuffer[i++] = node.bestMove[j];

          memcpy(buffer4, (unsigned char*)(&node.value), 4);        //  Force the float into a 4-byte temp buffer.
          for(j = 0; j < 4; j++)                                    //  Copy bytes to output buffer.
            outputBuffer[i++] = buffer4[j];

                                                                    //  Indicate that search is DONE.
          inputParametersBuffer[PARAM_BUFFER_STATUS_OFFSET] = STATUS_DONE;

          break;

        //////////////////////////////////////////////////////////////  Unknown phases are errors.
        default:
          inputParametersBuffer[PARAM_BUFFER_STATUS_OFFSET] = STATUS_ERROR;

          break;
      }

    status = inputParametersBuffer[PARAM_BUFFER_STATUS_OFFSET];
                                                                    //  True: search is complete (or crashed or halted); False: search is ongoing.
    return (status == STATUS_DONE           ||
            status == STATUS_STOP_REQUESTED ||
            status == STATUS_STOP_TIME      ||
            status == STATUS_ABORTED        ||
            status == STATUS_ERROR          );
  }

/* HEARTBEAT NEGAMAX: _PHASE_ENTER_NODE
   Adjudicate node-entry conditions, probe the transposition table, and test for null-move pruning. */
void enterNode_step(unsigned int gsIndex, NegamaxNode* node)
  {
    unsigned char gamestateByteArray[_GAMESTATE_BYTE_SIZE];         //  Store locally for comparison.
    unsigned int negamaxSearchBufferLength;
    //unsigned int occurrences;                                       //  Count occurrences of game states.
    //unsigned char repetitionVerdict;                                //  Returned by historyVerdict().
    signed char R, newDepth;
    NegamaxNode child;
    float standPat;                                                 //  Score at a given moment in search.
    unsigned int i, j;
    bool b_isTerminal;
    //bool b_isSideToMoveInCheck;

    for(i = 0; i < _GAMESTATE_BYTE_SIZE; i++)                       //  Copy unique byte-signature for the current game state to a local buffer.
      gamestateByteArray[i] = node->gs[i];

    //////////////////////////////////////////////////////////////////  Compute the canonical, repetition-path byte array for this node.
    for(i = 0; i < _GAMESTATE_BYTE_SIZE; i++)                       //  Copy "node"s "gs" to "queryGameStateBuffer"
      queryGameStateBuffer[i] = gamestateByteArray[i];              //  for isTerminal() and evaluate().
    copyQuery2EvalGSInput();                                        //  Copy "queryGameStateBuffer" to Evaluation Module's "inputBuffer".

    //repetitionState();                                              //  (Ask the Evaluation Module) Compute the canonical representation for this node.
    //copyEvalRepetitionOutput2AnswerRepetitionBuffer();              //  Copy bytes from Evaluation Engine's repetition-encoding output buffer
                                                                    //               to Negamax's repetition-encoding answer buffer.
    //saveRepetitionState(gsIndex);                                   //  Save the canonical repetition-detection encoding under gsIndex.

    //////////////////////////////////////////////////////////////////  Compute the hash for this node.
    node->zhash = hash(gamestateByteArray);                         //  Zobrist-hash the game state byte array.
    node->hIndex = hashIndex(node->zhash);                          //  Index modulo size of transposition table.

    //////////////////////////////////////////////////////////////////  Terminal test.
                                                                    //  "node"s "gs" is already in the "queryGameStateBuffer".
                                                                    //  And "queryGameStateBuffer" is already in Evaluation Module's "inputBuffer"
    b_isTerminal = isTerminal();                                    //  (Ask the Evaluation Module) Is the given game state terminal?
    if(b_isTerminal)                                                //  - Terminal-state check.
      {
        node->value = evaluate();                                   //  This imported function handles testing the AI's side.
        node->phase = _PHASE_FINISH_NODE;                           //  Mark for the finishing phase.
        incrementNodeCtr();                                         //  Increase node-evaluation counter by 1.
        saveNode(node, gsIndex);                                    //  Save the updated node.
        return;
      }

    //////////////////////////////////////////////////////////////////  Path-dependent repetition adjudication.
    //if(!NN_HAS_FLAG(node, NN_FLAG_REPETITION_DISABLED))
    //  {
    //    occurrences = repetitionOccurrenceCount(gsIndex);           //  Count repetitions.
    //    repetitionVerdict = historyVerdict(occurrences);            //  Ask the Evaluation Module (without passing a game-state byte-array):
    //                                                                //  "Is this occurrence count sufficient to force a draw?"
    //    if(repetitionVerdict == HISTORY_DRAW)                       //  (Defined above.)
    //      {
    //        node->value = 0.0f;                                     //  In CHESS, DRAWS EQUAL ZERO.
    //        node->phase = _PHASE_FINISH_NODE;
    //        NN_SET_FLAG(node, NN_FLAG_PATH_DEPENDENT_RESULT);       //  Flag this repetition-draw evaluation as path-dependent.
    //        incrementNodeCtr();                                     //  This counts as a node evaluation.
    //        saveNode(node, gsIndex);                                //  Save the node.
    //        return;                                                 //  Done here.
    //      }
    //  }

    //////////////////////////////////////////////////////////////////  Transposition-table probe.
    transpoProbe(gsIndex, node);                                    //  Check the transpo table.
    if(node->phase == _PHASE_FINISH_NODE)                           //  Cut-off produced: we're done here.
      return;

    //////////////////////////////////////////////////////////////////  Check whether the side to move is in check.
                                                                    //  "node"s "gs" is already in the "queryGameStateBuffer".
                                                                    //  And "queryGameStateBuffer" is already in Evaluation Module's "inputBuffer"
    //b_isSideToMoveInCheck = isSideToMoveInCheck();                  //  (Ask the Evaluation Module) Is the side to move in the given game state in check?
    //if(b_isSideToMoveInCheck)
    //  NN_SET_FLAG(node, NN_FLAG_IN_CHECK);
    //else
    //  NN_CLEAR_FLAG(node, NN_FLAG_IN_CHECK);

    //////////////////////////////////////////////////////////////////  Leaf-node test.
    if(node->depth <= 0)
      {
        if(node->depth <= -(signed char)_QUIESCENCE_MAX_PLY)        //  Prevent runaway quiescence search extensions.
          {
                                                                    //  "node"s "gs" is already in the "queryGameStateBuffer".
                                                                    //  And "queryGameStateBuffer" is already in Evaluation Module's "inputBuffer"
            node->value = evaluate();
            node->phase = _PHASE_FINISH_NODE;
            incrementNodeCtr();
            saveNode(node, gsIndex);
            return;
          }

        if(!NN_HAS_FLAG(node, NN_FLAG_IN_CHECK))
          {
                                                                    //  "node"s "gs" is already in the "queryGameStateBuffer".
                                                                    //  And "queryGameStateBuffer" is already in Evaluation Module's "inputBuffer"
            standPat = evaluate();                                  //  Evaluation as if this were the end of search.
            incrementNodeCtr();                                     //  That evaluation counts.

            if(standPat >= node->beta)                              //  Stand-pat cutoff.
              {
                node->value = node->beta;                           //  Standard quiescence search returns beta on cutoff.
                node->phase = _PHASE_FINISH_NODE;
                saveNode(node, gsIndex);
                return;
              }
            if(standPat > node->alpha)                              //  Improve alpha using stand-pat.
              node->alpha = standPat;
            node->value = standPat;                                 //  Baseline best score is stand-pat (since "do nothing" is allowed when not in check).
          }
        else                                                        //  In check: standing pat is NOT allowed.
          {
            node->value = -std::numeric_limits<float>::infinity();
            node->bestMove[0] = _NONE;
          }

        node->phase = _PHASE_GEN_AND_ORDER;                         //  Now search noisy replies (captures/promos), or evasions if in check.
        saveNode(node, gsIndex);
        return;
      }

    //////////////////////////////////////////////////////////////////  Attempt null-move pruning.
                                                                    //  Conditions are correct to try null-move pruning.
    if( !NN_HAS_FLAG(node, NN_FLAG_NULL_TRIED) && !NN_HAS_FLAG(node, NN_FLAG_IS_NULL_CHILD) &&
        !NN_HAS_FLAG(node, NN_FLAG_AT_ROOT)    && !NN_HAS_FLAG(node, NN_FLAG_IS_PV)         &&
        !NN_HAS_FLAG(node, NN_FLAG_IN_CHECK)   && node->depth >= 3                           )
      {
                                                                    //  - Null-move pruning.
        NN_SET_FLAG(node, NN_FLAG_NULL_TRIED);                      //  Indicate that we did try the null move for this node.
        NN_SET_FLAG(node, NN_FLAG_NULL_IN_PROGRESS);                //  Indicate that this node's null-child is (WILL BE) on the stack.
        R = NULL_MOVE_BASE_REDUCTION;
        if(node->depth >= 6)
          R += NULL_MOVE_EXTRA_REDUCTION;
        newDepth = node->depth - 1 - R;
        if(newDepth < 0)
          newDepth = 0;

        makeNullMove();                                             //  (Ask the Evaluation Module) To make the null-move.
        copyEvalOutput2AnswerGSBuffer();                            //  Copy Evaluation Module's post-null-move game state to Negamax's GS answer buffer.
                                                                    //  Save the current length, before addition of child nodes.
        negamaxSearchBufferLength = restoreNegamaxSearchBufferLength();

        i = 0;
        for(j = 0; j < _GAMESTATE_BYTE_SIZE; j++)                   //  Copy child gamestate byte-array to child node.
          child.gs[j] = answerGameStateBuffer[i++];
        child.parent = gsIndex;                                     //  Set child's parent to the given index into "negamaxSearchBuffer".

        child.parentMove[0] = _NONE;                                //  Set child's parent-move to the null move.

        child.bestMove[0] = _NONE;                                  //  Set child's best-move to a blank move.

        child.moveOffset = 0;                                       //  Set child's offset to zero.
        child.moveCount = 0;                                        //  Set child's number of moves to zero.
        child.moveNextPtr = 0;                                      //  Set child's move-to-try-next pointer to zero.
        child.depth = newDepth;                                     //  Set child's depth to "newDepth".
        child.ply = node->ply + 1;                                  //  Set child's ply to node's plus one.

        child.originalAlpha = -node->beta;                          //  Set child's alpha to negative parent's beta.
        child.alpha = -node->beta;
        child.beta = -node->alpha;                                  //  Set child's beta to negative parent's alpha.
        child.zhash = 0L;
        child.hIndex = 0;
        child.phase = _PHASE_ENTER_NODE;                            //  Set child's phase.
        child.flags = 0;
        NN_SET_FLAG(&child, NN_FLAG_IS_NULL_CHILD);                 //  Set the flag that says this is a null-move child.
        NN_SET_FLAG(&child, NN_FLAG_REPETITION_DISABLED);           //  Disable repeated-state checking for null-move children AND their descendants.
        child.value = -std::numeric_limits<float>::infinity();

        node->phase = _PHASE_AFTER_CHILD;
        node->moveNextPtr++;

        saveNode(&child, negamaxSearchBufferLength);                //  Write serialized node to head of "negamaxSearchBuffer".
        negamaxSearchBufferLength++;                                //  Increment "negamaxSearchBufferLength".
        saveNegamaxSearchBufferLength(negamaxSearchBufferLength);   //  Write number of NegamaxNodes in "negamaxSearchBuffer".
        saveNode(node, gsIndex);                                    //  Save the updated node.
      }
    else                                                            //  Conditions are insufficient to try null-move pruning.
      {
        node->phase = _PHASE_GEN_AND_ORDER;                         //  Mark this node as ready for the _PHASE_GEN_AND_ORDER phase.
        saveNode(node, gsIndex);                                    //  Save the updated node.
      }

    return;
  }

/* Attempt to look up the given game state in the transposition table.
   This step will either find the transposition or it won't. */
void transpoProbe(unsigned int gsIndex, NegamaxNode* node)
  {
    TranspoRecord ttRecord;
    bool foundTTLookup;
    bool foundStaleSlot;
    unsigned int original_hIndex;                                   //  For the unlikely case in which we fill the table and search all the way around.
    unsigned int stale_hIndex;
    unsigned char generation;
    unsigned int oldness;
    unsigned int i;

    node->originalAlpha = node->alpha;                              //  Save alpha as given.
    original_hIndex = node->hIndex;                                 //  Save the modulo-index to where this entry WANTS to go.

    foundStaleSlot = false;
    stale_hIndex = 0;
    generation = getGeneration();

    statsIncrement(STATS_TT_PROBES_OFFSET);                         //  Increase the number of TT lookup attempts.

    //  LINEAR PROBING:
    //  We first want to see whether the given game state has already been searched.
    //  So, starting at "hIndex", probe extant TT records until the game-state byte-arrays match.
    //  If you encounter a free space, stop and give up: we have not found a pre-searched state.
    foundTTLookup = false;
    while(!foundTTLookup)
      {
        if(!fetchRecord(node->hIndex, &ttRecord))                   //  Blank slot: the requested position is not in the table.
          {                                                         //  Prefer an earlier stale slot if we encountered one.
            if(foundStaleSlot)
              node->hIndex = stale_hIndex;

            break;
          }

        if(ttRecord.lock == node->zhash)                            //  Occupied slot: first ask whether this is our position.
          {
            foundTTLookup = true;
            break;
          }
                                                                    //  Different position.
                                                                    //  If sufficiently old, remember this as a possible replacement slot--but KEEP PROBING.
        oldness = (generation >= ttRecord.age) ? generation - ttRecord.age : 255 - ttRecord.age + generation;

        if(!foundStaleSlot && oldness >= _TRANSPO_AGE_THRESHOLD)
          {
            stale_hIndex = node->hIndex;
            foundStaleSlot = true;
          }

        if(node->hIndex == (_TRANSPO_TABLE_SIZE - 1))               //  Advance the linear probe.
          node->hIndex = 0;
        else
          node->hIndex++;

        if(node->hIndex == original_hIndex)                         //  Entire table traversed.
          {
            if(foundStaleSlot)
              node->hIndex = stale_hIndex;
            else
              node->hIndex = original_hIndex;

            break;
          }
      }
                                                                    //  Attempt to look up the given game state (under Zobrist hash) in the transposition table.
    if(foundTTLookup)                                               //  HIT! Found this same game state.
      {
        statsIncrement(STATS_TT_HITS_OFFSET);                       //  Increment hit counter.

        ttRecord.age = getGeneration();                             //  This record was useful: keep it current.
                                                                    //  Save this updated (rejuvenated) record back to the byte array.
        serializeTranspoRecord(&ttRecord, transpositionTableBuffer + 1 + node->hIndex * _TRANSPO_RECORD_BYTE_SIZE);
                                                                    //  Even if it turns out that this is not a cut-off, the best move stored here
                                                                    //  may still be a hint.
        if(ttRecord.bestMove[0] < _NONE)
          {
            for(i = 0; i < _MOVE_BYTE_SIZE; i++)                    //  Copy from TT record to Node's best move.
              node->bestMove[i] = ttRecord.bestMove[i];
            saveNode(node, gsIndex);
          }

        if(ttRecord.depth >= node->depth)                           //  The record has been ratified from a greater depth.
          {
            statsIncrement(STATS_TT_DEPTH_QUALIFIED_OFFSET);        //  Increment depth-qualified count.

            if(ttRecord.type == NODE_TYPE_PV)                       //  Exact.
              {
                statsIncrement(STATS_TT_CUTOFFS_OFFSET);            //  Increment cutoff count.

                node->value = ttRecord.score;                       //  This node's return value furnished by the transpo lookup.
                node->phase = _PHASE_FINISH_NODE;                   //  Set node's phase to Parent-Update.
                saveNode(node, gsIndex);                            //  Save the updated node.
                return;
              }
            else if(ttRecord.type == NODE_TYPE_CUT)                 //  Lower Bound.
              node->alpha = std::max(node->alpha, ttRecord.score);
            else if(ttRecord.type == NODE_TYPE_ALL)                 //  Upper Bound.
              node->beta = std::min(node->beta, ttRecord.score);

            if(node->alpha >= node->beta)
              {
                statsIncrement(STATS_TT_CUTOFFS_OFFSET);            //  Increment cutoff count.

                if(ttRecord.type == NODE_TYPE_CUT)                  //  Lower bound caused fail-high.
                  node->value = node->beta;
                else                                                //  Upper bound caused fail-low.
                  node->value = node->alpha;

                node->phase = _PHASE_FINISH_NODE;                   //  Set node's phase to Parent-Update.
                saveNode(node, gsIndex);                            //  Save the updated node.
                return;
              }
          }
      }

    return;
  }

/* HEARTBEAT NEGAMAX: _PHASE_GEN_AND_ORDER
   Generate the moves available from the given node and append them to the arena. */
void expansion_step(unsigned int gsIndex, NegamaxNode* node)
  {
    unsigned int negamaxMoveBufferLength;                           //  Size of the MOVE stack.
    unsigned int numMoves;

    NegamaxMove movesBuffer[_MAX_MOVES];                            //  Local storage to be filled, sorted, then appended to "negamaxMovesBuffer".
    signed int scores[_MAX_MOVES];                                  //  Scores furnished by both Evaluation Module (SEE, promotion, check)
                                                                    //  and by Negamax Module (TT best move, killer move, history heuristic).
    NegamaxMove move;
    signed int score;

    unsigned char killerFlag;

    unsigned char buffer4[4];                                       //  Convert 4-byte arrays to their proper data types.
    signed int si4;
    //unsigned char toMove;

    unsigned int outCount;
    bool isQ;
    unsigned char ttHint[_MOVE_BYTE_SIZE];

    unsigned int i, j, answerBufferCtr;

    negamaxMoveBufferLength = restoreNegamaxMoveBufferLength();     //  Save the current length of the MOVE stack, before addition of moves.

    isQ = (node->depth <= 0);                                       //  Is the given node a quiescence-search extension?

    for(j = 0; j < _MOVE_BYTE_SIZE; j++)                            //
      ttHint[j] = node->bestMove[j];

    for(i = 0; i < _GAMESTATE_BYTE_SIZE; i++)                       //  Copy "node"s "gs" to "queryGameStateBuffer" for getSortedMoves().
      queryGameStateBuffer[i] = node->gs[i];

    copyQuery2EvalGSInput();                                        //  Copy Negamax Module's "queryGameStateBuffer" to Evaluation Module's "inputBuffer".

    //toMove = sideToMove();                                          //  (Ask the Evaluation Module) Which side is to move?
    numMoves = getMoves();                                          //  (Ask the Evaluation Module) Generate SEE-scored list of moves.
                                                                    //  (These are scored, quick-n-cheap, BUT NOT SORTED.)

                                                                    //  Only reset value/bestMove for NORMAL (not quiescence-extension) nodes.
    if(!isQ)                                                        //  In quiescence search, we already set node->value to standPat
      {                                                             //  (or -inf in check) in enterNode_step.
        node->value = -std::numeric_limits<float>::infinity();      //  Cause the first legal move to be the best found so far.
        node->bestMove[0] = _NONE;
      }

    copyEvalOutput2AnswerMovesBuffer( numMoves );                   //  Copy from Evaluation Module's output buffer to Negamax Module's "answerMovesBuffer".

    answerBufferCtr = 0;                                            //  Convert each run of (_GAMESTATE_BYTE_SIZE + _MOVE_BYTE_SIZE) bytes in "answerMovesBuffer"
    outCount = 0;
    for(i = 0; i < numMoves; i++)                                   //  to a NegamaxNode and append them to "negamaxSearchBuffer".
      {
        for(j = 0; j < _MOVE_BYTE_SIZE; j++)                        //  Retrieve the bytes that define the move.
          move.moveByteArray[j] = answerMovesBuffer[answerBufferCtr++];

        for(j = 0; j < 4; j++)                                      //  Copy the 4 bytes of the move's SEE score from the global buffer.
          buffer4[j] = answerMovesBuffer[answerBufferCtr++];
        memcpy(&si4, buffer4, 4);                                   //  Force the 4-byte buffer into a SIGNED int.
        score = si4;                                                //  Save the rough, SEE score to a local array.

        move.quietMove = answerMovesBuffer[answerBufferCtr++];      //  0: quiet; 1: capture or promotion.

        if(isQ)                                                     //  Quiescence filtering:
          {                                                         //  - if in check: keep ALL legal moves (evasions)
                                                                    //  - else: keep only noisy moves (captures/promotions)
            if(move.quietMove == MOVEFLAG_QUIET)                    //  Quiet ==> Skip in quiescence search.
              continue;
          }
                                                                    //  TT best-move bump (use ttHint because node->bestMove may have been cleared).
        if(ttHint[0] != _NONE && move.moveByteArray[0] == ttHint[0])
          score += MOVE_SORTING_TRANSPO_BEST_MOVE_BONUS;

        if(!isQ && move.quietMove == MOVEFLAG_QUIET)                //  Killer/history (not applied to quiescence search.)
          {
            killerFlag = killerLookup(node->ply, node->gs[_ANCHOR_BYTE_OFFSET], move.moveByteArray[0]);
            if(killerFlag == KILLER_FOUND_FIRST)
              score += MOVE_SORTING_KILLER_MOVE_1_BONUS;
            else if(killerFlag == KILLER_FOUND_SECOND)
              score += MOVE_SORTING_KILLER_MOVE_2_BONUS;
            score += historyLookup(node->gs[_ANCHOR_BYTE_OFFSET], move.moveByteArray[0]);
          }

        movesBuffer[outCount] = move;                               //  Keep it.
        scores[outCount] = score;
        outCount++;
      }

    node->moveOffset = negamaxMoveBufferLength;                     //  The offset into "negamaxMovesBuffer" of where this node's child-moves begin.
    node->moveCount = outCount;                                     //  Overwrite moveCount with the filtered count.
    node->moveNextPtr = 0;

    if(node->moveCount == 0)                                        //  If there is nothing to search:
      {                                                             //  - In quiescence search when not in check:
                                                                    //    node->value already holds stand-pat from enterNode_step()
                                                                    //  - In check: if truly mated, isTerminal() should have caught it;
                                                                    //    otherwise finish with whatever baseline you set.
        node->phase = _PHASE_FINISH_NODE;
        saveNode(node, gsIndex);
        return;
      }

    quicksort(true, scores, movesBuffer, 0, node->moveCount - 1);   //  Sort DESCENDING according to (shallow) evaluation.

    for(i = 0; i < node->moveCount; i++)                            //  Write the sorted move-data to the global buffer.
      {
        saveMove(movesBuffer + i, negamaxMoveBufferLength);         //  Write the move-data at the (current) end of the "stack".
        negamaxMoveBufferLength++;                                  //  Increase the length of the "stack".
      }

    saveNegamaxMoveBufferLength(negamaxMoveBufferLength);           //  Write the new, expanded length of the "stack".

    node->phase = _PHASE_NEXT_MOVE;                                 //  On next pulse, take the next child.
    saveNode(node, gsIndex);                                        //  Save the newly expanded node.

    return;
  }

/* HEARTBEAT NEGAMAX: _PHASE_NEXT_MOVE
   Push the next move to be explored. */
void nextMove_step(unsigned int gsIndex, NegamaxNode* node)
  {
    unsigned int negamaxSearchBufferLength;                         //  Size of the NODE stack.
    unsigned int globalMoveIndex;                                   //  Compute the global offset of NegamaxMoves into "negamaxMovesBuffer".
    NegamaxNode child;
    NegamaxMove move;
    unsigned int i;

    negamaxSearchBufferLength = restoreNegamaxSearchBufferLength(); //  Save the current length of the NODE stack, before addition of a child node.

    if(node->moveNextPtr >= node->moveCount)                        //  If there are no more moves, then we're done generating children for this node.
      {                                                             //  ( >= rather than == is the "belt + suspenders" approach.)
        node->phase = _PHASE_FINISH_NODE;
        saveNode(node, gsIndex);
        return;                                                     //  End heartbeat.
      }

    globalMoveIndex = node->moveOffset + node->moveNextPtr;         //  Index into "negamaxMovesBuffer".
    restoreMove(globalMoveIndex, &move);                            //  Recover the NegamaxMove struct from the global byte-array.

    node->moveNextPtr++;                                            //  Point to the next move.
    node->phase = _PHASE_AFTER_CHILD;                               //  Advance this node's phase.
    saveNode(node, gsIndex);                                        //  Write the updated node.

    for(i = 0; i < _GAMESTATE_BYTE_SIZE; i++)                       //  Copy "node"s "gs" to "queryGameStateBuffer" for makeMove().
      queryGameStateBuffer[i] = node->gs[i];
    copyQuery2EvalGSInput();                                        //  Copy Negamax Module's "queryGameStateBuffer" to Evaluation Module's "inputGameStateBuffer".

    for(i = 0; i < _MOVE_BYTE_SIZE; i++)                            //  Copy the byte-array of the move to be made to "queryMoveBuffer" for makeMove().
      queryMoveBuffer[i] = move.moveByteArray[i];
    copyQuery2EvalMoveInput();                                      //  Copy Negamax Module's "queryMoveBuffer" to Evaluation Module's "inputMoveBuffer".

    makeMove();                                                     //  (Ask the Evaluation Module) Apply the move to the game state.

    copyEvalOutput2AnswerGSBuffer();                                //  Copy Evaluation Module's "outputGameStateBuffer" to Negamax Module's "answerGameStateBuffer".

    for(i = 0; i < _GAMESTATE_BYTE_SIZE; i++)                       //  Copy "answerGameStateBuffer" to child's "gs".
      child.gs[i] = answerGameStateBuffer[i];

    child.parent = gsIndex;                                         //  Fill in the child node, descended from the given node.
    child.parentMove[0] = move.moveByteArray[0];                    //  Move the parent made to get here.

    child.bestMove[0] = _NONE;                                      //  Blank.

    child.moveOffset = 0;                                           //  Blank.
    child.moveCount = 0;
    child.moveNextPtr = 0;

    child.depth = (signed char)(node->depth - 1);                   //  Depth/ply bookkeeping.
    child.ply = (unsigned char)(node->ply + 1);

    child.originalAlpha = -node->beta;                              //  Negamax window flip.
    child.alpha = -node->beta;
    child.beta = -node->alpha;
    child.value = -std::numeric_limits<float>::infinity();

    child.zhash = 0L;
    child.hIndex = 0;

    child.phase = _PHASE_ENTER_NODE;                                //  Receive at this phase.
    child.flags = 0;
    if(NN_HAS_FLAG(node, NN_FLAG_REPETITION_DISABLED))              //  Repetition remains disabled throughout a subtree descended from an artificial null move.
      NN_SET_FLAG(&child, NN_FLAG_REPETITION_DISABLED);

    saveNode(&child, negamaxSearchBufferLength);                    //  Encode this new node at the end of the stack.
    saveNegamaxSearchBufferLength(negamaxSearchBufferLength + 1);   //  Increment the length of the stack.

    return;                                                         //  End heartbeat.
  }

/* HEARTBEAT NEGAMAX: _PHASE_AFTER_CHILD
   Incorporate the node's data into its parent. */
void afterChild_step(unsigned int gsIndex, NegamaxNode* node)
  {
    NegamaxNode parent;
    unsigned int parentIndex;
    NegamaxMove move;
    unsigned int moveIndex;
    unsigned int negamaxSearchBufferLength;
    //unsigned char toMove;
    float score;
    unsigned int i;

    bool bestUnset;
    bool valueUnset;
    bool preferPathIndependentTie;                                  //  Prefer an equal score that does not depend on search history.
    float negInf = -std::numeric_limits<float>::infinity();         //  Ensure that a "best move" is stored, even if all positions are losing.

    parentIndex = node->parent;                                     //  Retrieve the index of the parent of the given node.
    restoreNode(parentIndex, &parent);                              //  Restore the parent of the given node.

    bestUnset = (parent.bestMove[0] == _NONE);
    valueUnset = (parent.value == negInf);

    if(parentIndex == gsIndex)                                      //  Make sure this isn't the root node.
      {
                                                                    //  Indicate error if it is.
        inputParametersBuffer[PARAM_BUFFER_STATUS_OFFSET] = STATUS_ERROR;
        return;
      }

    score = -node->value;                                           //  Negamax principle.
    preferPathIndependentTie = ( score == parent.value                               &&
                                 NN_HAS_FLAG(&parent, NN_FLAG_PATH_DEPENDENT_RESULT) &&
                                !NN_HAS_FLAG(node, NN_FLAG_PATH_DEPENDENT_RESULT)    );

    //////////////////////////////////////////////////////////////////  Null-Move Child.
    if(NN_HAS_FLAG(node, NN_FLAG_IS_NULL_CHILD))
      {
        NN_CLEAR_FLAG(&parent, NN_FLAG_NULL_IN_PROGRESS);           //  Clear out the bookkeeping: null child is done.
                                                                    //  Since we incremented moveNextPtr when this null-child was spawned, undo that now.
        parent.moveNextPtr = 0;                                     //  Null-move is NOT a real move from the move list!
        if(score >= parent.beta)                                    //  Null-move pruning decision: fail-high => cutoff.
          {
            parent.value = score;
            parent.alpha = std::max(parent.alpha, score);
            parent.phase = _PHASE_FINISH_NODE;
          }
        else                                                        //  No cutoff: proceed to generate/order the real moves.
          parent.phase = _PHASE_GEN_AND_ORDER;

        saveNode(&parent, parentIndex);
                                                                    //  Pop this null-child.
        negamaxSearchBufferLength = restoreNegamaxSearchBufferLength();
        saveNegamaxSearchBufferLength(negamaxSearchBufferLength - 1);
        return;
      }

    //////////////////////////////////////////////////////////////////  Normal Child.
    //for(i = 0; i < _GAMESTATE_BYTE_SIZE; i++)                       //  Copy "parent"s "gs" to "queryGameStateBuffer" for sideToMove().
    //  queryGameStateBuffer[i] = parent.gs[i];
    //copyQuery2EvalGSInput();                                        //  Copy Negamax Module's "queryGameStateBuffer" to Evaluation Module's "inputBuffer".
    //toMove = sideToMove();                                          //  (Ask the Evaluation Module) Which side is to move?

    moveIndex = parent.moveOffset + parent.moveNextPtr - 1;         //  Retrieve the index of the move the parent made to reach this node.
    restoreMove(moveIndex, &move);                                  //  Restore the move.

                                                                    //  Even if all positions are losing, store a "best move"
                                                                    //  so that we avoid returning the uninitialized (_NONE, _NONE, _NO_PROMO).
    if(score > parent.value || (bestUnset && valueUnset) || preferPathIndependentTie)
      {
        parent.value = score;
        for(i = 0; i < _MOVE_BYTE_SIZE; i++)
          parent.bestMove[i] = node->parentMove[i];
                                                                    //  The parent's result inherits the history-dependence of the child
                                                                    //  that currently supplies its best value.
        if(NN_HAS_FLAG(node, NN_FLAG_PATH_DEPENDENT_RESULT))
          NN_SET_FLAG(&parent, NN_FLAG_PATH_DEPENDENT_RESULT);
        else
          NN_CLEAR_FLAG(&parent, NN_FLAG_PATH_DEPENDENT_RESULT);
      }

    if(score > parent.alpha)                                        //  Update parent's alpha.
      parent.alpha = score;
    if(parent.alpha >= parent.beta)                                 //  Cutoff.
      {
        if(parent.depth > 0 && move.quietMove == MOVEFLAG_QUIET)    //  The move is "quiet": it is not a capture, not a promotion.
          {
                                                                    //  This is a KILLER MOVE!
            killerAdd(parent.ply, parent.gs[_ANCHOR_BYTE_OFFSET], move.moveByteArray[0]);
                                                                    //  Update the HISTORY HEURISTIC.
            historyUpdate(parent.gs[_ANCHOR_BYTE_OFFSET], move.moveByteArray[0], (unsigned char)parent.depth);
          }

        parent.phase = _PHASE_FINISH_NODE;                          //  Parent's work is done.
      }
    else                                                            //  Not a cutoff.
      {
        if(parent.moveNextPtr >= parent.moveCount)
          parent.phase = _PHASE_FINISH_NODE;
        else
          parent.phase = _PHASE_NEXT_MOVE;
      }

    saveNode(&parent, parentIndex);                                 //  Save the parent.

    negamaxSearchBufferLength = restoreNegamaxSearchBufferLength(); //  Recover the length of the node stack.
    saveNegamaxSearchBufferLength(negamaxSearchBufferLength - 1);   //  Shorten the length of the node stack; effectively "popping" this node.

    return;
  }

/* HEARTBEAT NEGAMAX: _PHASE_FINISH_NODE
   Conditionally write to the Transpo Table and finish this node. */
void finishNode_step(unsigned int gsIndex, NegamaxNode* node)
  {
    TranspoRecord ttEntry;
    unsigned char currGen;
    unsigned char oldness;

    bool samePosition;
    bool replace;

    float v, a0, b;
    unsigned char ttType;
    unsigned int i;

    if(!NN_HAS_FLAG(node, NN_FLAG_PATH_DEPENDENT_RESULT))
      {
        v = node->value;
        a0 = node->originalAlpha;
        b = node->beta;

        if(v <= a0)
          ttType = NODE_TYPE_ALL;                                   //  Upper bound (fail-low).
        else if(v >= b)
          ttType = NODE_TYPE_CUT;                                   //  Lower bound (fail-high).
        else
          ttType = NODE_TYPE_PV;                                    //  Exact.

        currGen = getGeneration();                                  //  Get the current transposition-table generation.
                                                                    //  Recover whatever's at this address.
        deserializeTranspoRecord(transpositionTableBuffer + 1 + node->hIndex * _TRANSPO_RECORD_BYTE_SIZE, &ttEntry);
        oldness = generationAge(currGen, ttEntry.age);
        samePosition = (ttEntry.age != 0 && ttEntry.lock == node->zhash);
        replace = false;
                                                                    //  Either:
                                                                    //    This slot was free. Write.
                                                                    //  Or:
                                                                    //    This slot was occupied but too old to be useful anymore. Overwrite.
                                                                    //  Or:
                                                                    //    This slot was occupied but our information now comes from a deeper depth. Overwrite.
        if(ttEntry.age == 0)                                        //  Free slot.
          replace = true;
        else if(samePosition)                                       //  Same position: preserve the stronger information.
          {
            if(node->depth > ttEntry.depth)
              replace = true;                                       //  New search is deeper.

            else if(node->depth == ttEntry.depth)
              {
                if(ttType == NODE_TYPE_PV)                          //  Exact information is always worth keeping at equal depth.
                  replace = true;
                else if(ttEntry.type == NODE_TYPE_PV)
                  replace = false;
                else if(ttType == NODE_TYPE_CUT && ttEntry.type == NODE_TYPE_CUT)
                  replace = (node->value > ttEntry.score);          //  Higher lower-bound is stronger.
                else if(ttType == NODE_TYPE_ALL && ttEntry.type == NODE_TYPE_ALL)
                  replace = (node->value < ttEntry.score);          //  Lower upper-bound is stronger.
                else
                  replace = true;                                   //  Different bound kinds.
              }
          }
                                                                    //  Different position: transpoProbe() normally sends us here
        else                                                        //  only because this is its chosen replacement slot.
          {
            if(oldness >= _TRANSPO_AGE_THRESHOLD)
              replace = true;
            else if(ttEntry.depth <= node->depth)
              replace = true;                                       //  Full-table/no-stale fallback: prefer the new record if at least as deep.
          }

        if(replace)
          {
            ttEntry.lock = node->zhash;                             //  Lock = Zobrist key.

            for(i = 0; i < _MOVE_BYTE_SIZE; i++)                    //  Copy the best move found for this state.
              ttEntry.bestMove[i] = node->bestMove[i];

            ttEntry.depth = node->depth;                            //  Save depth.
            ttEntry.score = node->value;                            //  Save value.
            ttEntry.type = ttType;                                  //  Save the type.
            ttEntry.age = currGen;                                  //  Set the age.
                                                                    //  Conditionally write back to buffer:
                                                                    //  if we didn't actually change anything, why bother writing?
            serializeTranspoRecord(&ttEntry, transpositionTableBuffer + 1 + node->hIndex * _TRANSPO_RECORD_BYTE_SIZE);
          }
      }

    if(node->moveCount > 0)                                         //  Only if this node generated moves at all (rather than early exiting).
      saveNegamaxMoveBufferLength(node->moveOffset);                //  Roll back the moves arena.

    if(node->parent == gsIndex)                                     //  Only the ROOT NODE has itself as its own parent.
      node->phase = _PHASE_COMPLETE;                                //  Prepare to write final output and report completion.
    else                                                            //  Else: set node phase to _PHASE_AFTER_CHILD so that this child updates its parent
      node->phase = _PHASE_AFTER_CHILD;                             //  and then pops itself.

    saveNode(node, gsIndex);
    return;
  }

/**************************************************************************************************
 Quicksort  */

void quicksort(bool desc, signed int* scores, NegamaxMove* moves, unsigned int lo, unsigned int hi)
  {
    unsigned int p;

    if(lo < hi)
      {
        p = partition(desc, scores, moves, lo, hi);

        if(p > 0)                                                   //  PREVENT ROLL-OVER TO 0xFFFF.
          quicksort(desc, scores, moves, lo, p - 1);                //  Left side: start quicksort.
        if(p < 65535)                                               //  PREVENT ROLL-OVER TO 0x0000.
          quicksort(desc, scores, moves, p + 1, hi);                //  Right side: start quicksort.
      }

    return;
  }

unsigned int partition(bool desc, signed int* scores, NegamaxMove* moves, unsigned int lo, unsigned int hi)
  {
    signed int pivot = scores[hi];
    unsigned int i = lo;
    unsigned int j;
    unsigned char k;
    signed int tmpSignedInt;
    NegamaxMove tmpMove;
    bool trigger;

    for(j = lo; j < hi; j++)
      {
        if(desc)
          trigger = (scores[j] > pivot);                            //  SORT DESCENDING.
        else
          trigger = (scores[j] < pivot);                            //  SORT ASCENDING.

        if(trigger)
          {
            tmpSignedInt = scores[i];                               //  Swap scores[i] with scores[j].
            scores[i]    = scores[j];
            scores[j]    = tmpSignedInt;

            for(k = 0; k < _MOVE_BYTE_SIZE; k++)                    //  tmp gets [i].
              tmpMove.moveByteArray[k] = moves[i].moveByteArray[k];
            tmpMove.quietMove = moves[i].quietMove;

            for(k = 0; k < _MOVE_BYTE_SIZE; k++)                    //  [i] gets [j].
              moves[i].moveByteArray[k] = moves[j].moveByteArray[k];
            moves[i].quietMove = moves[j].quietMove;

            for(k = 0; k < _MOVE_BYTE_SIZE; k++)                    //  [j] gets tmp.
              moves[j].moveByteArray[k] = tmpMove.moveByteArray[k];
            moves[j].quietMove = tmpMove.quietMove;

            i++;
          }
      }

    tmpSignedInt = scores[i];                                       //  Swap scores[i] with scores[hi].
    scores[i]    = scores[hi];
    scores[hi]   = tmpSignedInt;

    for(k = 0; k < _MOVE_BYTE_SIZE; k++)                            //  tmp gets [i].
      tmpMove.moveByteArray[k] = moves[i].moveByteArray[k];
    tmpMove.quietMove = moves[i].quietMove;

    for(k = 0; k < _MOVE_BYTE_SIZE; k++)                            //  [i] gets [hi].
      moves[i].moveByteArray[k] = moves[hi].moveByteArray[k];
    moves[i].quietMove = moves[hi].quietMove;

    for(k = 0; k < _MOVE_BYTE_SIZE; k++)                            //  [hi] gets tmp.
      moves[hi].moveByteArray[k] = tmpMove.moveByteArray[k];
    moves[hi].quietMove = tmpMove.quietMove;

    return i;
  }

/**************************************************************************************************
 Search-buffer functions  */

/* Read the first 4 bytes from "negamaxSearchBuffer" and return the unsigned int they describe. */
unsigned int restoreNegamaxSearchBufferLength(void)
  {
    unsigned char buffer4[4];
    unsigned int ui4;
    unsigned char i;

    for(i = 0; i < 4; i++)                                          //  Copy the first 4 bytes from the global buffer.
      buffer4[i] = negamaxSearchBuffer[i];
    memcpy(&ui4, buffer4, 4);                                       //  Force the 4-byte buffer into an unsigned int.
    return ui4;                                                     //  Return the unsigned int.
  }

/* Read the first 4 bytes from "negamaxMovesBuffer" and return the unsigned int they describe. */
unsigned int restoreNegamaxMoveBufferLength(void)
  {
    unsigned char buffer4[4];
    unsigned int ui4;
    unsigned char i;

    for(i = 0; i < 4; i++)                                          //  Copy the first 4 bytes from the global buffer.
      buffer4[i] = negamaxMovesBuffer[i];
    memcpy(&ui4, buffer4, 4);                                       //  Force the 4-byte buffer into an unsigned int.
    return ui4;                                                     //  Return the unsigned int.
  }

/* Write the given "len" to the first 4 bytes of "negamaxSearchBuffer". */
void saveNegamaxSearchBufferLength(unsigned int len)
  {
    unsigned char buffer4[4];
    unsigned char i;

    memcpy(buffer4, (unsigned char*)(&len), 4);                     //  Force unsigned int into 4-byte unsigned char buffer.
    for(i = 0; i < 4; i++)                                          //  Write 4-byte buffer to "negamaxSearchBuffer".
      negamaxSearchBuffer[i] = buffer4[i];

    return;
  }

/* Write the given "len" to the first 4 bytes of "negamaxMovesBuffer". */
void saveNegamaxMoveBufferLength(unsigned int len)
  {
    unsigned char buffer4[4];
    unsigned char i;

    memcpy(buffer4, (unsigned char*)(&len), 4);                     //  Force unsigned int into 4-byte unsigned char buffer.
    for(i = 0; i < 4; i++)                                          //  Write 4-byte buffer to "negamaxMovesBuffer".
      negamaxMovesBuffer[i] = buffer4[i];

    return;
  }

/* In "negamaxSearchBuffer", at an offset of "index" * sizeof(NegamaxNode), read the bytes there into the given struct, "node". */
void restoreNode(unsigned int index, NegamaxNode* node)
  {
    unsigned char buffer[_NEGAMAX_NODE_BYTE_SIZE];
    unsigned char buffer4[4];
    unsigned char buffer8[8];
    unsigned int ui4;
    float f4;
    unsigned long long ull8;

    unsigned int i, j;

    for(i = 0; i < _NEGAMAX_NODE_BYTE_SIZE; i++)                    //  Copy a slice of "negamaxSearchBuffer" to the local "buffer".
      buffer[i] = negamaxSearchBuffer[index * _NEGAMAX_NODE_BYTE_SIZE + 4 + i];

    i = 0;                                                          //  Reset "i" to iterate through local buffer.
    for(j = 0; j < _GAMESTATE_BYTE_SIZE; j++)                       //  Recover node's gamestate byte array from local buffer.
      node->gs[j] = buffer[i++];                                    //  NODE.GS

    for(j = 0; j < 4; j++)                                          //  Copy 4 bytes from the local buffer.
      buffer4[j] = buffer[i++];
    memcpy(&ui4, buffer4, 4);                                       //  Force the 4-byte buffer into an unsigned int.
    node->parent = ui4;                                             //  NODE.PARENT

    for(j = 0; j < _MOVE_BYTE_SIZE; j++)                            //  Recover node's parent-move from local buffer.
      node->parentMove[j] = buffer[i++];                            //  NODE.PARENTMOVE

    for(j = 0; j < _MOVE_BYTE_SIZE; j++)                            //  Recover node's best-move from local buffer.
      node->bestMove[j] = buffer[i++];                              //  NODE.BESTMOVE

    for(j = 0; j < 4; j++)                                          //  Copy 4 bytes from the local buffer.
      buffer4[j] = buffer[i++];
    memcpy(&ui4, buffer4, 4);                                       //  Force the 4-byte buffer into an unsigned int.
    node->moveOffset = ui4;                                         //  NODE.MOVEOFFSET

    for(j = 0; j < 4; j++)                                          //  Copy 4 bytes from the local buffer.
      buffer4[j] = buffer[i++];
    memcpy(&ui4, buffer4, 4);                                       //  Force the 4-byte buffer into an unsigned int.
    node->moveCount = ui4;                                          //  NODE.MOVECOUNT

    for(j = 0; j < 4; j++)                                          //  Copy 4 bytes from the local buffer.
      buffer4[j] = buffer[i++];
    memcpy(&ui4, buffer4, 4);                                       //  Force the 4-byte buffer into an unsigned int.
    node->moveNextPtr = ui4;                                        //  NODE.MOVENEXTPTR

    node->depth = (signed char)buffer[i++];                         //  NODE.DEPTH
    node->ply = (unsigned char)buffer[i++];                         //  NODE.PLY

    for(j = 0; j < 4; j++)                                          //  Copy 4 bytes from the local buffer.
      buffer4[j] = buffer[i++];
    memcpy(&f4, buffer4, 4);                                        //  Force the 4-byte buffer into a float.
    node->originalAlpha = f4;                                       //  NODE.ORIGINALALPHA

    for(j = 0; j < 4; j++)                                          //  Copy 4 bytes from the local buffer.
      buffer4[j] = buffer[i++];
    memcpy(&f4, buffer4, 4);                                        //  Force the 4-byte buffer into a float.
    node->alpha = f4;                                               //  NODE.ALPHA

    for(j = 0; j < 4; j++)                                          //  Copy 4 bytes from the local buffer.
      buffer4[j] = buffer[i++];
    memcpy(&f4, buffer4, 4);                                        //  Force the 4-byte buffer into a float.
    node->beta = f4;                                                //  NODE.BETA

    for(j = 0; j < 4; j++)                                          //  Copy 4 bytes from the local buffer.
      buffer4[j] = buffer[i++];
    memcpy(&f4, buffer4, 4);                                        //  Force the 4-byte buffer into a float.
    node->value = f4;                                               //  NODE.VALUE

    for(j = 0; j < 8; j++)                                          //  Copy 8 bytes from the local buffer.
      buffer8[j] = buffer[i++];
    memcpy(&ull8, buffer8, 8);                                      //  Force the 8-byte buffer into an unsigned long long.
    node->zhash = ull8;                                             //  NODE.ZHASH

    for(j = 0; j < 4; j++)                                          //  Copy 4 bytes from the local buffer.
      buffer4[j] = buffer[i++];
    memcpy(&ui4, buffer4, 4);                                       //  Force the 4-byte buffer into an unsigned int.
    node->hIndex = ui4;                                             //  NODE.HINDEX

    node->phase = buffer[i++];                                      //  NODE.PHASE
    node->flags = buffer[i++];                                      //  NODE.FLAGS

    return;
  }

/* Write bytes for the given struct "node" to "negamaxSearchBuffer" at an offset of "index" * sizeof(NegamaxNode). */
void saveNode(NegamaxNode* node, unsigned int index)
  {
    unsigned char buffer[_NEGAMAX_NODE_BYTE_SIZE];
    unsigned char buffer4[4];
    unsigned char buffer8[8];
    unsigned int ui4;
    float f4;
    unsigned long long ull8;

    unsigned int i, j;

    i = 0;
    for(j = 0; j < _GAMESTATE_BYTE_SIZE; j++)                       //  Copy node's gamestate byte array to buffer.
      buffer[i++] = node->gs[j];                                    //  NODE.GS

    ui4 = node->parent;                                             //  Copy node's parent index to buffer.
    memcpy(buffer4, (unsigned char*)(&ui4), 4);                     //  Force unsigned int into 4-byte unsigned char buffer.
    for(j = 0; j < 4; j++)                                          //  Append 4-byte buffer to buffer.
      buffer[i++] = buffer4[j];                                     //  NODE.PARENT

    for(j = 0; j < _MOVE_BYTE_SIZE; j++)                            //  Copy node's parent-move to buffer.
      buffer[i++] = node->parentMove[j];                            //  NODE.PARENTMOVE

    for(j = 0; j < _MOVE_BYTE_SIZE; j++)                            //  Copy node's best-move to buffer.
      buffer[i++] = node->bestMove[j];                              //  NODE.BESTMOVE

    ui4 = node->moveOffset;                                         //  Copy the node's move-offset to the buffer.
    memcpy(buffer4, (unsigned char*)(&ui4), 4);                     //  Force unsigned int into 4-byte unsigned char buffer.
    for(j = 0; j < 4; j++)                                          //  Append 4-byte buffer to buffer.
      buffer[i++] = buffer4[j];                                     //  NODE.MOVEOFFSET

    ui4 = node->moveCount;                                          //  Copy the node's move-count to the buffer.
    memcpy(buffer4, (unsigned char*)(&ui4), 4);                     //  Force unsigned int into 4-byte unsigned char buffer.
    for(j = 0; j < 4; j++)                                          //  Append 4-byte buffer to buffer.
      buffer[i++] = buffer4[j];                                     //  NODE.MOVECOUNT

    ui4 = node->moveNextPtr;                                        //  Copy the node's move-to-try-next to the buffer.
    memcpy(buffer4, (unsigned char*)(&ui4), 4);                     //  Force unsigned int into 4-byte unsigned char buffer.
    for(j = 0; j < 4; j++)                                          //  Append 4-byte buffer to buffer.
      buffer[i++] = buffer4[j];                                     //  NODE.MOVENEXTPTR

    buffer[i++] = (unsigned char)node->depth;                       //  NODE.DEPTH
    buffer[i++] = (unsigned char)node->ply;                         //  NODE.PLY

    f4 = node->originalAlpha;                                       //  Copy the node's originalAlpha to the buffer.
    memcpy(buffer4, (unsigned char*)(&f4), 4);                      //  Force float into 4-byte unsigned char buffer.
    for(j = 0; j < 4; j++)                                          //  Append 4-byte buffer to buffer.
      buffer[i++] = buffer4[j];                                     //  NODE.ORIGINALALPHA

    f4 = node->alpha;                                               //  Copy the node's alpha to the buffer.
    memcpy(buffer4, (unsigned char*)(&f4), 4);                      //  Force float into 4-byte unsigned char buffer.
    for(j = 0; j < 4; j++)                                          //  Append 4-byte buffer to buffer.
      buffer[i++] = buffer4[j];                                     //  NODE.ALPHA

    f4 = node->beta;                                                //  Copy the node's beta to the buffer.
    memcpy(buffer4, (unsigned char*)(&f4), 4);                      //  Force float into 4-byte unsigned char buffer.
    for(j = 0; j < 4; j++)                                          //  Append 4-byte buffer to buffer.
      buffer[i++] = buffer4[j];                                     //  NODE.BETA

    f4 = node->value;                                               //  Copy the node's value to the buffer.
    memcpy(buffer4, (unsigned char*)(&f4), 4);                      //  Force float into 4-byte unsigned char buffer.
    for(j = 0; j < 4; j++)                                          //  Append 4-byte buffer to buffer.
      buffer[i++] = buffer4[j];                                     //  NODE.VALUE

    ull8 = node->zhash;                                             //  Copy the node's zhash to the buffer.
    memcpy(buffer8, (unsigned char*)(&ull8), 8);                    //  Force unsigned long long into 8-byte unsigned char buffer.
    for(j = 0; j < 8; j++)                                          //  Append 8-byte buffer to buffer.
      buffer[i++] = buffer8[j];                                     //  NODE.ZHASH

    ui4 = node->hIndex;                                             //  Copy the node's hIndex to the buffer.
    memcpy(buffer4, (unsigned char*)(&ui4), 4);                     //  Force unsigned int into 4-byte unsigned char buffer.
    for(j = 0; j < 4; j++)                                          //  Append 4-byte buffer to buffer.
      buffer[i++] = buffer4[j];                                     //  NODE.HINDEX

    buffer[i++] = node->phase;                                      //  NODE.PHASE
    buffer[i++] = node->flags;                                      //  NODE.FLAGS

    for(i = 0; i < _NEGAMAX_NODE_BYTE_SIZE; i++)                    //  Now write "buffer" to an offset location in "negamaxSearchBuffer".
      negamaxSearchBuffer[index * _NEGAMAX_NODE_BYTE_SIZE + 4 + i] = buffer[i];

    return;
  }

/* In "negamaxMovesBuffer", at an offset of "index" * sizeof(NegamaxMove), read the bytes there into the given struct, "moveData". */
void restoreMove(unsigned int index, NegamaxMove* moveData)
  {
    unsigned char buffer[_NEGAMAX_MOVE_BYTE_SIZE];
    unsigned int i, j;

    for(i = 0; i < _NEGAMAX_MOVE_BYTE_SIZE; i++)                    //  Copy a slice of "negamaxMovesBuffer" to the local "buffer".
      buffer[i] = negamaxMovesBuffer[4 + index * _NEGAMAX_MOVE_BYTE_SIZE + i];

    i = 0;
    for(j = 0; j < _MOVE_BYTE_SIZE; j++)
      moveData->moveByteArray[j] = buffer[i++];                     //  MOVE.MOVEBYTEARRAY

    moveData->quietMove = buffer[i++];                              //  MOVE.QUIETMOVE

    return;
  }

/* Write bytes for the given Negamax move (byte array) to "negamaxMovesBuffer" at an offset of "index" * _MOVE_BYTE_SIZE. */
void saveMove(NegamaxMove* moveData, unsigned int index)
  {
    unsigned char buffer[_NEGAMAX_MOVE_BYTE_SIZE];
    unsigned int i, j;

    i = 0;
    for(j = 0; j < _MOVE_BYTE_SIZE; j++)
      buffer[i++] = moveData->moveByteArray[j];                     //  MOVE.MOVEBYTEARRAY

    buffer[i++] = moveData->quietMove;                              //  MOVE.QUIETMOVE

    for(i = 0; i < _NEGAMAX_MOVE_BYTE_SIZE; i++)                    //  Now write "buffer" to an offset location in "negamaxMovesBuffer".
      negamaxMovesBuffer[4 + index * _NEGAMAX_MOVE_BYTE_SIZE + i] = buffer[i];

    return;
  }

/**************************************************************************************************
 Zobrist hashing  */

static inline unsigned long long zobristKey(unsigned int index)
  {
    unsigned long long key;                                         //  Copy 8 bytes from the serial buffer.
                                                                    //  Force the 8-byte buffer into an unsigned long long.
    memcpy(&key, &zobristHashBuffer[index * 8], sizeof(unsigned long long));
    return key;
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

/* Hash the given byte array "hashInputBuffer". */
unsigned long long hash(unsigned char* hashInputBuffer)
  {
    unsigned long long h = 0L;
    unsigned char i, j, k, ctr, mask;

    ctr = 0;                                                        //  Iterate over bytes in the encoded game-state array.

    j = 0;                                                          //  Iterate over board indices.

    for(i = 0; i < 13; i++)  /////////////////////////////////////////  Encode white stones.
      {
        mask = 128;                                                 //  Restore high bit.
        for(k = 0; k < 8; k++)
          {
            if(j < _NONE && (hashInputBuffer[ctr] & mask) == mask)  //  White stone found.
              h ^= zobristKey(W_A1 + j);
            j++;
            mask >>= 1;
          }
        ctr++;
      }

    j = 0;                                                          //  Iterate over board indices.

    for(i = 0; i < 13; i++)  /////////////////////////////////////////  Encode black stones.
      {
        mask = 128;                                                 //  Restore high bit.
        for(k = 0; k < 8; k++)
          {
            if(j < _NONE && (hashInputBuffer[ctr] & mask) == mask)
              h ^= zobristKey(B_A1 + j);
            j++;
            mask >>= 1;
          }
        ctr++;
      }

    if((hashInputBuffer[25] & 1) == 1)  //////////////////////////////  Encode side to move.
      h ^= zobristKey(W_TO_MOVE);

    if(hashInputBuffer[_ANCHOR_BYTE_OFFSET] < _NONE)  ////////////////  Encode the previous move/anchor index.
      h ^= zobristKey( PREV_A1 + hashInputBuffer[_ANCHOR_BYTE_OFFSET] );

    return h;
  }

/**************************************************************************************************
 Killer-move functions  */

/* Look up the given move, at the given ply.
   If this is the first (freshest) killer-move listed, then return 1.
   If this is the second (less fresh) killer-move listed, then return 2.
   If the given move, at the given ply is not found, return 0. */
unsigned char killerLookup(unsigned char ply, unsigned char anchor, unsigned char index)
  {
    unsigned int offset;
    unsigned char direction;

    if(anchor == _NONE || ply >= _KILLER_MOVE_MAX_DEPTH)
      return KILLER_NOT_FOUND;

    if(index == u(anchor))
      direction = KILLER_DIRECTION_U;
    else if(index == ur(anchor))
      direction = KILLER_DIRECTION_UR;
    else if(index == r(anchor))
      direction = KILLER_DIRECTION_R;
    else if(index == dr(anchor))
      direction = KILLER_DIRECTION_DR;
    else if(index == d(anchor))
      direction = KILLER_DIRECTION_D;
    else if(index == dl(anchor))
      direction = KILLER_DIRECTION_DL;
    else if(index == l(anchor))
      direction = KILLER_DIRECTION_L;
    else if(index == ul(anchor))
      direction = KILLER_DIRECTION_UL;
    else
      direction = KILLER_DIRECTION_INVALID;

    if(direction == KILLER_DIRECTION_INVALID)
      return KILLER_NOT_FOUND;

    offset = ply * _KILLER_MOVE_PER_PLY;
    if(killerMovesTableBuffer[offset] == direction)
      return KILLER_FOUND_FIRST;

    offset++;
    if(killerMovesTableBuffer[offset] == direction)
      return KILLER_FOUND_SECOND;

    return KILLER_NOT_FOUND;
  }

/* Add the given killer move at the given ply.
   Demote whatever was there before. */
void killerAdd(unsigned char ply, unsigned char anchor, unsigned char index)
  {
    unsigned int offset = ply * _KILLER_MOVE_PER_PLY;
    unsigned char direction;
    unsigned char tmpDirection;

    if(anchor == _NONE || ply >= _KILLER_MOVE_MAX_DEPTH)
      return;

    if(index == u(anchor))
      direction = KILLER_DIRECTION_U;
    else if(index == ur(anchor))
      direction = KILLER_DIRECTION_UR;
    else if(index == r(anchor))
      direction = KILLER_DIRECTION_R;
    else if(index == dr(anchor))
      direction = KILLER_DIRECTION_DR;
    else if(index == d(anchor))
      direction = KILLER_DIRECTION_D;
    else if(index == dl(anchor))
      direction = KILLER_DIRECTION_DL;
    else if(index == l(anchor))
      direction = KILLER_DIRECTION_L;
    else if(index == ul(anchor))
      direction = KILLER_DIRECTION_UL;
    else
      direction = KILLER_DIRECTION_INVALID;

    if(direction == KILLER_DIRECTION_INVALID)
      return;
                                                                    //  If the given move is already equal to the first preferred killer,
                                                                    //  then do nothing.
    if(killerMovesTableBuffer[offset] == direction)
      return;
                                                                    //  If the given move is already equal to the second preferred killer,
                                                                    //  then promote number 2 to number 1 and demote number 1 to number 2.
    if(killerMovesTableBuffer[offset + 1] == direction)
      {
        tmpDirection = killerMovesTableBuffer[offset + 1];          //  Killer #2 --> tmp
                                                                    //  Killer #1 --> Killer #2
        killerMovesTableBuffer[offset + 1] = killerMovesTableBuffer[offset];
        killerMovesTableBuffer[offset] = tmpDirection;              //  tmp --> Killer #1
        return;
      }
                                                                    //  If the given move is new,
                                                                    //  then drop number 2, demote number 1 to number 2, and the given move the new number 1.
    killerMovesTableBuffer[offset + 1] = killerMovesTableBuffer[offset];
    killerMovesTableBuffer[offset] = direction;

    return;
  }

/*  Had to copy this from gamestate.h because we do not want to include gamestate.h. */
unsigned char u(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i + 10) == row(i) + 1)
          return i + 10;
      }
    return _NONE;
  }

/*  Had to copy this from gamestate.h because we do not want to include gamestate.h. */
unsigned char d(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i - 10) == row(i) - 1 && row(i) != _NONE)
          return i - 10;
      }
    return _NONE;
  }

/*  Had to copy this from gamestate.h because we do not want to include gamestate.h. */
unsigned char l(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i - 1) == row(i))
          return i - 1;
      }
    return _NONE;
  }

/*  Had to copy this from gamestate.h because we do not want to include gamestate.h. */
unsigned char r(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i + 1) == row(i))
          return i + 1;
      }
    return _NONE;
  }

/*  Had to copy this from gamestate.h because we do not want to include gamestate.h. */
unsigned char ul(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i + 9) == row(i) + 1)
          return i + 9;
      }
    return _NONE;
  }

/*  Had to copy this from gamestate.h because we do not want to include gamestate.h. */
unsigned char ur(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i + 11) == row(i) + 1)
          return i + 11;
      }
    return _NONE;
  }

/*  Had to copy this from gamestate.h because we do not want to include gamestate.h. */
unsigned char dl(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i - 11) == row(i) - 1 && row(i) != _NONE)
          return i - 11;
      }
    return _NONE;
  }

/*  Had to copy this from gamestate.h because we do not want to include gamestate.h. */
unsigned char dr(unsigned char i)
  {
    if(i < _NONE)
      {
        if(row(i - 9) == row(i) - 1 && row(i) != _NONE)
          return i - 9;
      }
    return _NONE;
  }

/*  Had to copy this from gamestate.h because we do not want to include gamestate.h. */
unsigned char col(unsigned char i)
  {
    if(i < _NONE)
      return i % 10;
    return _NONE;
  }

/*  Had to copy this from gamestate.h because we do not want to include gamestate.h. */
unsigned char row(unsigned char i)
  {
    if(i < _NONE)
      return (i - (i % 10)) / 10;
    return _NONE;
  }

/**************************************************************************************************
 History-heuristic functions  */

/* Look up the history-heuristic score for the given anchor point and move index. */
unsigned int historyLookup(unsigned char anchor, unsigned char index)
  {
    unsigned int offset = anchor * _NONE + index;
    return (unsigned int)historyTableBuffer[offset];
  }

/* Increment the history-heuristic score for the given anchor point and move index. */
void historyUpdate(unsigned char anchor, unsigned char index, unsigned char depth)
  {
    unsigned int inc = depth * depth;
    unsigned int value;
    unsigned int offset = anchor * _NONE + index;

    value = historyTableBuffer[offset] + inc;

    historyTableBuffer[offset] = (value > 255) ? 255 : (unsigned char)value;

    return;
  }

/* Try to prevent the history table from becoming saturated.
   Call this from JavaScript every... 32 completed root searches.
   This will preserve the rankings but (hopefully) keep values from getting blown out. */
void ageHistoryTable(void)
  {
    unsigned int i;
    for(i = 0; i < sizeof(historyTableBuffer); i++)
        historyTableBuffer[i] >>= 1;
    return;
  }

/**************************************************************************************************
 Node Ctr++  */

void incrementNodeCtr(void)
  {
    unsigned int ctr;
    unsigned char buffer4[4];
    unsigned char i;

    for(i = 0; i < 4; i++)
      buffer4[i] = inputParametersBuffer[PARAM_BUFFER_NODESSEARCHED_OFFSET + i];

    memcpy(&ctr, buffer4, 4);                                       //  Force the 4-byte buffer into an unsigned int.

    ctr++;

    memcpy(buffer4, (unsigned char*)(&ctr), 4);                     //  Force the unsigned int into a 4-byte temp buffer.
    for(i = 0; i < 4; i++)                                          //  Copy bytes to parameters buffer.
      inputParametersBuffer[PARAM_BUFFER_NODESSEARCHED_OFFSET + i] = buffer4[i];

    return;
  }

/**************************************************************************************************
 Transposition-table functions.  */

/* Increase the generation stamp in the transposition table.
   Call this function from JavaScript when a new set of possible opponent moves is generated.
   When the transpo-table counter rolls over, we dump the entire table.
   The bool returned here simply indicates to JavaScript when that happens (we might like to know). */
void incTranspoTableGeneration(void)
  {
    incGeneration();
    return;
  }

unsigned int generationAge(unsigned char current, unsigned char stored)
  {
    if(stored == 0)
      return 255;                                                   //  0 signified an unused record.

    if(current >= stored)
      return current - stored;

    return (255u - stored) + current;
  }

unsigned int statsGet(unsigned int offset)
  {
    unsigned int value;
    memcpy(&value, statsBuffer + offset, 4);
    return value;
  }

void statsSet(unsigned int offset, unsigned int value)
  {
    memcpy(statsBuffer + offset, &value, 4);
    return;
  }

void statsIncrement(unsigned int offset)
  {
    unsigned int value = statsGet(offset);
    statsSet(offset, value + 1);
    return;
  }

void resetTTStats(void)
  {
    memset(statsBuffer, 0, _STATS_BUFFER_SIZE);
    return;
  }

unsigned int getTTProbes(void)
  {
    return statsGet(STATS_TT_PROBES_OFFSET);
  }

unsigned int getTTHits(void)
  {
    return statsGet(STATS_TT_HITS_OFFSET);
  }

unsigned int getTTDepthQualified(void)
  {
    return statsGet(STATS_TT_DEPTH_QUALIFIED_OFFSET);
  }

unsigned int getTTCutoffs(void)
  {
    return statsGet(STATS_TT_CUTOFFS_OFFSET);
  }

/**************************************************************************************************
 Repetition History Functions.  */

/*
unsigned char repetitionPathPrefixLength(void)
  {
    return repetitionPathBuffer[0];
  } */

/*
unsigned int repetitionPathNodeOffset(unsigned int gsIndex)
  {
    return _REPETITION_PATH_HEADER_SIZE + (repetitionPathBuffer[0] + gsIndex) * _REPETITION_STATE_BYTE_SIZE;
  } */

/*
void saveRepetitionState(unsigned int gsIndex)
  {
    memcpy(&repetitionPathBuffer[ repetitionPathNodeOffset(gsIndex) ], answerRepetitionStateBuffer, _REPETITION_STATE_BYTE_SIZE);
    return;
  } */

/*
unsigned int repetitionOccurrenceCount(unsigned int gsIndex)
  {
    unsigned int count = 0;
    unsigned int historyLen;
    unsigned int prefixLen;
    unsigned int livePathLen;
    unsigned int i;

    unsigned char* target;
    unsigned char* candidate;

    if(gsIndex >= _NEGAMAX_NODE_STACK_CAPACITY)                     //  This should never happen: signal an error.
      {
        inputParametersBuffer[PARAM_BUFFER_STATUS_OFFSET] = STATUS_ERROR;
        return 0;
      }
                                                                    //  The canonical state belonging to this node.
    target = &repetitionPathBuffer[ repetitionPathNodeOffset(gsIndex) ];

    //////////////////////////////////////////////////////////////////  Search the REAL game history.
    historyLen = repetitionHistoryLength();
    if(historyLen > _REPETITION_HISTORY_CAPACITY)                   //  This should never happen: signal an error.
      {
        inputParametersBuffer[PARAM_BUFFER_STATUS_OFFSET] = STATUS_ERROR;
        return 0;
      }

    for(i = 0; i < historyLen; i++)
      {
        candidate = &repetitionHistoryBuffer[4 + i * _REPETITION_STATE_BYTE_SIZE];
        if(memcmp(target, candidate, _REPETITION_STATE_BYTE_SIZE) == 0)
          count++;
      }

    //////////////////////////////////////////////////////////////////  Search the LIVE hypothetical path.
    prefixLen = repetitionPathPrefixLength();
    livePathLen = prefixLen + gsIndex + 1;                          //  Includes the current node itself.

    for(i = 0; i < livePathLen; i++)
      {
        candidate = &repetitionPathBuffer[_REPETITION_PATH_HEADER_SIZE + i * _REPETITION_STATE_BYTE_SIZE];
        if(memcmp(target, candidate, _REPETITION_STATE_BYTE_SIZE) == 0)
          count++;
      }

    return count;
  } */

/*
unsigned int repetitionHistoryLength(void)
  {
    unsigned int len;
    memcpy(&len, &repetitionHistoryBuffer[0], 4);
    return len;
  } */