@echo off

clang Yaml.c -o Yaml.exe -D _CRT_SECURE_NO_WARNINGS
Yaml.exe
echo > ExitCode: %ErrorLevel%