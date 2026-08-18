#  Build a Docker image that can compile C and C++ into WebAssembly.
#          sudo docker build -t c-wasm .

FROM emscripten/emsdk

WORKDIR /home
RUN git clone --recursive https://github.com/WebAssembly/wabt
WORKDIR /home/wabt
RUN git submodule update --init
RUN mkdir build
WORKDIR /home/wabt/build
RUN cmake ..
RUN cmake --build .
RUN apt-get update && apt-get install -y libeigen3-dev && rm -rf /var/lib/apt/lists/*
WORKDIR /home
RUN mkdir src
WORKDIR /home/src
