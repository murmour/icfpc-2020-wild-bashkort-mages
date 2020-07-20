#!/bin/sh

. ./cleanup.sh
cd app
$OCAMLOPT -o main.o -x $OPT -c main.ml -DHTTPLIB -DSUBMISSION
$OCAMLOPT -o bot.o -x $OPT -c bot.ml
$OCAMLOPT main.o bot.o -o app
