var currentLang = "English";

var lang_en = document.getElementById("lang_en"); ////////////////////  Language selection buttons
var lang_pl = document.getElementById("lang_pl");
var lang_es = document.getElementById("lang_es");
var lang_de = document.getElementById("lang_de");

var techDetails_en; //////////////////////////////////////////////////  Load from file: tech notes about this page
var techDetails_pl;
var techDetails_es;
var techDetails_de;

var gui_en; //////////////////////////////////////////////////////////  Load from file: GUI interface and default settings
var gui_pl;
var gui_es;
var gui_de;

//////////////////////////////////////////////////////////////////////  Interface controls
var NodeCtr = 0;                                                    //  Nodes searched in the previous script call
var thoughtStaged = false;                                          //  Whether or not the Artwork For Thinking SHOULD be visible
var countStaged = false;                                            //  Whether or not the Node Counter SHOULD be visible

var angle = 20;                                                     //  Current camera angle (20 looks nice, let's start with that)
var fullscreenAvailable = THREEx.FullScreen.available();            //  Whether fullscreen is available on this device
var fullscreenActive = false;                                       //  Whether fullscreen is currently active
var showParticles = true;                                           //  Toggle the particle effect

var clockgame = false;                                              //  Whether to use the game clock
var whiteTimeMin = 60;                                              //  Time in minutes to each side
var blackTimeMin = 60;
var whiteTimeSec = 0;                                               //  Seconds remaining to each side
var blackTimeSec = 0;

var panelOpen = false;                                              //  Whether the central panel is open

//////////////////////////////////////////////////////////////////////
//   I N I T s
function initGUI()
  {
    lang_en.addEventListener('click', function() { setlang('English') } );
    lang_pl.addEventListener('click', function() { setlang('Polish') } );
    lang_es.addEventListener('click', function() { setlang('Spanish') } );
    lang_de.addEventListener('click', function() { setlang('German') } );

    loadTechDetails();                                              //  Load tech-details

    loadGUIXML();                                                   //  Load GUI

    loadWebASM();                                                   //  Load engines
  }

//  Load all techDetails varialbes
function loadTechDetails()
  {
    var TechXML_en = new XMLHttpRequest();                          //  IE 7+, Firefox, Chrome, Opera, Safari
    TechXML_en.open("GET", 'obj/xml/about_en.xml', true);
    TechXML_en.onreadystatechange = function()
      {
        if(TechXML_en.readyState == 4 && TechXML_en.status == 200)
          {
            techDetails_en = TechXML_en.responseText;
            elementsLoaded++;
            loadTotalReached();
          }
      };
    TechXML_en.send();

    var TechXML_pl = new XMLHttpRequest();                          //  IE 7+, Firefox, Chrome, Opera, Safari
    TechXML_pl.open("GET", 'obj/xml/about_pl.xml', true);
    TechXML_pl.onreadystatechange = function()
      {
        if(TechXML_pl.readyState == 4 && TechXML_pl.status == 200)
          {
            techDetails_pl = TechXML_pl.responseText;
            elementsLoaded++;
            loadTotalReached();
          }
      };
    TechXML_pl.send();

    var TechXML_es = new XMLHttpRequest();                          //  IE 7+, Firefox, Chrome, Opera, Safari
    TechXML_es.open("GET", 'obj/xml/about_es.xml', true);
    TechXML_es.onreadystatechange = function()
      {
        if(TechXML_es.readyState == 4 && TechXML_es.status == 200)
          {
            techDetails_es = TechXML_es.responseText;
            elementsLoaded++;
            loadTotalReached();
          }
      };
    TechXML_es.send();

    var TechXML_de = new XMLHttpRequest();                          //  IE 7+, Firefox, Chrome, Opera, Safari
    TechXML_de.open("GET", 'obj/xml/about_de.xml', true);
    TechXML_de.onreadystatechange = function()
      {
        if(TechXML_de.readyState == 4 && TechXML_de.status == 200)
          {
            techDetails_de = TechXML_de.responseText;
            elementsLoaded++;
            loadTotalReached();
          }
      };
    TechXML_de.send();
  }

//  Load all control panels
function loadGUIXML()
  {
    var GUIXML_en = new XMLHttpRequest();                           //  IE 7+, Firefox, Chrome, Opera, Safari
    GUIXML_en.open("GET", 'obj/xml/gui_en.xml', true);
    GUIXML_en.onreadystatechange = function()
      {
        if(GUIXML_en.readyState == 4 && GUIXML_en.status == 200)
          {
            gui_en = GUIXML_en.responseText;
            elementsLoaded++;
            loadTotalReached();
          }
      };
    GUIXML_en.send();

    var GUIXML_pl = new XMLHttpRequest();                           //  IE 7+, Firefox, Chrome, Opera, Safari
    GUIXML_pl.open("GET", 'obj/xml/gui_pl.xml', true);
    GUIXML_pl.onreadystatechange = function()
      {
        if(GUIXML_pl.readyState == 4 && GUIXML_pl.status == 200)
          {
            gui_pl = GUIXML_pl.responseText;
            elementsLoaded++;
            loadTotalReached();
          }
      };
    GUIXML_pl.send();

    var GUIXML_es = new XMLHttpRequest();                           //  IE 7+, Firefox, Chrome, Opera, Safari
    GUIXML_es.open("GET", 'obj/xml/gui_es.xml', true);
    GUIXML_es.onreadystatechange = function()
      {
        if(GUIXML_es.readyState == 4 && GUIXML_es.status == 200)
          {
            gui_es = GUIXML_es.responseText;
            elementsLoaded++;
            loadTotalReached();
          }
      };
    GUIXML_es.send();

    var GUIXML_de = new XMLHttpRequest();                           //  IE 7+, Firefox, Chrome, Opera, Safari
    GUIXML_de.open("GET", 'obj/xml/gui_de.xml', true);
    GUIXML_de.onreadystatechange = function()
      {
        if(GUIXML_de.readyState == 4 && GUIXML_de.status == 200)
          {
            gui_de = GUIXML_de.responseText;
            elementsLoaded++;
            loadTotalReached();
          }
      };
    GUIXML_de.send();
  }

//  Load compiled programs.
function loadWebASM()
  {
                                                                    //  Load GameLogic.
    fetch('obj/wasm/gamelogic.wasm', {headers: {'Content-Type': 'application/wasm'} })
    .then(response => response.arrayBuffer())
    .then(bytes =>
      {
        WebAssembly.instantiate(bytes,
          {
            env: {
                   memoryBase: 0,
                   tableBase: 0,
                                                                    //  Malloc 1 page for 5.3 KB file.
                   memory: new WebAssembly.Memory({initial: 1, maximum: 1}),
                   table: new WebAssembly.Table({initial: 0, element: 'anyfunc'}),
                                                                    //  (Fantastically stupid.)
                   _printRow: function(a, b, c, d, e, f, g, h, i, j, anchor)
                     {
                       const _EMPTY       = 0x00;                   //  (See C code.)
                       const _WHITE_STONE = 0x01;
                       const _BLACK_STONE = 0x02;
                       var consoleStr = '';

                       switch(a)
                         {
                           case _EMPTY:        consoleStr += ' . ';  break;
                           case _WHITE_STONE:  if(anchor == 1)
                                                 consoleStr += '[W]';
                                               else
                                                 consoleStr += ' W ';
                                                                     break;
                           case _BLACK_STONE:  if(anchor == 1)
                                                 consoleStr += '[B]';
                                               else
                                                 consoleStr += ' B ';
                                                                     break;
                         }
                       switch(b)
                         {
                           case _EMPTY:        consoleStr += ' . ';  break;
                           case _WHITE_STONE:  if(anchor == 2)
                                                 consoleStr += '[W]';
                                               else
                                                 consoleStr += ' W ';
                                                                     break;
                           case _BLACK_STONE:  if(anchor == 2)
                                                 consoleStr += '[B]';
                                               else
                                                 consoleStr += ' B ';
                                                                     break;
                         }
                       switch(c)
                         {
                           case _EMPTY:        consoleStr += ' . ';  break;
                           case _WHITE_STONE:  if(anchor == 3)
                                                 consoleStr += '[W]';
                                               else
                                                 consoleStr += ' W ';
                                                                     break;
                           case _BLACK_STONE:  if(anchor == 3)
                                                 consoleStr += '[B]';
                                               else
                                                 consoleStr += ' B ';
                                                                     break;
                         }
                       switch(d)
                         {
                           case _EMPTY:        consoleStr += ' . ';  break;
                           case _WHITE_STONE:  if(anchor == 4)
                                                 consoleStr += '[W]';
                                               else
                                                 consoleStr += ' W ';
                                                                     break;
                           case _BLACK_STONE:  if(anchor == 4)
                                                 consoleStr += '[B]';
                                               else
                                                 consoleStr += ' B ';
                                                                     break;
                         }
                       switch(e)
                         {
                           case _EMPTY:        consoleStr += ' . ';  break;
                           case _WHITE_STONE:  if(anchor == 5)
                                                 consoleStr += '[W]';
                                               else
                                                 consoleStr += ' W ';
                                                                     break;
                           case _BLACK_STONE:  if(anchor == 5)
                                                 consoleStr += '[B]';
                                               else
                                                 consoleStr += ' B ';
                                                                     break;
                         }
                       switch(f)
                         {
                           case _EMPTY:        consoleStr += ' . ';  break;
                           case _WHITE_STONE:  if(anchor == 6)
                                                 consoleStr += '[W]';
                                               else
                                                 consoleStr += ' W ';
                                                                     break;
                           case _BLACK_STONE:  if(anchor == 6)
                                                 consoleStr += '[B]';
                                               else
                                                 consoleStr += ' B ';
                                                                     break;
                         }
                       switch(g)
                         {
                           case _EMPTY:        consoleStr += ' . ';  break;
                           case _WHITE_STONE:  if(anchor == 7)
                                                 consoleStr += '[W]';
                                               else
                                                 consoleStr += ' W ';
                                                                     break;
                           case _BLACK_STONE:  if(anchor == 7)
                                                 consoleStr += '[B]';
                                               else
                                                 consoleStr += ' B ';
                                                                     break;
                         }
                       switch(h)
                         {
                           case _EMPTY:        consoleStr += ' . ';  break;
                           case _WHITE_STONE:  if(anchor == 8)
                                                 consoleStr += '[W]';
                                               else
                                                 consoleStr += ' W ';
                                                                     break;
                           case _BLACK_STONE:  if(anchor == 8)
                                                 consoleStr += '[B]';
                                               else
                                                 consoleStr += ' B ';
                                                                     break;
                         }
                       switch(i)
                         {
                           case _EMPTY:        consoleStr += ' . ';  break;
                           case _WHITE_STONE:  if(anchor == 9)
                                                 consoleStr += '[W]';
                                               else
                                                 consoleStr += ' W ';
                                                                     break;
                           case _BLACK_STONE:  if(anchor == 9)
                                                 consoleStr += '[B]';
                                               else
                                                 consoleStr += ' B ';
                                                                     break;
                         }
                       switch(j)
                         {
                           case _EMPTY:        consoleStr += ' . ';  break;
                           case _WHITE_STONE:  if(anchor == 10)
                                                 consoleStr += '[W]';
                                               else
                                                 consoleStr += ' W ';
                                                                     break;
                           case _BLACK_STONE:  if(anchor == 10)
                                                 consoleStr += '[B]';
                                               else
                                                 consoleStr += ' B ';
                                                                     break;
                         }

                       console.log(consoleStr);
                     },
                   _printGameStateData: function(wToMove, prevMove)
                     {
                       var castlingString;

                       if(wToMove)
                         console.log('White to move.');
                       else
                         console.log('Black to move.');

                       if(prevMove == 100)
                         console.log('Freedom!');
                       else
                         console.log('Anchor index = ' + prevMove);
                     }
                 }
          })
        .then(instance =>
          {
            gameEngine = instance;
                                                                    //  Assign offset.
            gameStateOffset = gameEngine.instance.exports.getCurrentState();
            gameStateBuffer = new Uint8Array(gameEngine.instance.exports.memory.buffer, gameStateOffset, _GAMESTATE_BYTE_SIZE);
                                                                    //  Assign offset.
            gameOutputOffset = gameEngine.instance.exports.getMovesBuffer();
            gameOutputBuffer = new Uint8Array(gameEngine.instance.exports.memory.buffer, gameOutputOffset, _MOVEBUFFER_BYTE_SIZE);

            elementsLoaded++;                                       //  Check this load off our list.
            loadTotalReached();                                     //  Check the total.
            console.log('Game-Logic WASM memory: ', (gameEngine.instance.exports.memory.buffer.byteLength / (1024 * 1024)).toFixed(2), 'MiB');
          });
      });
  }

//////////////////////////////////////////////////////////////////////
//   B U T T O N    E V E N T S

//  Change camera position and rotation according to this angle.
function updateAngle()
  {
    var x = parseInt(document.getElementById('angle-slider').value);

    document.getElementById('angle-num').innerHTML = x;
    resetCameraPositionAngle(x);
  }

//  Toggle fullscreen
function updateFullscreenToggle()
  {
    fullscreenActive = !fullscreenActive;
    toggleFullscreen(fullscreenActive);
  }

//  Toggle particles
function updateParticleToggle()
  {
    showParticles = !showParticles;
    toggleParticleEffect(showParticles);
  }

//  Update search depth
function updatePlies()
  {
    var x = parseInt(document.getElementById('plies-slider').value);
    document.getElementById('plies-num').innerHTML = x;

    zinn.maxPly = x;                                                //  Change the number of ply.
  }

//  Toggle A.I. side to play
function updateAIPlaysBlack()
  {
    if(!gameStarted)
      {
        Options = [];                                               //  IMPLEMENTATION-SPECIFIC

        if(zinn.team == 'Black')
          {
            zinn.team = 'White';
            MasterControl = false;
            HumansTurn = false;
          }

        pullGUIComponents();                                        //  Cue the A.I. to make the first move
                                                                    //  It now becomes the A.I.'s turn!
        artworkForThinking(true);                                   //  Show the "thinking" artwork.
        updateNodeCounter(zinn.nodeCtr);                            //  Show the A.I.'s node count.
        nodeCounter(true);                                          //  Show the node counter.
      }
  }

//  Enable/Disable time control.
function updateTimeControl()
  {
    var whiteClock = document.getElementById('white-clock');
    var blackClock = document.getElementById('black-clock');
    var whiteClockDiv = document.getElementById('white-clock-div');
    var blackClockDiv = document.getElementById('black-clock-div');

    if(!gameStarted)
      {
        clockgame = !clockgame;

        if(clockgame)
          {
            whiteClock.style.visibility = 'visible';
            blackClock.style.visibility = 'visible';
            if(!isNaN(whiteTimeMin))
              whiteClockDiv.innerHTML = whiteTimeMin + ':00';
            if(!isNaN(blackTimeMin))
              blackClockDiv.innerHTML = blackTimeMin + ':00';
            whiteClockDiv.style.visibility = 'visible';
            blackClockDiv.style.visibility = 'visible';
          }
        else
          {
            whiteClock.style.visibility = 'hidden';
            blackClock.style.visibility = 'hidden';
            whiteClockDiv.innerHTML = '';
            blackClockDiv.innerHTML = '';
            whiteClockDiv.style.visibility = 'hidden';
            blackClockDiv.style.visibility = 'hidden';
          }
      }
  }

//  Update minutes allotted to each side.
function updateClockTimeAllotted()
  {
    var whiteClock = document.getElementById('white-clock-div');
    var blackClock = document.getElementById('black-clock-div');
    var x = parseInt(document.getElementById('minutesallocated-input').value);

    if(!gameStarted)
      {
        if(!isNaN(x))
          {
            whiteTimeMin = blackTimeMin = x;

            whiteClock.innerHTML = whiteTimeMin + ':00';
            blackClock.innerHTML = blackTimeMin + ':00';
          }
      }
  }

//  Commit to some settings and pull them from the control panel
function pullGUIComponents()
  {
    gameStarted = true;                                             //  The game has officially begun.
                                                                    //  Commit to the choices made for play and pull their controls from the control panel.
    document.getElementById('switch-sides-tr').style.display = 'none';
    document.getElementById('AIblack').removeAttribute('onclick');

    document.getElementById('timecontrol-label').style.display = 'none';
    document.getElementById('useclock-label').style.display = 'none';
    document.getElementById('timecontrol').style.display = 'none';
    document.getElementById('timecontrol').removeAttribute('onclick');

    document.getElementById('minutesallocated-label').style.display = 'none';
    document.getElementById('minutesallocated-input').style.display = 'none';
    document.getElementById('minutesallocated-input').removeAttribute('oninput');
  }

//  Open the central panel.
function popPanel()
  {
    document.getElementById('panel-content').classList.remove('collapsed-panel');
    document.getElementById('centralpanel').classList.add('popped-panel');
    setPanelToggleReveal(false);
    panelOpen = true;
  }

//  Close the central panel.
function hidePanel()
  {
    document.getElementById('centralpanel').classList.remove('popped-panel');
    document.getElementById('panel-content').classList.add('collapsed-panel');
    setPanelToggleReveal(true);
    panelOpen = false;
  }

//  Call setPanelToggleReveal(true) to make the button reveal the panel.
//  Call setPanelToggleReveal(false) to make the button collapse the panel.
function setPanelToggleReveal(b)
  {
    var t = document.getElementById('panel-toggle');

    if(b)                                                           //  Arrow pointing up is &#9650;
      t.innerHTML = '<a href="javascript:void(0);" onclick="popPanel();">&#9650;</a>';
    else                                                            //  Arrow pointing down is &#9660;
      t.innerHTML = '<a href="javascript:void(0);" onclick="hidePanel();">&#9660;</a>';
  }

function setlang(lang)
  {
    switch(lang)
      {
        case 'Polish':
             currentLang = "Polish";
             break;
        case 'Spanish':
             currentLang = "Spanish";
             break;
        case 'German':
             currentLang = "German";
             break;
        default:
             currentLang = "English";
             break;
      }
    updateGUIlabels();
  }

function updateGUIlabels()
  {
    switch(currentLang)
      {
        case 'Polish':  document.getElementById('project-title').innerHTML = 'Wolno&#347;&#263;';
                        document.getElementById("node-counter-label").innerHTML = 'W&#281;z&#322;y rozwi&#261;zywane:';

                        document.getElementById('view-label').innerHTML = 'Widzenie';
                        document.getElementById('angle-label').innerHTML = 'K&#261;t';
                        document.getElementById('fullscreen-label').innerHTML = 'Ca&#322;y ekran';
                        document.getElementById('particles-label').innerHTML = 'Niebo';
                        document.getElementById('ai-label').innerHTML = 'A.I.';
                        document.getElementById('plies-label').innerHTML = 'Poziomy';
                        document.getElementById('aiblack-label').innerHTML = 'A.I. gra czarnymi';
                        document.getElementById('timecontrol-label').innerHTML = 'Zegar szachowy';
                        document.getElementById('useclock-label').innerHTML = 'U&#380;ywaj zegara';
                        document.getElementById('minutesallocated-label').innerHTML = 'Limit minut';

                        document.getElementById('tech-details').innerHTML = techDetails_pl;
                        break;
        case 'Spanish': document.getElementById('project-title').innerHTML = 'Libertad';
                        document.getElementById("node-counter-label").innerHTML = 'Nodos evaluados:';

                        document.getElementById('view-label').innerHTML = 'Visto';
                        document.getElementById('angle-label').innerHTML = '&#193;ngulo';
                        document.getElementById('fullscreen-label').innerHTML = 'Pantalla completa';
                        document.getElementById('particles-label').innerHTML = 'Cielo';
                        document.getElementById('ai-label').innerHTML = 'A.I.';
                        document.getElementById('plies-label').innerHTML = 'Niveles';
                        document.getElementById('aiblack-label').innerHTML = 'A.I. juega las piezas negras';
                        document.getElementById('timecontrol-label').innerHTML = 'Reloj de ajedrez';
                        document.getElementById('useclock-label').innerHTML = 'Usa el reloj';
                        document.getElementById('minutesallocated-label').innerHTML = 'Minutos';

                        document.getElementById('tech-details').innerHTML = techDetails_es;
                        break;
        case 'German':  document.getElementById('project-title').innerHTML = 'Freiheit';
                        document.getElementById("node-counter-label").innerHTML = 'Knoten untersucht:';

                        document.getElementById('view-label').innerHTML = 'Sicht';
                        document.getElementById('angle-label').innerHTML = 'Blickwinkel';
                        document.getElementById('fullscreen-label').innerHTML = 'Vollbildansicht';
                        document.getElementById('particles-label').innerHTML = 'Himmel';
                        document.getElementById('ai-label').innerHTML = 'A.I.';
                        document.getElementById('plies-label').innerHTML = 'Halbz&#252;ge voraus';
                        document.getElementById('aiblack-label').innerHTML = 'A.I. spielt Schwarz';
                        document.getElementById('timecontrol-label').innerHTML = 'Bedenkzeit';
                        document.getElementById('useclock-label').innerHTML = 'Schachuhr';
                        document.getElementById('minutesallocated-label').innerHTML = 'Minuten';

                        document.getElementById('tech-details').innerHTML = techDetails_de;
                        break;
        default:        document.getElementById('project-title').innerHTML = 'Freedom';
                        document.getElementById("node-counter-label").innerHTML = 'Nodes searched:';

                        document.getElementById('view-label').innerHTML = 'View';
                        document.getElementById('angle-label').innerHTML = 'Angle';
                        document.getElementById('fullscreen-label').innerHTML = 'Fullscreen';
                        document.getElementById('particles-label').innerHTML = 'Sky';
                        document.getElementById('ai-label').innerHTML = 'A.I.';
                        document.getElementById('plies-label').innerHTML = 'Plies';
                        document.getElementById('aiblack-label').innerHTML = 'A.I. plays black';
                        document.getElementById('timecontrol-label').innerHTML = 'Time control';
                        document.getElementById('useclock-label').innerHTML = 'Timed game';
                        document.getElementById('minutesallocated-label').innerHTML = 'Minutes allocated';

                        document.getElementById('tech-details').innerHTML = techDetails_en;
      }
  }

function toggleParticleEffect(b)
  {
    var flowfield = document.getElementById('flowfield');
    if(b)
      flowfield.style.visibility = 'visible';
    else
      flowfield.style.visibility = 'hidden';
  }

//////////////////////////////////////////////////////////////////////////////////////
//   D I S P L A Y    A C T I O N S
function artworkForThinking(b)
  {
    if(b)
      document.getElementById("thinking-icon").className = "thinking";
    else
      document.getElementById("thinking-icon").className = "not-thinking";

    thoughtStaged = b;
  }

function nodeCounter(b)
  {
    if(b)
      {
        document.getElementById("node-counter-label").className = "show-node";
        document.getElementById("node-counter").className = "show-node";
      }
    else
      {
        document.getElementById("node-counter-label").className = "hide-node";
        document.getElementById("node-counter").className = "hide-node";
      }

    countStaged = b;
  }

function updateNodeCounter(ctr)
  {
    document.getElementById("node-counter").innerHTML = ctr;
  }

initGUI();
