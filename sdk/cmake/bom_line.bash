#!/bin/bash
BOM_DIR=$1
echo BOM_DIR $BOM_DIR
TGT_BOM=$2
echo TGT_BOM $TGT_BOM
INST_DIR=$3
echo INST_DIR $INST_DIR
BRK_TOKEN=$4
echo BRK_TOKEN $BRK_TOKEN
TGT_FILE=$5
echo TGT_FILE $TGT_FILE

[ "$INST_DIR" = "NULL" ] && INST_DIR=""


[[ -d ${BOM_DIR} ]] || mkdir ${BOM_DIR}
echo ALL $@

STG1=${TGT_FILE##*/${BRK_TOKEN}/}
STRIPPED_NAME=${STG1#$BRK_TOKEN/}
echo STG1= ${STG1} STRIPPED_NAME ${STRIPPED_NAME}

if [[ -L "$TGT_FILE" ]]; then
  echo "<N/A>"$':'"INS"${INST_DIR}/${STRIPPED_NAME}$':'CKSUM$':'ONE$'::'INT$':'755$':'$(readlink $TGT_FILE) >> ${TGT_BOM}
else
  echo "DEL"/${STRIPPED_NAME}$':'"INS"${INST_DIR}/${STRIPPED_NAME}$':'CKSUM$':'ONE$':'$':'INT$':'$(stat -c '%a' ${TGT_FILE}) >> ${TGT_BOM}
fi
