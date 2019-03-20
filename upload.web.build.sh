#!/bin/bash

SRC_DIR=.
DEST_DIR=/home/on-three/public_html/midi-player

rsync -avz ${SRC_DIR}/index.html on-three@${DEV_VPS}:${DEST_DIR}
rsync -avz ${SRC_DIR}/index.js on-three@${DEV_VPS}:${DEST_DIR}
rsync -avz ${SRC_DIR}/index.wasm on-three@${DEV_VPS}:${DEST_DIR}
rsync -avz ${SRC_DIR}/index.data on-three@${DEV_VPS}:${DEST_DIR}
#rsync -avz ${SRC_DIR}/assets on-three@${DEV_VPS}:${DEST_DIR}
