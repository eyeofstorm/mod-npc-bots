#!/usr/bin/env bash

## GETS THE CURRENT MODULE ROOT DIRECTORY
MOD_NPC_BOTS_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/" && pwd )"

source $MOD_NPC_BOTS_ROOT"/conf/conf.sh.dist"

if [ -f $MOD_NPC_BOTS_ROOT"/conf/conf.sh" ]; then
    source $MOD_NPC_BOTS_ROOT"/conf/conf.sh"
fi
