#!/usr/bin/env bash

rm -rf /tmp/previousDatebook.pdb \
   /tmp/previousMemos.pdb \
   /tmp/previousTodo.pdb

touch /tmp/datebook.pdb
touch /tmp/memo.pdb
touch /tmp/todo.pdb

./helper_save_pdbs_test

if [ ! -f /tmp/previousDatebook.pdb ]; then
    echo "/tmp/datebook.pdb not saved as /tmp/previousDatebook.pdb"
    exit 1
fi
if [ ! -f /tmp/previousMemos.pdb ]; then
    echo "/tmp/memo.pdb not saved as /tmp/previousMemos.pdb"
    exit 1
fi
if [ ! -f /tmp/previousTodo.pdb ]; then
    echo "/tmp/todo.pdb not saved as /tmp/previousTodo.pdb"
fi

rm -rf /tmp/previousDatebook.pdb \
   /tmp/previousMemos.pdb \
   /tmp/previousTodo.pdb \
   /tmp/datebook.pdb \
   /tmp/memo.pdb \
   /tmp/todo.pdb
