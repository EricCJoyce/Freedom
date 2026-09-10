const GAME_ONGOING = 0;                                             //  (See C code).
const GAME_OVER_WHITE_WINS = 1;                                     //  (See C code).
const GAME_OVER_BLACK_WINS = 2;                                     //  (See C code).
const GAME_OVER_DRAW = 3;                                           //  (See C code).

function animate()
  {
    animating = true;                                               //  Set the flag: animation is in progress.

    if(animationInstruction != null)
      {
        switch(animationInstruction.action)
          {
            case 'place-white-stone':  Select_A = animationInstruction.a;
                                       placeWhiteStone(animationInstruction.a);
                                       break;
            case 'place-black-stone':  Select_A = animationInstruction.a;
                                       placeBlackStone(animationInstruction.a);
                                       break;
          }
      }
  }

function placeWhiteStone(a)
  {
    putSound();                                                     //  Play a placement sound.

    gamePieces.push(new THREE.Mesh(stoneGeometry, whiteMaterial));
    gamePieces[gamePieces.length - 1].team = 'White';

    zinn.commitRealMove(a);                                         //  HERE UPDATE THE GAME-ENGINE!!! (And OBSERVE the update.)

    gamePieces[gamePieces.length - 1].position.x = convIndexToX(a);
    gamePieces[gamePieces.length - 1].position.y = convIndexToY(a);
    gamePieces[gamePieces.length - 1].position.z = STONE_Z;
    gamePieces[gamePieces.length - 1].scale.x = 1;
    gamePieces[gamePieces.length - 1].scale.y = 1;
    gamePieces[gamePieces.length - 1].scale.z = 1;
    gamePieces[gamePieces.length - 1].rotation.x = Math.PI * 0.5;
    scene.add(gamePieces[gamePieces.length - 1]);

    animationTarget = gamePieces.length - 1;

    animate_startScale = {x: 1,  y: 1,  z: 1};
    animate_endScale   = {x: 10, y: 10, z: 10};

    var tween = new TWEEN.Tween(animate_startScale).to(animate_endScale, 500);
    tween.onUpdate(function()
      {
        gamePieces[animationTarget].scale.x = animate_startScale.x;
        gamePieces[animationTarget].scale.y = animate_startScale.y;
        gamePieces[animationTarget].scale.z = animate_startScale.z;
      });
    tween.onComplete(function()
      {
        swapTurns();
      });
    tween.start();
  }

function placeBlackStone(a)
  {
    putSound();                                                     //  Play a placement sound.

    gamePieces.push(new THREE.Mesh(stoneGeometry, blackMaterial));
    gamePieces[gamePieces.length - 1].team = 'Black';

    zinn.commitRealMove(a);                                         //  HERE UPDATE THE GAME-ENGINE!!! (And OBSERVE the update.)

    gamePieces[gamePieces.length - 1].position.x = convIndexToX(a);
    gamePieces[gamePieces.length - 1].position.y = convIndexToY(a);
    gamePieces[gamePieces.length - 1].position.z = STONE_Z;
    gamePieces[gamePieces.length - 1].scale.x = 1;
    gamePieces[gamePieces.length - 1].scale.y = 1;
    gamePieces[gamePieces.length - 1].scale.z = 1;
    gamePieces[gamePieces.length - 1].rotation.x = Math.PI * 0.5;
    scene.add(gamePieces[gamePieces.length - 1]);

    animationTarget = gamePieces.length - 1;

    animate_startScale = {x: 1,  y: 1,  z: 1};
    animate_endScale   = {x: 10, y: 10, z: 10};

    var tween = new TWEEN.Tween(animate_startScale).to(animate_endScale, 500);
    tween.onUpdate(function()
      {
        gamePieces[animationTarget].scale.x = animate_startScale.x;
        gamePieces[animationTarget].scale.y = animate_startScale.y;
        gamePieces[animationTarget].scale.z = animate_startScale.z;
      });
    tween.onComplete(function()
      {
        swapTurns();
      });
    tween.start();
  }

function swapTurns()
  {
    var winFlag;

    Select_A = _NOTHING;                                            //  Reset.

    winFlag = gameEngine.instance.exports.isWin_client();           //  Is the game state now terminal?
    if(winFlag != GAME_ONGOING)                                     //  The game state is now terminal.
      {
        artworkForThinking(false);                                  //  Pull "thinking" artwork.
        nodeCounter(false);                                         //  Pull the node counter.

        gameOver = true;                                            //  Signal that the game is over.
        chime_mp3.play();                                           //  Play the sound.

        if(winFlag == GAME_OVER_WHITE_WINS)
          {
            switch(currentLang)
              {
                case 'Spanish': alert(alertStringScrub('\xA1El blanco gana!'));  break;
                case 'German': alert(alertStringScrub('Wei&#223; gewinnt!'));  break;
                case 'Polish': alert(alertStringScrub('Bia&#322;y wygrywa!'));  break;
                default: alert(alertStringScrub('White wins!'));
              }
          }
        else if(winFlag == GAME_OVER_BLACK_WINS)
          {
            switch(currentLang)
              {
                case 'Spanish': alert(alertStringScrub('\xA1El negro gana!'));  break;
                case 'German': alert(alertStringScrub('Schwarz gewinnt!'));  break;
                case 'Polish': alert(alertStringScrub('Czarny wygrywa!'));  break;
                default: alert(alertStringScrub('Black wins!'));
              }
          }
        else
          {
            switch(currentLang)
              {
                case 'Spanish': alert(alertStringScrub('\xA1!'));  break;
                case 'German': alert(alertStringScrub('!'));  break;
                case 'Polish': alert(alertStringScrub('!'));  break;
                default: alert(alertStringScrub('Draw!'));
              }
          }
      }
    else                                                            //  The game is still on going.
      {
        if(CurrentTurn == 'White')
          {
            CurrentTurn = 'Black';
            if(zinn.team == 'White')
              {
                HumansTurn = true;
                getMoves();
              }
            else
              HumansTurn = false;
          }
        else
          {
            CurrentTurn = 'White';
            if(zinn.team == 'Black')
              {
                HumansTurn = true;
                getMoves();
              }
            else
              HumansTurn = false;
          }

        animationInstruction = null;                                //  Blank out animation instruction.
        animating = false;                                          //  We are no longer animating.
                                                                    //  Restore control to the human player.
        if((CurrentTurn == 'White' && zinn.team == 'Black') || (CurrentTurn == 'Black' && zinn.team == 'White'))
          {
            artworkForThinking(false);                              //  Pull "thinking" artwork.
            nodeCounter(false);                                     //  Pull the node counter.
            zinn.nodeCtr = 0;                                       //  Reset node counter.
            MasterControl = true;
          }
        else
          {
            artworkForThinking(true);                               //  Show the "thinking" artwork.
            updateNodeCounter(zinn.nodeCtr);                        //  Show the A.I.'s node count.
            nodeCounter(true);                                      //  Show the node counter.
          }

        gameEngine.instance.exports.draw();                         //  Output to the console.
      }

    updateBoard();
    updateScore();

    return;
  }

function updateBoard()
  {
    var i;
    for(i = _A1; i < _NOTHING; i++)
      {
        if(gameEngine.instance.exports.isWhite_client(i) && gameEngine.instance.exports.isLive_client(i))
          liveWhiteSq(i);
        else if(gameEngine.instance.exports.isBlack_client(i) && gameEngine.instance.exports.isLive_client(i))
          liveBlackSq(i);
        else
          normalSq(i);
      }
    return;
  }

function updateScore()
  {
    document.getElementById('white-score-div').innerHTML = gameEngine.instance.exports.whiteScore_client();
    document.getElementById('black-score-div').innerHTML = gameEngine.instance.exports.blackScore_client();
    return;
  }

function putSound()
  {
    switch(Math.floor(Math.random() * 5))
      {
        case 0: put1_mp3.play();  break;
        case 1: put2_mp3.play();  break;
        case 2: put3_mp3.play();  break;
        case 3: put4_mp3.play();  break;
        default: put5_mp3.play();
      }

    return;
  }
