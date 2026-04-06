#!/bin/bash
set -e

ORIGINAL="test_data/sample.txt"
RECIPE="test_recipe"
RESTORED="test_data/restored.txt"

mkdir -p test_data

echo "--- Generando archivo de prueba ---"
python3 -c "print('Linea de prueba repetida\n' * 1000)" > $ORIGINAL

echo "--- Haciendo backup ---"
./backup_app backup $ORIGINAL $RECIPE

echo "--- Restaurando ---"
./backup_app restore $RECIPE $RESTORED

echo "--- Verificando integridad ---"
if diff -q $ORIGINAL $RESTORED > /dev/null; then
    echo "PASS: El archivo restaurado es identico al original"
else
    echo "FAIL: Los archivos difieren"
    diff $ORIGINAL $RESTORED | head -20
    exit 1
fi

echo "--- Limpiando ---"
rm -rf test_data repo
