#!/bin/bash

main() {
  generate-config
  build

  fix-permissions
}

generate-config() {
  cmake -B ./build
}

build() {
  cmake --build ./build
}

fix-permissions() {
  chmod -R 777 build/
}

main
