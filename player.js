const STATUS_UNSEARCHED = 0;                                        //  Branch object has not been examined at all.
const STATUS_BOOK_LOOKUP = 1;                                       //  Call to back-end book-lookup is currently out.
const STATUS_SEARCHING = 2;                                         //  This branch has been pushed/initialized as root, and negamax is in some state.
const STATUS_DONE_SEARCH = 3;                                       //  No Negamax search is active for this branch; 'depth' is the last completed depth.

const NEGAMAX_STATUS_IDLE           = 0x00;                         //  (See C++ code) No search running. Awaiting instructions.
const NEGAMAX_STATUS_RUNNING        = 0x01;                         //  (See C++ code) Search running.
const NEGAMAX_STATUS_DONE           = 0x02;                         //  (See C++ code) Search complete.
const NEGAMAX_STATUS_STOP_REQUESTED = 0x03;                         //  (See C++ code) Will halt the present search at the next safe point.
const NEGAMAX_STATUS_STOP_TIME      = 0x04;                         //  (See C++ code) Will halt the present search at the next safe point, owing to time constraints.
const NEGAMAX_STATUS_ABORTED        = 0x05;                         //  (See C++ code) Search was hard-killed: be wary of partial results.
const NEGAMAX_STATUS_ERROR          = 0xFF;                         //  (See C++ code) An error has occurred.

const NEGAMAX_CTRL_STOP_REQUESTED   = 0x01;                         //  (See C++ code) Set this byte in commandFlags to request that the present search stop.
const NEGAMAX_CTRL_HARD_ABORT       = 0x02;                         //  (See C++ code) Set this byte in commandFlags to request that the present search abort.
const NEGAMAX_CTRL_STOP_TIME        = 0x04;                         //  (See C++ code) Request that the present search stop because its time budget has expired.

/* A.I. Player class for FREEDOM. */
class Player
  {
    /**********************************************************************************************
     Constructor                                                                                  */

    constructor()
      {
        this.team = 'Black';                                        //  In {'White', 'Black'}. Default to Black.
        this.currentPly = 1;                                        //  The depth to which the A.I. should search ON THE CURRENT ITERATION.
        this.maxPly = 1;                                            //  Maximum depth to which this A.I. should search.

        this.searchId = 0;                                          //  Monotonic persistent branch/search identifier.
        this.nodeCtr = 0;                                           //  Reset after every turn.

        this.branches = [];                                         //  Array of Objects, each {ByteArray(GameState), uchar(depth), ByteArray(Move)}.
        this.branchIterator = 0;                                    //                          The state in which    Depth to      The agent's reply.
                                                                    //                          the agent may act.    which agent   (Having searched.)
                                                                    //                                                has searched.
        //////////////////////////////////////////////////////////////  The evaluation engine.
        this.evaluationEngine = null;                               //  WebAssembly Module containing evaluation functions.

        this.evaluationInputGameStateOffset = null;                 //  (Offset into Module memory.)
        this.evaluationInputGameStateBuffer = null;                 //  ByteArray: Input buffer for an encoded query-gamestate.

        this.evaluationInputMoveOffset = null;                      //  (Offset into Module memory.)
        this.evaluationInputMoveBuffer = null;                      //  ByteArray: Input buffer for an encoded query-move.

        this.evaluationOutputGameStateOffset = null;                //  (Offset into Module memory.)
        this.evaluationOutputGameStateBuffer = null;                //  ByteArray: Output buffer for an encoded answer-gamestate.
                                                                    //  (You would obtain a single gamestate, for instance, after calling makeMove().)

        this.evaluationOutputMovesOffset = null;                    //  (Offset into Module memory.)
        this.evaluationOutputMovesBuffer = null;                    //  ByteArray: Output buffer for encoded answer-moves.
                                                                    //  (You would obtain several moves, for instance, after calling getMoves().)

        //this.evaluationOutputRepetitionEncOffset = null;            //  (Offset into Module memory.)
        //this.evaluationOutputRepetitionEncBuffer = null;            //  ByteArray: Output buffer for repetition-detection encodings.

        //////////////////////////////////////////////////////////////  The negamax engine.
        this.negamaxEngine = null;                                  //  WebAssembly Module containing the negamax search engine.

        this.negamaxInputOffset = null;                             //  (Offset into Module memory.)
        this.negamaxInputBuffer = null;                             //  ByteArray: Input buffer for tree search. Encode a query-gamestate here.
                                                                    //             When Player asks negamax something.
        this.negamaxParamsOffset = null;                            //  (Offset into Module memory.)
        this.negamaxParamsBuffer = null;                            //  ByteArray: Input buffer for search parameters.

        this.negamaxOutputOffset = null;                            //  (Offset into Module memory.)
        this.negamaxOutputBuffer = null;                            //  ByteArray: Output buffer for tree search. Decode an answer from here.
                                                                    //             When negamax answers Player.
        this.negamaxQueryGameStateOffset = null;                    //  (Offset into Module memory.)
        this.negamaxQueryGameStateBuffer = null;                    //  ByteArray: Used by the negamax module to pass encoded game states to the evaluation module.

        this.negamaxQueryMoveOffset = null;                         //  (Offset into Module memory.)
        this.negamaxQueryMoveBuffer = null;                         //  ByteArray: Used by the negamax module to pass encoded moves to the evaluation module.

        this.negamaxAnswerGameStateOffset = null;                   //  (Offset into Module memory.)
        this.negamaxAnswerGameStateBuffer = null;                   //  ByteArray: Used by the negamax module to receive encoded game states from the evaluation module.

        this.negamaxAnswerMovesOffset = null;                       //  (Offset into Module memory.)
        this.negamaxAnswerMovesBuffer = null;                       //  ByteArray: Used by the negamax module to receive encoded moves from the evaluation module.

        this.ZobristHashOffset = null;                              //  (Offset into Module memory.)
        this.ZobristHashBuffer = null;                              //  ByteArray representation of the Zobrist hasher.

        this.TranspositionTableOffset = null;                       //  (Offset into Module memory.)
        this.TranspositionTableBuffer = null;                       //  ByteArray representation of the transposition table.

        this.negamaxSearchOffset = null;                            //  (Offset into Module memory.)
        this.negamaxSearchBuffer = null;                            //  ByteArray: Working node buffer for tree-search.

        this.negamaxMovesOffset = null;                             //  (Offset into Module memory.)
        this.negamaxMovesBuffer = null;                             //  ByteArray: Working move buffer for tree-search.

        this.negamaxKillerMovesOffset = null;                       //  (Offset into Module memory.)
        this.negamaxKillerMovesBuffer = null;                       //  ByteArray representation of the killer-moves table.

        this.negamaxHistoryHeuristicOffset = null;                  //  (Offset into Module memory.)
        this.negamaxHistoryHeuristicBuffer = null;                  //  ByteArray representation of the history-heuristic table.

        this.negamaxStatisticsOffset = null;                        //  (Offset into Module memory.)
        this.negamaxStatisticsBuffer = null;                        //  ByteArray representation of statistics collected during search.

        //////////////////////////////////////////////////////////////  Repetition tracker.
        //this.repetitionHistoryOffset = null;                        //  (Offset into Module memory.)
        //this.repetitionHistoryBuffer = null;                        //  ByteArray representation of the repetition-tracker table.

        //this.repetitionPathOffset = null;                           //  (Offset into Module memory.)
        //this.repetitionPathBuffer = null;                           //  ByteArray representation of the repetition-path buffer.

        //this.answerRepetitionStateOffset = null;                    //  (Offset into Module memory.)
        //this.answerRepetitionStateBuffer = null;                    //  ByteArray: receiving buffer for special game-state encoding.

        //this.repetitionHistoryView = null;

        //////////////////////////////////////////////////////////////  Fetch, instantiate, and connect the Evaluation Module.
        fetch('obj/wasm/eval.wasm', {headers: {'Content-Type': 'application/wasm'} })
        .then(response => response.arrayBuffer())
        .then(bytes =>
          {
            WebAssembly.instantiate(bytes,
              {
                env: {
                       memoryBase: 0,
                       tableBase: 0,
                                                                    //  Compiled with -s INITIAL_HEAP=1048576 = 16 pages.
                       memory: new WebAssembly.Memory({initial: 16, maximum: 16}),
                       table: new WebAssembly.Table({initial: 0, element: 'anyfunc'})
                     }
              })
            .then(instance =>
              {
                this.evaluationEngine = instance;
                                                                    //  Assign offset to input-gamestate buffer.
                this.evaluationInputGameStateOffset = this.evaluationEngine.instance.exports.getInputGameStateBuffer();
                this.evaluationInputGameStateBuffer = new Uint8Array(this.evaluationEngine.instance.exports.memory.buffer, this.evaluationInputGameStateOffset, _GAMESTATE_BYTE_SIZE);
                                                                    //  Assign offset to input-move buffer.
                this.evaluationInputMoveOffset = this.evaluationEngine.instance.exports.getInputMoveBuffer();
                this.evaluationInputMoveBuffer = new Uint8Array(this.evaluationEngine.instance.exports.memory.buffer, this.evaluationInputMoveOffset, _MOVE_BYTE_SIZE);
                                                                    //  Assign offset to output-gamestate buffer.
                this.evaluationOutputGameStateOffset = this.evaluationEngine.instance.exports.getOutputGameStateBuffer();
                this.evaluationOutputGameStateBuffer = new Uint8Array(this.evaluationEngine.instance.exports.memory.buffer, this.evaluationOutputGameStateOffset, _GAMESTATE_BYTE_SIZE);
                                                                    //  Assign offset to output-moves buffer.
                this.evaluationOutputMovesOffset = this.evaluationEngine.instance.exports.getOutputMovesBuffer();
                this.evaluationOutputMovesBuffer = new Uint8Array(this.evaluationEngine.instance.exports.memory.buffer, this.evaluationOutputMovesOffset, _MAX_MOVES * (_MOVE_BYTE_SIZE + 5));
                                                                    //  Assign offset to repetition-detection's output buffer.
                //this.evaluationOutputRepetitionEncOffset = this.evaluationEngine.instance.exports.getOutputRepetitionStateBuffer();
                //this.evaluationOutputRepetitionEncBuffer = new Uint8Array(this.evaluationEngine.instance.exports.memory.buffer, this.evaluationOutputRepetitionEncOffset, _REPETITION_STATE_BYTE_SIZE);

                elementsLoaded++;                                   //  Check evaluationEngine off our list.

                console.log('Evaluation WASM memory: ', (this.evaluationEngine.instance.exports.memory.buffer.byteLength / (1024 * 1024)).toFixed(2), 'MiB');

                                                                    //  Load the tree-search module AFTER the evaluation module is complete.
                                                                    //  Fetch, instantiate, and connect the Negamax Module.
                fetch('obj/wasm/negamax.wasm', {headers: {'Content-Type': 'application/wasm'} })
                .then(response => response.arrayBuffer())
                .then(bytes =>
                  {
                    WebAssembly.instantiate(bytes,
                      {
                        env: {
                               memoryBase: 0,
                               tableBase: 0,
                                                                    //  Compiled with -s INITIAL_MEMORY=19005440 = 290 pages.
                               memory: new WebAssembly.Memory({initial: 290}),
                               table: new WebAssembly.Table({initial: 1, element: 'anyfunc'}),
                               _copyQuery2EvalGSInput: function()
                                 {
                                   var i;                           //  Copy negamaxEngine's query buffer contents
                                                                    //  to evaluationEngine's input-gamestate buffer.
                                   for(i = 0; i < _GAMESTATE_BYTE_SIZE; i++)
                                     this.evaluationInputGameStateBuffer[i] = this.negamaxQueryGameStateBuffer[i];
                                   return;
                                 }.bind(this),
                               _copyQuery2EvalMoveInput: function()
                                 {
                                   var i;                           //  Copy negamaxEngine's query buffer contents
                                                                    //  to evaluationEngine's input-move buffer.
                                   for(i = 0; i < _MOVE_BYTE_SIZE; i++)
                                     this.evaluationInputMoveBuffer[i] = this.negamaxQueryMoveBuffer[i];
                                   return;
                                 }.bind(this),
                               _copyEvalOutput2AnswerGSBuffer: function()
                                 {
                                   var i;                           //  Copy evaluationEngine's output buffer contents
                                                                    //  to negamaxEngine's answer-gamestate buffer.
                                   for(i = 0; i < _GAMESTATE_BYTE_SIZE; i++)
                                     this.negamaxAnswerGameStateBuffer[i] = this.evaluationOutputGameStateBuffer[i];
                                   return;
                                 }.bind(this),
                               _copyEvalOutput2AnswerMovesBuffer: function(len)
                                 {
                                   var i;
                                   const size = _MOVE_BYTE_SIZE + 5;//  Copy evaluationEngine's output buffer contents
                                   for(i = 0; i < len * size; i++)  //  to negamaxEngine's answer-moves buffer.
                                     this.negamaxAnswerMovesBuffer[i] = this.evaluationOutputMovesBuffer[i];
                                   return;
                                 }.bind(this),
                               //_copyEvalRepetitionOutput2AnswerRepetitionBuffer: function()
                               //  {
                               //                                     //  Copy the FIDE-compliant repetition-detection encoding
                               //                                     //  from evaluationEngine's encoding output buffer
                               //    var i;                           //  to   negamaxEngine's    repetition-detection encoding input buffer.
                               //    for(i = 0; i < _REPETITION_STATE_BYTE_SIZE; i++)
                               //      this.answerRepetitionStateBuffer[i] = this.evaluationOutputRepetitionEncBuffer[i];
                               //    return;
                               //  }.bind(this),
                               //_sideToMove: function()              //  Returns unsigned char.
                               //  {
                               //    return this.evaluationEngine.instance.exports.sideToMove_eval();
                               //  }.bind(this),
                               _isTerminal: function()              //  Returns bool.
                                 {
                                   return this.evaluationEngine.instance.exports.isTerminal_eval();
                                 }.bind(this),
                               _makeMove: function()
                                 {
                                   this.evaluationEngine.instance.exports.makeMove_eval();
                                   return;
                                 }.bind(this),
                               _makeNullMove: function()
                                 {
                                   this.evaluationEngine.instance.exports.makeNullMove_eval();
                                   return;
                                 }.bind(this),
                               _evaluate: function()                //  Returns float.
                                 {
                                   return this.evaluationEngine.instance.exports.evaluate_eval();
                                 }.bind(this),
                               _getMoves: function()                //  Returns unsigned int.
                                 {
                                   return this.evaluationEngine.instance.exports.getMoves_eval();
                                 }.bind(this)
                               //_repetitionState: function()
                               //  {
                               //    this.evaluationEngine.instance.exports.repetitionState_eval();
                               //  }.bind(this),
                               //_historyVerdict: function(ctr)       //  Returns unsigned char.
                               //  {
                               //    return this.evaluationEngine.instance.exports.historyVerdict_eval(ctr);
                               //  }.bind(this)
                             }
                      })
                    .then(instance =>
                      {
                        var i;

                        this.negamaxEngine = instance;
                                                                    //  Assign offset to input buffer. This receives a game state as a byte array.
                        this.negamaxInputOffset = this.negamaxEngine.instance.exports.getInputBuffer();
                        this.negamaxInputBuffer = new Uint8Array(this.negamaxEngine.instance.exports.memory.buffer, this.negamaxInputOffset, _GAMESTATE_BYTE_SIZE);
                                                                    //  Assign offset to parameters buffer.
                        this.negamaxParamsOffset = this.negamaxEngine.instance.exports.getParametersBuffer();
                        this.negamaxParamsBuffer = new Uint8Array(this.negamaxEngine.instance.exports.memory.buffer, this.negamaxParamsOffset, _PARAMETER_ARRAY_SIZE);
                                                                    //  Assign offset to output buffer.
                                                                    //  This receives a game state, a 1-byte uchar (depth achieved), a move, a 4-byte float (score).
                        this.negamaxOutputOffset = this.negamaxEngine.instance.exports.getOutputBuffer();
                        this.negamaxOutputBuffer = new Uint8Array(this.negamaxEngine.instance.exports.memory.buffer, this.negamaxOutputOffset, _GAMESTATE_BYTE_SIZE + 1 + _MOVE_BYTE_SIZE + 4);
                                                                    //  Assign offset to input buffer. This receives a game state as a byte array.
                        this.negamaxQueryGameStateOffset = this.negamaxEngine.instance.exports.getQueryGameStateBuffer();
                        this.negamaxQueryGameStateBuffer = new Uint8Array(this.negamaxEngine.instance.exports.memory.buffer, this.negamaxQueryGameStateOffset, _GAMESTATE_BYTE_SIZE);
                                                                    //  Assign offset to input buffer. This receives a game state as a byte array.
                        this.negamaxQueryMoveOffset = this.negamaxEngine.instance.exports.getQueryMoveBuffer();
                        this.negamaxQueryMoveBuffer = new Uint8Array(this.negamaxEngine.instance.exports.memory.buffer, this.negamaxQueryMoveOffset, _MOVE_BYTE_SIZE);
                                                                    //  Assign offset to auxiliary buffer.
                                                                    //  This is a receiving buffer, temporarily holding output from the evaluation module before converting these data to negamax nodes.
                        this.negamaxAnswerGameStateOffset = this.negamaxEngine.instance.exports.getAnswerGameStateBuffer();
                        this.negamaxAnswerGameStateBuffer = new Uint8Array(this.negamaxEngine.instance.exports.memory.buffer, this.negamaxAnswerGameStateOffset, _GAMESTATE_BYTE_SIZE);
                                                                    //  Assign offset to auxiliary buffer.
                                                                    //  This is a receiving buffer, temporarily holding output from the evaluation module before converting these data to negamax nodes.
                        this.negamaxAnswerMovesOffset = this.negamaxEngine.instance.exports.getAnswerMovesBuffer();
                        this.negamaxAnswerMovesBuffer = new Uint8Array(this.negamaxEngine.instance.exports.memory.buffer, this.negamaxAnswerMovesOffset, _MAX_MOVES * (_MOVE_BYTE_SIZE + 5));
                                                                    //  Assign offset to Zobrist hash buffer.
                                                                    //  This receives 8 * _ZHASH_TABLE_SIZE bytes. Sections of 8 bytes treated as unsigned long longs.
                        this.ZobristHashOffset = this.negamaxEngine.instance.exports.getZobristHashBuffer();
                        this.ZobristHashBuffer = new Uint8Array(this.negamaxEngine.instance.exports.memory.buffer, this.ZobristHashOffset, _HASH_VALUE_BYTE_SIZE * _ZHASH_TABLE_SIZE);
                                                                    //  Assign offset to Transposition Table buffer.
                        this.TranspositionTableOffset = this.negamaxEngine.instance.exports.getTranspositionTableBuffer();
                        this.TranspositionTableBuffer = new Uint8Array(this.negamaxEngine.instance.exports.memory.buffer, this.TranspositionTableOffset, 1 + _TRANSPO_TABLE_SIZE * _TRANSPO_RECORD_BYTE_SIZE);
                                                                    //  Assign offset to tree-search buffer.
                                                                    //  This is a working buffer that receives bytes from the evaluation engine.
                        this.negamaxSearchOffset = this.negamaxEngine.instance.exports.getNegamaxSearchBuffer();
                        this.negamaxSearchBuffer = new Uint8Array(this.negamaxEngine.instance.exports.memory.buffer, this.negamaxSearchOffset, 4 + _NEGAMAX_NODE_STACK_CAPACITY * _NEGAMAX_NODE_BYTE_SIZE);
                                                                    //  Assign offset to tree-search buffer.
                                                                    //  This is a working buffer that receives bytes from the evaluation engine.
                        this.negamaxMovesOffset = this.negamaxEngine.instance.exports.getNegamaxMovesBuffer();
                        this.negamaxMovesBuffer = new Uint8Array(this.negamaxEngine.instance.exports.memory.buffer, this.negamaxMovesOffset, 4 + _NEGAMAX_MOVE_ARENA_CAPACITY * _NEGAMAX_MOVE_BYTE_SIZE);
                                                                    //  Assign offset to auxiliary buffer.
                                                                    //  This is a working buffer that holds the killer-moves table.
                        this.negamaxKillerMovesOffset = this.negamaxEngine.instance.exports.getKillerMovesBuffer();
                        this.negamaxKillerMovesBuffer = new Uint8Array(this.negamaxEngine.instance.exports.memory.buffer, this.negamaxKillerMovesOffset, _KILLER_MOVE_PER_PLY * 2 * _KILLER_MOVE_MAX_DEPTH);
                                                                    //  Assign offset to auxiliary buffer.
                                                                    //  This is a working buffer that holds the history-heuristic table.
                        this.negamaxHistoryHeuristicOffset = this.negamaxEngine.instance.exports.getHistoryTableBuffer();
                        this.negamaxHistoryHeuristicBuffer = new Uint8Array(this.negamaxEngine.instance.exports.memory.buffer, this.negamaxHistoryHeuristicOffset, 2 * _NOTHING * _NOTHING);
                                                                    //  Assign offset to auxiliary buffer.
                                                                    //  This is a working buffer that holds negamax statistics.
                        this.negamaxStatisticsOffset = this.negamaxEngine.instance.exports.getStatisticsBuffer();
                        this.negamaxStatisticsBuffer = new Uint8Array(this.negamaxEngine.instance.exports.memory.buffer, this.negamaxStatisticsOffset, _STATS_BUFFER_SIZE);
                                                                    //  Assign offset to auxiliary buffer.
                                                                    //  This is a working buffer that holds specially encoded states for repetition testing.
                        //this.repetitionHistoryOffset = this.negamaxEngine.instance.exports.getRepetitionHistoryBuffer();
                        //this.repetitionHistoryBuffer = new Uint8Array(this.negamaxEngine.instance.exports.memory.buffer, this.repetitionHistoryOffset, 4 + _REPETITION_HISTORY_CAPACITY * _REPETITION_STATE_BYTE_SIZE);
                                                                    //  4-byte window into "repetitionHistoryBuffer", starting at 0.
                        //this.repetitionHistoryView = new DataView(this.repetitionHistoryBuffer.buffer, this.repetitionHistoryBuffer.byteOffset, 4);
                                                                    //  Assign offset to auxiliary buffer.
                                                                    //  This is a working buffer that holds paths for repetition testing.
                        //this.repetitionPathOffset = this.negamaxEngine.instance.exports.getRepetitionPathBuffer();
                        //this.repetitionPathBuffer = new Uint8Array(this.negamaxEngine.instance.exports.memory.buffer, this.repetitionPathOffset, _REPETITION_PATH_HEADER_SIZE + (_REPETITION_PATH_PREFIX_CAPACITY + _NEGAMAX_NODE_STACK_CAPACITY) * _REPETITION_STATE_BYTE_SIZE);
                                                                    //  Assign offset to auxiliary buffer.
                                                                    //  This buffer receives game state encodings for repetition testing.
                        //this.answerRepetitionStateOffset = this.negamaxEngine.instance.exports.getAnswerRepetitionStateBuffer();
                        //this.answerRepetitionStateBuffer = new Uint8Array(this.negamaxEngine.instance.exports.memory.buffer, this.answerRepetitionStateOffset, _REPETITION_STATE_BYTE_SIZE);

                        this.TranspositionTableBuffer[0] = 1;       //  Set "generation byte" to 1.

                        elementsLoaded++;                           //  Check negaMaxEngine off our list.

                        console.log('Negamax WASM memory: ', (this.negamaxEngine.instance.exports.memory.buffer.byteLength / (1024 * 1024)).toFixed(2), 'MiB');

                                                                    //  Load a Zobrist Hasher.
                        var ReqXML = new XMLHttpRequest();          //  IE 7+, Firefox, Chrome, Opera, Safari.
                        var params = 'sendRequest=eggsnhash';
                        ReqXML.parent = this;                       //  Add a reference to the Player object inside this callback function.

                        ReqXML.open("POST", 'obj/sess/reqzhash.php', true);
                        ReqXML.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
                        ReqXML.onreadystatechange = function()
                          {
                            if(ReqXML.readyState == 4 && ReqXML.status == 200)
                              {
                                if(ReqXML.responseText == "")       //  Null return: unknown error.
                                  {
                                    switch(currentLang)
                                      {
                                        case "Spanish": alert(alertStringScrub("Error"));  break;
                                        case "German": alert(alertStringScrub("Fehler"));  break;
                                        case "Polish": alert(alertStringScrub("B&#322;&#261;d na stronie"));  break;
                                        default: alert(alertStringScrub("Error"));
                                      }
                                  }
                                else
                                  {
                                    var parse = ReqXML.responseText.split('|');
                                    var i;
                                    if(parse[0] == 'freedom' && parse[1] == 'ok')
                                      {
                                        parse = parse[2].split(',');//  Repurpose "parse".
                                        const keyCount = parseInt(parse[0], 10);
                                        console.log(keyCount + ' Zobrist keys.');
                                        if(keyCount != _ZHASH_TABLE_SIZE || parse.length != 1 + _HASH_VALUE_BYTE_SIZE * keyCount)
                                          {
                                            console.error('Zinn: malformed Zobrist hasher.');
                                            return;
                                          }
                                                                    //  Load into Zobrist-hasher's buffer.
                                        for(i = 1; i < parse.length; i++)
                                          this.parent.ZobristHashBuffer[i - 1] = parseInt(parse[i], 10)

                                        elementsLoaded++;           //  Check Zobrist hasher off our list.
                                        loadTotalReached();         //  Check the total.
                                      }
                                    else                            //  Error-label or garbage.
                                      {
                                        switch(currentLang)
                                          {
                                            case "Spanish": alert(alertStringScrub("Error"));  break;
                                            case "German": alert(alertStringScrub("Fehler"));  break;
                                            case "Polish": alert(alertStringScrub("B&#322;&#261;d na stronie"));  break;
                                            default: alert(alertStringScrub("Error"));
                                          }
                                      }
                                  }
                              }
                          };
                        ReqXML.send(params);
                      });
                  });
              });
          });
      }

    /**********************************************************************************************
     Step orchestration                                                                           */

    /* Called on every frame, except while animating, as long as the game is not over. */
    step()
      {
        if(this.evaluationEngine == null || this.negamaxEngine == null)
          return;

        if(CurrentTurn == this.team)
          this.stepMyTurn();
        else
          this.stepPondering();

        return;
      }

    /* Advance Zinn while it is Zinn's turn. */
    stepMyTurn()
      {
        var branch;
        var status;
                                                                    //  Pondering branches describe possible positions after the opponent moves.
                                                                    //  As soon as the opponent has actually moved, retain only the branch matching
                                                                    //  the real position.
                                                                    //  Any active search belongs to the old pondering schedule and is stopped first.
        if(!this.hasSingleCurrentBranch())
          this.adoptCurrentPosition();

        if(this.branches.length != 1)
          return;

        this.branchIterator = 0;
        branch = this.branches[0];

        switch(branch.status)
          {
            case STATUS_UNSEARCHED:
              this.lookup(0);
              break;

            case STATUS_BOOK_LOOKUP:
              break;

            case STATUS_SEARCHING:
              status = this.pulseSearch(0);
                                                                    //  A future clock strategist may ask Negamax to stop because time has expired.
                                                                    //  In that case, use the last COMPLETED iterative-deepening result.
              if(status == NEGAMAX_STATUS_STOP_TIME)
                {
                  if(this.hasUsableMove(branch))
                    this.playBestMove(branch);
                  else
                    console.error('Zinn: time stop occurred before any usable move was completed.');
                }
              break;

            case STATUS_DONE_SEARCH:
              if(branch.bookHit || branch.depth >= this.maxPly)
                this.playBestMove(branch);
              else
                {
                  this.currentPly = Math.min(this.maxPly, Math.max(1, branch.depth + 1));
                  this.initializeSearch(0, this.currentPly);
                }
              break;

            default:
              console.error('Zinn: unknown branch status on AI turn: ' + branch.status);
              break;
          }

        return;
      }

    /* Advance Zinn while the opponent is thinking. */
    stepPondering()
      {
        var branch;

        if(this.branches.length == 0)
          {
            this.branch();
            return;
          }
                                                                    //  Once every non-book branch has reached maxPly, pondering is complete.
                                                                    //  There is no reason to initialize a dummy Negamax search while we wait for the opponent.
        if(this.ponderingComplete())
          return;

        if(this.branchIterator >= this.branches.length)
          this.branchIterator = 0;

        branch = this.branches[this.branchIterator];

        switch(branch.status)
          {
            case STATUS_UNSEARCHED:
              this.lookup(this.branchIterator);
              break;

            case STATUS_BOOK_LOOKUP:
              break;

            case STATUS_SEARCHING:
              this.pulseSearch(this.branchIterator);
              break;

            case STATUS_DONE_SEARCH:
                                                                    //  A branch may lag behind the current iterative-deepening round
                                                                    //  (for example, because its book lookup returned late).
                                                                    //  Catch it up one depth at a time.
              if(!branch.bookHit && branch.depth < this.currentPly)
                this.initializeSearch(this.branchIterator, branch.depth + 1);
              else
                this.advancePonderingBranch();
              break;

            default:
              console.error('Zinn: unknown branch status while pondering: ' + branch.status);
              this.advancePonderingBranch();
              break;
          }

        return;
      }

    /* True IFF the sole retained branch is the position currently displayed by the game. */
    hasSingleCurrentBranch()
      {
        return (this.branches.length == 1 && this.byteArrCmp(gameStateBuffer, this.branches[0].gamestate));
      }

    /* Stop any obsolete search, then retain/create exactly one branch for the position that actually resulted from the opponent's move. */
    adoptCurrentPosition()
      {
        var index;
        var branch;

        this.stopObsoleteSearch();

        index = this.findBranchByGameState(gameStateBuffer);
        if(index >= 0)
          branch = this.branches[index];
        else
          branch = this.makeBranch(gameStateBuffer);
                                                                    //  If this branch happened to be the one whose deeper iteration was interrupted,
                                                                    //  its durable result is still whatever is recorded in branch.depth/bestMove.
        if(branch.status == STATUS_SEARCHING)
          branch.status = STATUS_DONE_SEARCH;

        this.branches = [branch];
        this.branchIterator = 0;
        this.currentPly = Math.min(this.maxPly, Math.max(1, branch.depth + 1));

        return;
      }

    /* Stop a search that belonged to the old pondering schedule.
       Completed work is harvested; incomplete work is discarded from the DFS stack but its TT entries remain available. */
    stopObsoleteSearch()
      {
        var status;
        var activeIndex;

        status = this.negamaxEngine.instance.exports.getStatus();
        activeIndex = this.findActiveBranchIndex();

        if(status == NEGAMAX_STATUS_DONE)
          {
            if(activeIndex >= 0)
              this.harvestCompletedSearch(activeIndex);
            else
              {
                console.warn('Zinn: Negamax is DONE but its search ID matches no retained branch.');
                this.collectNodes(-1);
              }
            return;
          }

        if(status == NEGAMAX_STATUS_RUNNING)
          {
            this.negamaxEngine.instance.exports.setControlFlag(NEGAMAX_CTRL_STOP_REQUESTED);
            this.negamaxEngine.instance.exports.negamax();
            status = this.negamaxEngine.instance.exports.getStatus();
          }

        if(status == NEGAMAX_STATUS_STOP_REQUESTED ||
           status == NEGAMAX_STATUS_STOP_TIME      ||
           status == NEGAMAX_STATUS_ABORTED        ||
           status == NEGAMAX_STATUS_ERROR)
          {
            this.collectNodes(activeIndex);
            if(activeIndex >= 0 && this.branches[activeIndex].status == STATUS_SEARCHING)
              this.branches[activeIndex].status = STATUS_DONE_SEARCH;
          }
        else if(status != NEGAMAX_STATUS_IDLE && status != NEGAMAX_STATUS_DONE)
          console.error('Zinn: unexpected Negamax status while stopping obsolete search: ' + status);

        return;
      }

    /* Pulse exactly one active Negamax search once.
       Terminal statuses are never pulsed again. */
    pulseSearch(branchIndex)
      {
        var status;
        var activeIndex;

        status = this.negamaxEngine.instance.exports.getStatus();

        if(status == NEGAMAX_STATUS_RUNNING)
          {
            this.negamaxEngine.instance.exports.negamax();
            status = this.negamaxEngine.instance.exports.getStatus();
          }

        activeIndex = this.findActiveBranchIndex();

        switch(status)
          {
            case NEGAMAX_STATUS_RUNNING:
              break;

            case NEGAMAX_STATUS_DONE:
              if(activeIndex >= 0)
                this.harvestCompletedSearch(activeIndex);
              else
                {
                  console.error('Zinn: completed Negamax result has no matching branch ID.');
                  this.collectNodes(-1);
                }
              break;

            case NEGAMAX_STATUS_STOP_REQUESTED:
            case NEGAMAX_STATUS_STOP_TIME:
            case NEGAMAX_STATUS_ABORTED:
            case NEGAMAX_STATUS_ERROR:
              this.collectNodes(activeIndex);
              if(activeIndex >= 0 && this.branches[activeIndex].status == STATUS_SEARCHING)
                this.branches[activeIndex].status = STATUS_DONE_SEARCH;
              break;

            case NEGAMAX_STATUS_IDLE:
              console.error('Zinn: branch is marked SEARCHING while Negamax is IDLE.');
              if(branchIndex >= 0 && branchIndex < this.branches.length)
                this.branches[branchIndex].status = STATUS_DONE_SEARCH;
              break;

            default:
              console.error('Zinn: unknown Negamax status: ' + status);
              break;
          }

        return status;
      }

    /* Harvest a COMPLETED search only after checking both identifiers available to us:
       the Negamax search ID and the root game-state bytes copied into its output buffer. */
    harvestCompletedSearch(branchIndex)
      {
        var branch;
        var depthAchieved;
        var targetDepth;
        var score;
        var i;

        if(branchIndex < 0 || branchIndex >= this.branches.length)
          return false;

        branch = this.branches[branchIndex];

        if(this.negamaxEngine.instance.exports.getSearchId() != branch.id)
          {
            console.error('Zinn: refusing stale Negamax result (search ID mismatch).');
            this.collectNodes(-1);
            branch.status = STATUS_DONE_SEARCH;
            return false;
          }

        for(i = 0; i < _GAMESTATE_BYTE_SIZE; i++)
          if(this.negamaxOutputBuffer[i] != branch.gamestate[i])
            {
              console.error('Zinn: refusing stale Negamax result (root game-state mismatch).');
              this.collectNodes(branchIndex);
              branch.status = STATUS_DONE_SEARCH;
              return false;
            }

        depthAchieved = this.negamaxEngine.instance.exports.finalDepthAchieved();
        targetDepth = this.negamaxEngine.instance.exports.getTargetDepth();
        score = this.negamaxEngine.instance.exports.finalScore();

        this.collectNodes(branchIndex);

        if(depthAchieved != targetDepth)
          {
            console.error('Zinn: completed search reports depth ' + depthAchieved +
                          ' but target depth was ' + targetDepth + '.');
            branch.status = STATUS_DONE_SEARCH;
            return false;
          }

        this.printTTStats(depthAchieved);

        for(i = 0; i < _MOVE_BYTE_SIZE; i++)
          branch.bestMove[i] = this.negamaxOutputBuffer[_GAMESTATE_BYTE_SIZE + 1 + i];

        branch.depth = Math.max(branch.depth, depthAchieved);
        branch.score = score;
        branch.status = STATUS_DONE_SEARCH;

        return true;
      }

    printTTStats(depthAchieved)
      {
        const probes = this.negamaxEngine.instance.exports.getTTProbes();
        const hits = this.negamaxEngine.instance.exports.getTTHits();
        const qualified = this.negamaxEngine.instance.exports.getTTDepthQualified();
        const cutoffs = this.negamaxEngine.instance.exports.getTTCutoffs();
        const hitPct = probes > 0 ? (100.0 * hits / probes).toFixed(2) : '0.00';
        const qualifiedPct = probes > 0 ? (100.0 * qualified / probes).toFixed(2) : '0.00';
        const cutoffPct = probes > 0 ? (100.0 * cutoffs / probes).toFixed(2) : '0.00';

        console.log('  TT depth ' + depthAchieved + ': ');
        console.log('    probes=' + probes);
        console.log('    hits=' + hits + ' (' + hitPct + '%)');
        console.log('    depth-qualified=' + qualified + ' (' + qualifiedPct + '%)');
        console.log('    cutoffs=' + cutoffs + ' (' + cutoffPct + '%)');
      }

    /* Account for work performed by the current Negamax run and reset its per-run counter. */
    collectNodes(branchIndex)
      {
        var nodesSearched;

        nodesSearched = this.negamaxEngine.instance.exports.getNodesSearched();
        if(nodesSearched > 0)
          {
            this.nodeCtr += nodesSearched;
            updateNodeCounter(this.nodeCtr);

            if(branchIndex >= 0 && branchIndex < this.branches.length)
              this.branches[branchIndex].nodeCtr += nodesSearched;
          }

        this.negamaxEngine.instance.exports.resetNodesSearched();
        return nodesSearched;
      }

    /* Advance to the next ponder branch.
       When a complete pass ends, raise the iterative-deepening round by one ply, up to maxPly. */
    advancePonderingBranch()
      {
        this.branchIterator++;

        if(this.branchIterator >= this.branches.length)
          {
            this.branchIterator = 0;
            if(this.currentPly < this.maxPly)
              this.currentPly++;
          }

        return;
      }

    ponderingComplete()
      {
        var i;

        for(i = 0; i < this.branches.length; i++)
          if(!this.branches[i].bookHit && this.branches[i].depth < this.maxPly)
            return false;

        return true;
      }

    /**********************************************************************************************
     Branch construction                                                                          */

    /* Create one branch with a unique persistent ID. */
    makeBranch(gamestate)
      {
        var branch;
        var i;

        branch = {gamestate: new Uint8Array(_GAMESTATE_BYTE_SIZE),
                  id:        this.searchId++,
                  bestMove:  [_NOTHING],
                  depth:     0,
                  status:    STATUS_UNSEARCHED,
                  nodeCtr:   0,
                  bookHit:   false,
                  score:     0.0};

        for(i = 0; i < _GAMESTATE_BYTE_SIZE; i++)
          branch.gamestate[i] = gamestate[i];

        return branch;
      }

    /* Collect all moves the opponent might make and create one future branch for each result. */
    branch()
      {
        var i, j, len;
        var moves;
        var childState;
        var buffer4 = new Uint8Array(4);

        for(i = 0; i < _GAMESTATE_BYTE_SIZE; i++)
          this.evaluationInputGameStateBuffer[i] = gameStateBuffer[i];

        len = this.evaluationEngine.instance.exports.getMoves_eval();
        moves = [];

        for(i = 0; i < len; i++)
          {
            moves.push({move: new Uint8Array(_MOVE_BYTE_SIZE), score: 0});

            for(j = 0; j < _MOVE_BYTE_SIZE; j++)
              moves[moves.length - 1].move[j] = this.evaluationOutputMovesBuffer[i * (_MOVE_BYTE_SIZE + 5) + j];

            for(j = 0; j < 4; j++)
              buffer4[j] = this.evaluationOutputMovesBuffer[i * (_MOVE_BYTE_SIZE + 5) + _MOVE_BYTE_SIZE + j];

            moves[moves.length - 1].score = new DataView(buffer4.buffer, buffer4.byteOffset, 4).getInt32(0, true);
          }

        moves.sort((a, b) => b.score - a.score);
        this.branches = [];

        for(i = 0; i < moves.length; i++)
          {
                                                                    //  Re-copy the parent state defensively before each makeMove_eval().
            for(j = 0; j < _GAMESTATE_BYTE_SIZE; j++)
              this.evaluationInputGameStateBuffer[j] = gameStateBuffer[j];

            for(j = 0; j < _MOVE_BYTE_SIZE; j++)
              this.evaluationInputMoveBuffer[j] = moves[i].move[j];

            this.evaluationEngine.instance.exports.makeMove_eval();

            childState = new Uint8Array(_GAMESTATE_BYTE_SIZE);
            for(j = 0; j < _GAMESTATE_BYTE_SIZE; j++)
              childState[j] = this.evaluationOutputGameStateBuffer[j];

            this.branches.push(this.makeBranch(childState));
          }

        this.branchIterator = 0;
        this.currentPly = 1;
        this.nodeCtr = 0;
        updateNodeCounter(this.nodeCtr);
        this.negamaxEngine.instance.exports.resetNodesSearched();
                                                                    //  Increase transposition table generation (potentially rolling over).
        this.negamaxEngine.instance.exports.incTranspoTableGeneration();

        return;
      }

    /**********************************************************************************************
     Opening Book                                                                                 */

    /* Attempt an opening-book lookup for a particular branch.
       The callback is tied to the immutable branch ID, never to mutable branchIterator.
       A stale response is simply ignored. */
    lookup(branchIndex = this.branchIterator)
      {
        var request;
        var branchId;
        var params;
        var i;

        if(branchIndex < 0 || branchIndex >= this.branches.length)
          return;

        if(this.branches[branchIndex].status != STATUS_UNSEARCHED)
          return;

        branchId = this.branches[branchIndex].id;
        params = 'sendRequest=zinn';
        for(i = 0; i < _GAMESTATE_BYTE_SIZE; i++)
          params += '&b' + i + '=' + this.branches[branchIndex].gamestate[i];

        this.branches[branchIndex].status = STATUS_BOOK_LOOKUP;

        request = new XMLHttpRequest();
        request.open('POST', 'obj/sess/lookup.php', true);
        request.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        request.setRequestHeader('Cache-Control', 'no-cache');

        request.onreadystatechange = () =>
          {
            var index;
            var parse;
            var j;

            if(request.readyState != 4)
              return;

            index = this.findBranchById(branchId);
            if(index < 0)                                           //  The branch was pruned while the request was out.
              return;

            this.branches[index].status = STATUS_DONE_SEARCH;       //  Any lookup failure simply means: search normally.

            if(request.status != 200 || request.responseText == '')
              return;

            parse = request.responseText.split('|');
            if(parse[0] != 'freedom' || parse[1] != 'found')
              return;

            parse = parse[2].split(',');
            if(parse.length < _MOVE_BYTE_SIZE)
              {
                console.warn('Zinn: malformed opening-book move.');
                return;
              }

            for(j = 0; j < _MOVE_BYTE_SIZE; j++)
              this.branches[index].bestMove[j] = parseInt(parse[j]);

            this.branches[index].bookHit = true;
            this.branches[index].status = STATUS_DONE_SEARCH;
          };

        request.send(params);
        return;
      }

    /**********************************************************************************************
     Negamax                                                                                      */

    /* Initialize Negamax for exactly one branch and one target depth. */
    initializeSearch(branchIndex=this.branchIterator, targetDepth=this.currentPly)
      {
        var i;
        var branch;

        if(branchIndex < 0 || branchIndex >= this.branches.length)
          return false;

        branch = this.branches[branchIndex];
        targetDepth = Math.max(1, Math.min(this.maxPly, targetDepth));

        for(i = 0; i < _GAMESTATE_BYTE_SIZE; i++)
          this.negamaxInputBuffer[i] = branch.gamestate[i];

        this.branchIterator = branchIndex;
        this.currentPly = targetDepth;

        this.negamaxEngine.instance.exports.unsetControlFlag(0xFF);
        this.negamaxEngine.instance.exports.resetNodesSearched();
        this.negamaxEngine.instance.exports.setSearchId(branch.id);
        this.negamaxEngine.instance.exports.setTargetDepth(targetDepth);
        //this.prepareRepetitionPathPrefix(branch);
        this.negamaxEngine.instance.exports.initSearch();

        branch.status = STATUS_SEARCHING;
        return true;
      }

    /*
    prepareRepetitionPathPrefix(branch)
      {
        if(this.byteArrCmp(branch.gamestate, gameStateBuffer))
          this.repetitionPathBuffer[0] = 0;
        else
          {
            this.repetitionPathBuffer[0] = 1;

            this.evaluationInputGameStateBuffer.set(gameStateBuffer);
            this.evaluationEngine.instance.exports.repetitionState_eval();

            this.repetitionPathBuffer.set(this.evaluationOutputRepetitionEncBuffer, _REPETITION_PATH_HEADER_SIZE);
          }

        return;
      } */

    findActiveBranchIndex()
      {
        var id;

        if(this.negamaxEngine == null)
          return -1;

        id = this.negamaxEngine.instance.exports.getSearchId();
        return this.findBranchById(id);
      }

    findBranchById(id)
      {
        var i;

        for(i = 0; i < this.branches.length; i++)
          if(this.branches[i].id == id)
            return i;

        return -1;
      }

    findBranchByGameState(gamestate)
      {
        var i;

        for(i = 0; i < this.branches.length; i++)
          if(this.byteArrCmp(gamestate, this.branches[i].gamestate))
            return i;

        return -1;
      }

    hasUsableMove(branch)
      {
        return (branch != null && branch.bestMove[0] != _NOTHING && branch.bestMove[1] != _NOTHING);
      }

    /* Compare two encoded game states byte-for-byte. */
    byteArrCmp(a, b)
      {
        var i;

        if(a == null || b == null || a.length < _GAMESTATE_BYTE_SIZE || b.length < _GAMESTATE_BYTE_SIZE)
          return false;

        for(i = 0; i < _GAMESTATE_BYTE_SIZE; i++)
          {
            if(a[i] != b[i])
              return false;
          }

        return true;
      }

    /**********************************************************************************************
     Taking Action                                                                                */

    playBestMove(branch)
      {
        if(!this.hasUsableMove(branch))
          {
            console.error('Zinn: refusing to play an unset best move.');
            return;
          }

        Select_A = branch.bestMove[0];

        if(this.team == 'Black')
          {
            animationInstruction = {a:Select_A, action:'place-black-stone'};
          }
        else
          {
            animationInstruction = {a:Select_A, action:'place-white-stone'};
          }

        console.log('>>> DEBUG: ' + Select_A);
        this.branches = [];
        this.branchIterator = 0;
        animate();

        return;
      }

    /**********************************************************************************************
     Observing State Updates                                                                      */

    commitRealMove(a)
      {
        //this.evaluationInputGameStateBuffer.set(gameStateBuffer);   //  Canonicalize the position we are ABOUT TO LEAVE.
        //this.evaluationEngine.instance.exports.repetitionState_eval();
        gameEngine.instance.exports.makeMove_client(a);             //  Commit the actual move.

        //if(gameEngine.instance.exports.repetitionBarrier_client())  //  Nothing before such an irreversible move can ever become the current position again.
        //  this.clearRepetitionHistory();
        //else
        //  this.appendRepetitionHistory(this.evaluationOutputRepetitionEncBuffer);

        return;
      }

    /**********************************************************************************************
     Tracking Repeated States                                                                     */

    /*
    repetitionHistoryLength()
      {
        return this.repetitionHistoryView.getUint32(0, true);
      } */

    /*
    setRepetitionHistoryLength(n)
      {
        this.repetitionHistoryView.setUint32(0, n, true);
        return;
      } */

    /*
    clearRepetitionHistory()
      {
        this.setRepetitionHistoryLength(0);
        return;
      } */

    /*
    appendRepetitionHistory(repetitionState)
      {
        const len = this.repetitionHistoryLength();

        if(len >= _REPETITION_HISTORY_CAPACITY)
          throw new Error("Repetition-history buffer overflow.");

        if(repetitionState.length !== _REPETITION_STATE_BYTE_SIZE)
          throw new Error("Invalid repetition-state size.");

        const offset = 4 + len * _REPETITION_STATE_BYTE_SIZE;
        this.repetitionHistoryBuffer.set(repetitionState, offset);
        this.setRepetitionHistoryLength(len + 1);
        return;
      } */
  }
