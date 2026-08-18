# [Freedom](https://www.ericjoycefilm.com/wastesoftime/boardgames/freedom/index.php?lang=en)

Notes on the creation of Freedom

## Docker container to compile C to WebAssembly
We wish to have means to compile C code into WebAssembly modules that handle game-compute on the front-end. Since this is a specialized, project-specific use-case, I do not want to modify my system's usual toolchains. 

Therefore, create a Docker container to compile WebAssembly modules. Leave the rest of my system alone.
```
sudo docker build -t c-wasm .
```

Confirm the container's existence.
```
sudo docker images
```

When you choose to, kill the container.
```
sudo docker image rm c-wasm
```

## Citation
If this code was helpful to you, please cite this repository.

```
@misc{freedom,
  title={Freedom in C},
  author={Eric C. Joyce},
  year={2026},
  publisher={Github},
  journal={GitHub repository},
  howpublished={\url{https://github.com/EricCJoyce/Freedom}}
}
```
