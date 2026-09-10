if(!Detector.webgl)
  Detector.addGetWebGLMessage();

var gameEngine;                                                     //  Compiled WebASM Module.
var gameStateOffset;                                                //  Address of read/write memory in gameEngine.
var gameStateBuffer;                                                //  Byte buffer
var gameOutputOffset;                                               //  Address of read/write memory in gameEngine.
var gameOutputBuffer;                                               //  Byte buffer

const _GAMESTATE_BYTE_SIZE = 27;                                    //  Size (see C code).
const _MOVE_BYTE_SIZE = 1;                                          //  Size (see C code).
const _MOVEBUFFER_BYTE_SIZE = 100;                                  //  Size (see C code).
const _MAX_MOVES = 100;                                             //  Size (see C code).
const _ZHASH_TABLE_SIZE = 301;                                      //  Size (see C code).
const _HASH_VALUE_BYTE_SIZE = 8;                                    //  Size of long long.
const _TRANSPO_TABLE_SIZE = 524288;                                 //  Size (see C code).
const _TRANSPO_RECORD_BYTE_SIZE = 16;                               //  Size (see C code).
const _PARAMETER_ARRAY_SIZE = 12;                                   //  Size (see C++ code).
const _NEGAMAX_NODE_STACK_CAPACITY = 32;                            //  Size (see C++ code).
const _NEGAMAX_MOVE_ARENA_CAPACITY = 4096;                          //  Size (see C++ code).
const _NEGAMAX_NODE_BYTE_SIZE = 77;                                 //  Size (see C++ code).
const _NEGAMAX_MOVE_BYTE_SIZE = 2;                                  //  Size (see C++ code).
const _KILLER_MOVE_PER_PLY = 2;                                     //  Size (see C++ code).
const _KILLER_MOVE_MAX_DEPTH = 64;                                  //  Size (see C++ code).
const _STATS_BUFFER_SIZE = 16;                                      //  Size (see C++ code).
//const _REPETITION_HISTORY_CAPACITY = 150;                           //  (See C++ code.)
//const _REPETITION_HASH_BYTE_SIZE = 16;                              //  (See C++ code.)
//const _REPETITION_PATH_CAPACITY = _NEGAMAX_NODE_STACK_CAPACITY;
//const _REPETITION_PATH_PREFIX_CAPACITY = 1;                         //  (See C++ code.)
//const _REPETITION_PATH_HEADER_SIZE = 1;                             //  (See C++ code.)
//const _REPETITION_STATE_BYTE_SIZE = 66;                             //  (See C++ code.)

var zinn = new Player();                                            //  Create the A.I. agent.

//////////////////////////////////////////////////////////////////////  Controls
var gameStarted = false;                                            //  When the clock starts running, if there's a clock.

//////////////////////////////////////////////////////////////////////  Window dimensions
var screenWidth = window.innerWidth;
var screenHeight = window.innerHeight;

//////////////////////////////////////////////////////////////////////  CONSTANTS
const COLS = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J'];
const ROWS = ['1', '2', '3', '4', '5', '6', '7', '8', '9', '10'];
const _A1  = 0,  _B1  = 1,  _C1  = 2,  _D1  = 3,  _E1  = 4,  _F1  = 5,  _G1  = 6,  _H1  = 7,  _I1  = 8,  _J1  = 9;
const _A2  = 10, _B2  = 11, _C2  = 12, _D2  = 13, _E2  = 14, _F2  = 15, _G2  = 16, _H2  = 17, _I2  = 18, _J2  = 19;
const _A3  = 20, _B3  = 21, _C3  = 22, _D3  = 23, _E3  = 24, _F3  = 25, _G3  = 26, _H3  = 27, _I3  = 28, _J3  = 29;
const _A4  = 30, _B4  = 31, _C4  = 32, _D4  = 33, _E4  = 34, _F4  = 35, _G4  = 36, _H4  = 37, _I4  = 38, _J4  = 39;
const _A5  = 40, _B5  = 41, _C5  = 42, _D5  = 43, _E5  = 44, _F5  = 45, _G5  = 46, _H5  = 47, _I5  = 48, _J5  = 49;
const _A6  = 50, _B6  = 51, _C6  = 52, _D6  = 53, _E6  = 54, _F6  = 55, _G6  = 56, _H6  = 57, _I6  = 58, _J6  = 59;
const _A7  = 60, _B7  = 61, _C7  = 62, _D7  = 63, _E7  = 64, _F7  = 65, _G7  = 66, _H7  = 67, _I7  = 68, _J7  = 69;
const _A8  = 70, _B8  = 71, _C8  = 72, _D8  = 73, _E8  = 74, _F8  = 75, _G8  = 76, _H8  = 77, _I8  = 78, _J8  = 79;
const _A9  = 80, _B9  = 81, _C9  = 82, _D9  = 83, _E9  = 84, _F9  = 85, _G9  = 86, _H9  = 87, _I9  = 88, _J9  = 89;
const _A10 = 90, _B10 = 91, _C10 = 92, _D10 = 93, _E10 = 94, _F10 = 95, _G10 = 96, _H10 = 97, _I10 = 98, _J10 = 99;
const _NOTHING = 100;

//////////////////////////////////////////////////////////////////////  Three.js
var VIEW_ANGLE = 45;
var ASPECT = screenWidth / screenHeight;
var NEAR = 0.1;
var FAR = 10000;
var container = document.getElementById('container');               //  Contains the game
var renderer, camera, scene;
var CAMERA_X, CAMERA_Y, CAMERA_Z;
const PREFERRED_M = -0.3079928;                                     //  IMPLEMENTATION-SPECIFIC:
const PREFERRED_C = 617.1691;                                       //  CAMERA_Z = M(minimum-screen-dimension) + C
                                                                    //  Keep the board at a comfortable but ample visual distance.
//////////////////////////////////////////////////////////////////////  Board squares
var boardSquares = [];                                              //  Array of PlaneGeometry objects
var normalMaterials = [];                                           //  Array of texture maps
var targetedMaterials = [];                                         //  Array of texture maps
var whiteLiveMaterials = [];                                        //  Array of texture maps
var blackLiveMaterials = [];                                        //  Array of texture maps

const BOARD_SQ_WIDTH = 10, BOARD_SQ_HEIGHT = 10;                    //  IMPLEMENTATION-SPECIFIC
const SQ_WIDTH = 22, SQ_HEIGHT = 22, SQ_W_SEG = 1, SQ_H_SEG = 1;
const SQ_OFFSET = 23;

//////////////////////////////////////////////////////////////////////  Board pieces
var stoneModelLoader, stoneGeometry;                                //  Template loaders
var gamePieces = [];

//////////////////////////////////////////////////////////////////////  Piece materials
var whiteMaterial, blackMaterial;

//////////////////////////////////////////////////////////////////////  Sounds
var put1_mp3, put2_mp3, put3_mp3, put4_mp3, put5_mp3, chime_mp3, youlose_mp3, error_mp3;

//////////////////////////////////////////////////////////////////////  Lights
var ambientLight;
var directionalLight1, directionalLight2;

//////////////////////////////////////////////////////////////////////  Game control
var MasterControl = false;                                          //  Shuts on/off all interactivity
var animating = false;
var animationInstruction = null;

var gameOver = false;                                               //  Has it ended?

var Select_A = _NOTHING;
var HumansTurn = true;
var CurrentTurn = 'White';                                          //  Whose turn to play

//////////////////////////////////////////////////////////////////////  Game clock
var previousSecond = Date.now();                                    //  Track the last millisecond

//////////////////////////////////////////////////////////////////////  Game logic
var Options = [];                                                   //  Array of indices.

//////////////////////////////////////////////////////////////////////  Game piece animation control
const STONE_Z = 1;
var animationTarget;                                                //  Maintains Mesh index when tweening
var animate_startScale;
var animate_endScale;

//////////////////////////////////////////////////////////////////////  Load-targets
var elementsLoaded = 0;                                             //  Track objects to load
const ELEMENTS_TO_LOAD = 422;                                       //  100 squares: normal, targeted, white, black + 1 mesh
                                                                    //  (DefaultLoadingManager excludes JSON meshes)
                                                                    //  +8 audio files
                                                                    //  +1 game logic WebASM module
                                                                    //  +1 evaluation WebASM module
                                                                    //  +1 evaluation module weights
                                                                    //  +1 tree-search WebASM module
                                                                    //  +1 Zobrist hasher
                                                                    //  +4 tech details (en, pl, es, de)
                                                                    //  +4 control panels (en, pl, es, de)
THREE.DefaultLoadingManager.onProgress = function(item, loaded, total)
  {
    elementsLoaded++;
    loadTotalReached();                                             //  Test for load complete
  };

init3D();
initScene();
initEvents();
initSounds();

//////////////////////////////////////////////////////////////////////
//   I N I T s
function init3D()
  {
    scene = new THREE.Scene();
    camera = new THREE.PerspectiveCamera(VIEW_ANGLE, ASPECT, NEAR, FAR);
    renderer = new THREE.WebGLRenderer( {alpha: true} );

    CAMERA_X = 0;
    CAMERA_Y = 0;
    CAMERA_Z = PREFERRED_M * Math.min(screenWidth, screenHeight) + PREFERRED_C;

    scene.add(camera);
    camera.position.set(CAMERA_X, CAMERA_Y, CAMERA_Z);
    renderer.setSize(screenWidth, screenHeight);
    container.append(renderer.domElement);

    document.querySelectorAll('canvas')[0].id = "interactivelayer"; //  Distinguish this canvas from the "flowfield" canvas

    var canvas = document.getElementById('interactivelayer');       //  Add event listener for context loss:
                                                                    //  put yourself back together
    canvas.addEventListener("webglcontextlost", restoreLostContext, false)

    renderer.render(scene, camera);
  }

function initScene()
  {
    initLights();
    initBoard();
    initPieces();
  }

function initLights()
  {
    ambientLight = new THREE.AmbientLight(0xc0c0c0);

    directionalLight1 = new THREE.DirectionalLight(0xffffff, 0.3);
    directionalLight1.position.set(1, -1, 1).normalize();

    directionalLight2 = new THREE.DirectionalLight(0xffffff, 0.1);
    directionalLight2.position.set(1, 1, -1).normalize();

    scene.add(ambientLight);
    scene.add(directionalLight1);
    scene.add(directionalLight2);
  }

function initBoard()
  {
    var i;
    var row_ctr = 0;
    var col_ctr = 0;

    for(i = _A1; i < _NOTHING; i++)
      {
        var normalmaterial = new THREE.MeshLambertMaterial({
            map: THREE.ImageUtils.loadTexture('obj/img/board/' + COLS[col_ctr] + ROWS[row_ctr] + '.png')
          });
        var targetedmaterial = new THREE.MeshLambertMaterial({
            map: THREE.ImageUtils.loadTexture('obj/img/board/' + COLS[col_ctr] + ROWS[row_ctr] + 't.png')
          });
        var whitematerial = new THREE.MeshLambertMaterial({
            map: THREE.ImageUtils.loadTexture('obj/img/board/' + COLS[col_ctr] + ROWS[row_ctr] + 'w.png')
          });
        var blackmaterial = new THREE.MeshLambertMaterial({
            map: THREE.ImageUtils.loadTexture('obj/img/board/' + COLS[col_ctr] + ROWS[row_ctr] + 'b.png')
          });
        normalMaterials.push(normalmaterial);
        targetedMaterials.push(targetedmaterial);
        whiteLiveMaterials.push(whitematerial);
        blackLiveMaterials.push(blackmaterial);
        col_ctr++;
        if(col_ctr > BOARD_SQ_WIDTH - 1)
          {
            col_ctr = 0;
            row_ctr++;
          }
      }
  }

function buildBoard()
  {
    var i;
    var x_offset = -80 - SQ_OFFSET;                                 //  IMPLEMENTATION-SPECIFIC
    var y_offset = -80 - SQ_OFFSET;                                 //  board fudge
    var target_x = 0;
    var target_y = 0;
    var row_ctr = 0;
    var col_ctr = 0;

    var planegeom = new THREE.PlaneGeometry(SQ_WIDTH, SQ_HEIGHT, SQ_W_SEG, SQ_H_SEG);

    for(i = _A1; i < _NOTHING; i++)
      {
        var plane = new THREE.Mesh(planegeom, normalMaterials[i]);
        plane.name = i;
        plane.position.set(target_x + x_offset, target_y + y_offset, 0);
        boardSquares.push(plane);
        scene.add(boardSquares[boardSquares.length - 1]);
        target_x += SQ_OFFSET;
        col_ctr++;
        if(target_x > (SQ_OFFSET * (BOARD_SQ_WIDTH - 1)))
          {
            target_x = 0;
            target_y += SQ_OFFSET;
            col_ctr = 0;
            row_ctr++;
          }
      }
  }

function initPieces()
  {
    whiteMaterial = new THREE.MeshLambertMaterial(
      {
        color: 0xEDF5FF,
        emissive: 0x000000
      });
    blackMaterial = new THREE.MeshPhongMaterial(
      {
        color: 0x171717,
        emissive: 0x000000,
        specular: 0xB9B9B9
      });

    stoneModelLoader = new THREE.JSONLoader();
    stoneModelLoader.load('obj/collada/Stone.json', function(geometry)
      {
        stoneGeometry = geometry;

        elementsLoaded++;
        loadTotalReached();
      });
  }

function initSounds()
  {
    put1_mp3 = new Audio();
    put1_mp3.addEventListener('canplaythrough', () => { elementsLoaded++; loadTotalReached(); }, { once: true });
    put1_mp3.src = 'obj/mp3/put1.mp3';

    put2_mp3 = new Audio();
    put2_mp3.addEventListener('canplaythrough', () => { elementsLoaded++; loadTotalReached(); }, { once: true });
    put2_mp3.src = 'obj/mp3/put2.mp3';

    put3_mp3 = new Audio();
    put3_mp3.addEventListener('canplaythrough', () => { elementsLoaded++; loadTotalReached(); }, { once: true });
    put3_mp3.src = 'obj/mp3/put3.mp3';

    put4_mp3 = new Audio();
    put4_mp3.addEventListener('canplaythrough', () => { elementsLoaded++; loadTotalReached(); }, { once: true });
    put4_mp3.src = 'obj/mp3/put4.mp3';

    put5_mp3 = new Audio();
    put5_mp3.addEventListener('canplaythrough', () => { elementsLoaded++; loadTotalReached(); }, { once: true });
    put5_mp3.src = 'obj/mp3/put5.mp3';

    chime_mp3 = new Audio();
    chime_mp3.addEventListener('canplaythrough', () => { elementsLoaded++; loadTotalReached(); }, { once: true });
    chime_mp3.src = 'obj/mp3/chime.mp3';

    youlose_mp3 = new Audio();
    youlose_mp3.addEventListener('canplaythrough', () => { elementsLoaded++; loadTotalReached(); }, { once: true });
    youlose_mp3.src = 'obj/mp3/youlose.mp3';

    error_mp3 = new Audio();
    error_mp3.addEventListener('canplaythrough', () => { elementsLoaded++; loadTotalReached(); }, { once: true });
    error_mp3.src = 'obj/mp3/error.mp3';
  }

function initEvents()
  {
    window.addEventListener('resize', onWindowResize, false);
    window.addEventListener('mousedown', onClick, false);
    document.addEventListener('touchstart', onTouch, false);
    document.addEventListener('mousemove', onMouseMove, false);
  }

//////////////////////////////////////////////////////////////////////
//   L O A D I N G
function loadTotalReached()
  {
    var element = document.getElementById('loadingbanner');
    var ldBanner = document.getElementById('percentLoaded');
    var maxPlySlider;

    if(elementsLoaded == ELEMENTS_TO_LOAD)
      {
        element.parentNode.removeChild(element);                    //  Remove "Loading . . ." banner

        buildBoard();
        begin();

        reqSess();                                                  //  Load initial byte array into GameState buffer.

        maxPlySlider = document.getElementById('plies-slider');     //  Set the maximum, according to the WASM.
        maxPlySlider.max = zinn.negamaxEngine.instance.exports.getMaxPly();
      }
    else
      ldBanner.innerHTML = Math.round(elementsLoaded / ELEMENTS_TO_LOAD * 100) + ' %';
  }

//////////////////////////////////////////////////////////////////////
//   L A U N C H
function begin()
  {
    var tech = document.getElementById('tech-details');
    var theGui = document.getElementById('control-panel');

    resetCameraPositionAngle(20);                                   //  Default camera angle (the nicest in my opinion)
    render();                                                       //  Begin animation

    switch(currentLang)                                             //  Load interface components
      {
        case 'Polish':  tech.innerHTML = techDetails_pl;
                        theGui.innerHTML = gui_pl;
                        break;
        case 'Spanish': tech.innerHTML = techDetails_es;
                        theGui.innerHTML = gui_es;
                        break;
        case 'German':  tech.innerHTML = techDetails_de;
                        theGui.innerHTML = gui_de;
                        break;
        default:        tech.innerHTML = techDetails_en;
                        theGui.innerHTML = gui_en;
                        break;
      }
                                                                    //  If fullscreen is not available on this
    if(!fullscreenAvailable)                                        //  device, then hide the switch
      document.getElementById('fullscreen-row').style.display = "none";

    setPanelToggleReveal(true);                                     //  Enable panel toggle

    MasterControl = true;                                           //  BEGIN !!
  }

//////////////////////////////////////////////////////////////////////
//   R E S T O R E   W E B G L   C O N T E X T
function restoreLostContext(e)
  {
    e.preventDefault();                                             //  Prevent default action

    if(DEBUG_VERBOSE)
      console.log('WebGL context crashed');

    //cancelRequestAnimationFrame(requestId);
  }

//////////////////////////////////////////////////////////////////////
//   B O A R D   S Q U A R E   U T I L s
function convIndexToX(i)
  {                                                                 //  IMPLEMENTATION-SPECIFIC piece fudge
    return ((i % BOARD_SQ_WIDTH) * SQ_OFFSET) - 80 - SQ_OFFSET;
  }

function convIndexToY(i)
  {                                                                 //  IMPLEMENTATION-SPECIFIC piece fudge
    return (((i - (i % BOARD_SQ_WIDTH)) / BOARD_SQ_WIDTH) * SQ_OFFSET) - 80 - SQ_OFFSET;
  }

//////////////////////////////////////////////////////////////////////
//   B O A R D   S Q U A R E   M A T E R I A L S
function normalSq(i)
  {
    i = typeof i !== 'undefined' ? i : _NOTHING;
    var ctr, j;

    if(i == _NOTHING)
      {
        ctr = _A1;
        j = _J10;
      }
    else
      {
        ctr = i;
        j = i;
      }

    for(; ctr <= j; ctr++)
      {
        boardSquares[ctr].material = normalMaterials[ctr];
      }
  }

function targetedSq(i)
  {
    i = typeof i !== 'undefined' ? i : _NOTHING;
    var ctr, j;

    if(i == _NOTHING)
      {
        ctr = _A1;
        j = _J10;
      }
    else
      {
        ctr = i;
        j = i;
      }

    for(; ctr <= j; ctr++)
      {
        boardSquares[ctr].material = targetedMaterials[ctr];
      }
  }

function liveWhiteSq(i)
  {
    i = typeof i !== 'undefined' ? i : _NOTHING;
    var ctr, j;

    if(i == _NOTHING)
      {
        ctr = _A1;
        j = _J10;
      }
    else
      {
        ctr = i;
        j = i;
      }

    for(; ctr <= j; ctr++)
      {
        boardSquares[ctr].material = whiteLiveMaterials[ctr];
      }
  }

function liveBlackSq(i)
  {
    i = typeof i !== 'undefined' ? i : _NOTHING;
    var ctr, j;

    if(i == _NOTHING)
      {
        ctr = _A1;
        j = _J10;
      }
    else
      {
        ctr = i;
        j = i;
      }

    for(; ctr <= j; ctr++)
      {
        boardSquares[ctr].material = blackLiveMaterials[ctr];
      }
  }

//////////////////////////////////////////////////////////////////////
//   M O U S E - M O V E
function onMouseMove(e)
  {
    var vector = new THREE.Vector3((e.clientX / screenWidth) * 2 - 1,
                                 - (e.clientY / screenHeight) * 2 + 1,
                                    1);
    vector.unproject(camera);

    var ray = new THREE.Raycaster(camera.position, vector.sub(camera.position).normalize());
    var intersects = ray.intersectObjects(boardSquares);

    over(intersects);
  }

function over(intersects)
  {
    var i, firsthit;

    if(!gameOver && MasterControl && HumansTurn && !panelOpen)
      {
        if(intersects.length > 0 && MasterControl && HumansTurn && !panelOpen)
          firsthit = intersects[0].object.name;

        for(i = _A1; i < _NOTHING; i++)
          {
            if(gameEngine.instance.exports.isEmpty_client(i))
              normalSq(i);
          }

        if(gameEngine.instance.exports.isEmpty_client(firsthit) && Select_A == _NOTHING && validTarget(firsthit))
          targetedSq(firsthit)
      }
  }

//////////////////////////////////////////////////////////////////////
//   C L I C K S
function validTarget(j)
  {
    var i = 0;
    while(i < Options.length && Options[i] != j)
      i++;
    if(i < Options.length)
      return true;
    return false;
  }

function onClick(e)
  {
    if(!gameOver)
      {
        var vector = new THREE.Vector3((e.clientX / screenWidth) * 2 - 1,
                                     - (e.clientY / screenHeight) * 2 + 1,
                                       1);
        vector.unproject(camera);

        var ray = new THREE.Raycaster(camera.position, vector.sub(camera.position).normalize());
        var intersects = ray.intersectObjects(boardSquares);

        selection(intersects);
      }
  }

function onTouch(e)
  {
    if(!gameOver && e.touches.length == 1)
      {
        var vector = new THREE.Vector3((e.touches[0].pageX / screenWidth) * 2 - 1,
                                     - (e.touches[0].pageY / screenHeight) * 2 + 1,
                                       1);
        vector.unproject(camera);

        var ray = new THREE.Raycaster(camera.position, vector.sub(camera.position).normalize());
        var intersects = ray.intersectObjects(boardSquares);

        selection(intersects);
      }
  }

function selection(intersects)
  {
    var firsthit;
    if(intersects.length > 0 && MasterControl && HumansTurn && !panelOpen)
      {
        firsthit = intersects[0].object.name;                       //  'name' of first square hit by the ray.

        if(CurrentTurn == 'White')                                  //  WHITE
          {
            if(Select_A == _NOTHING && validTarget(firsthit) && gameEngine.instance.exports.isEmpty_client(firsthit))
              {
                MasterControl = false;                              //  Disable control right away.
                HumansTurn = false;

                if(!gameStarted)                                    //  Officially start the game.
                  pullGUIComponents();

                Select_A = firsthit;                                //  Set Select_A.
                Options = [];                                       //  Empty array.
                normalSq(Select_A);                                 //  Reset all square colors.
                                                                    //  Assemble the animation command.
                animationInstruction = {a:Select_A, action:'place-white-stone'};
                animate();
              }
          }
        else                                                        //  BLACK
          {
            if(Select_A == _NOTHING && validTarget(firsthit) && gameEngine.instance.exports.isEmpty_client(firsthit))
              {
                MasterControl = false;                              //  Disable control right away.
                HumansTurn = false;

                if(!gameStarted)                                    //  Officially start the game.
                  pullGUIComponents();

                Select_A = firsthit;                                //  Set Select_A.
                Options = [];                                       //  Empty array.
                normalSq(Select_A);                                 //  Reset all square colors.
                                                                    //  Assemble the animation command.
                animationInstruction = {a:Select_A, action:'place-black-stone'};
                animate();
              }
          }
      }
  }

function getMoves()
  {
    var len = gameEngine.instance.exports.getMoves_client();
    var i;

    Options = [];

    for(i = 0; i < len; i++)
      Options.push( gameOutputBuffer[i] );

    return;
  }

//////////////////////////////////////////////////////////////////////
//   S C R E E N   E V E N T S
function onWindowResize(e)
  {
    screenWidth = window.innerWidth;
    screenHeight = window.innerHeight;

    ASPECT = screenWidth / screenHeight;
    camera.aspect = ASPECT;
    camera.updateProjectionMatrix();
    renderer.setSize(screenWidth, screenHeight);

    CAMERA_Z = PREFERRED_M * Math.min(screenWidth, screenHeight) + PREFERRED_C;
    resetCameraPositionAngle(angle);                                //  Set this to itself just so that it can redraw
  }

function toggleFullscreen(b)
  {
    if(b)
      THREEx.FullScreen.request();
    else
      THREEx.FullScreen.cancel();
  }

//////////////////////////////////////////////////////////////////////
//   C A M E R A   M O V E M E N T
function resetCameraPositionAngle(a)
  {
    angle = a;

    camera.position.z = Math.cos(-a * (Math.PI / 180)) * CAMERA_Z;
    camera.position.y = CAMERA_Y + Math.sin(-a * (Math.PI / 180)) * CAMERA_Z;
    camera.rotation.x = a * (Math.PI / 180);
  }
