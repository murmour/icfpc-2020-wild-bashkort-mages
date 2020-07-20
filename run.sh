#!/bin/sh

cd app
app "$@" || echo "run error code: $?"
