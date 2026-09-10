//////////////////////////////////////////////////////////////////////
//   R E N D E R I N G   (Things to do every instant)
function render()
  {
    if(elementsLoaded == ELEMENTS_TO_LOAD)                          //  If all textures are loaded
      {
        if(gameStarted && !gameOver)
          {
            var currentTime = Date.now();                           //  Update game clock
            var whiteClockDiv = document.getElementById('white-clock-div');
            var blackClockDiv = document.getElementById('black-clock-div');

            if(currentTime - previousSecond >= 1000)                //  In milliseconds.
              {
                previousSecond = currentTime;                       //  Update the previous second.
                                                                    //  If this is a clock game, we must update the clock...
                if(gameStarted && clockgame && !gameOver)
                  {
                    if(CurrentTurn == 'White')
                      {
                        if(whiteTimeSec == 0 && whiteTimeMin == 0)  //  White has run out the clock.
                          {
                            gameOver = true;
                            artworkForThinking(false);
                            nodeCounter(false);
                            switch(currentLang)
                              {
                                case 'Spanish': alertString  = 'Se acab\xF3 el tiempo.\n';
                                                alertString += '\xA1El equipo negro gana!';
                                                break;
                                case 'German':  alertString  = 'Die Zeit ist vorbei.\n';
                                                alertString += 'Das schwarze Team gewinnt!';
                                                break;
                                case 'Polish':  alertString  = 'Czas min\u0105\u0142.\n';
                                                alertString += 'Czarny zesp\xF3\u0142 wygra!';
                                                break;
                                default:        alertString  = 'Time\'s up.\n';
                                                alertString += 'Black wins!';
                              }
                            alert(alertStringScrub(alertString));
                          }
                        else if(--whiteTimeSec < 0)
                          {
                                                                    //  Is White's time up?
                            if(whiteTimeMin <= 0 && whiteTimeSec <= 0)
                              whiteTimeSec = 0;                     //  Avoid having the clock say -1.
                            else
                              {
                                whiteTimeMin--;
                                whiteTimeSec = 59;
                              }
                          }
                      }
                    else
                      {
                        if(blackTimeSec == 0 && blackTimeMin == 0)  //  Black has run out the clock.
                          {
                            gameOver = true;
                            artworkForThinking(false);
                            nodeCounter(false);
                            switch(currentLang)
                              {
                                case 'Spanish': alertString  = 'Se acab\xF3 el tiempo.\n';
                                                alertString += '\xA1El equipo blanco gana!';
                                                break;
                                case 'German':  alertString  = 'Die Zeit ist vorbei.\n';
                                                alertString += 'Das wei\xDFe Team gewinnt!';
                                                break;
                                case 'Polish':  alertString  = 'Czas min\u0105\u0142.\n';
                                                alertString += 'Bia\u0142y zesp\xF3\u0142 wygra!';
                                                break;
                                default:        alertString  = 'Time\'s up.\n';
                                                alertString += 'White wins!';
                              }
                            alert(alertStringScrub(alertString));
                          }
                        else if(--blackTimeSec < 0)
                          {
                                                                    //  Is Black's time up?
                            if(blackTimeMin <= 0 && blackTimeSec <= 0)
                              blackTimeSec = 0;                     //  Avoid having the clock say -1.
                            else
                              {
                                blackTimeMin--;
                                blackTimeSec = 59;
                              }
                          }
                      }

                    whiteClockDiv.innerHTML = whiteTimeMin + ':' + ("0" + whiteTimeSec).slice(-2);
                    blackClockDiv.innerHTML = blackTimeMin + ':' + ("0" + blackTimeSec).slice(-2);
                  }
              }

            if(!gameOver && !animating)                             //  Unless the game is over or the state is in transition...
              {
                zinn.step();                                        //  Pulse the Player.
              }
          }

        TWEEN.update();                                             //  Update any/all Tweens

        requestAnimationFrame(render);
        renderer.render(scene, camera);                             //  Draw
      }
  }