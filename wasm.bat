@echo off
emcc -o out/index.html -Ilib/wasm -Ilib -Llib/wasm^
	-lraylib chemio.c -Os -Wall^
	--preload-file res^
	-s USE_GLFW=3 -s ASYNCIFY --shell-file lib/wasm/shell.html -DPLATFORM_WEB

