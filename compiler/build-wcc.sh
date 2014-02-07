# wcc: wannabe c compiler command for unixlikes running x86

if [ $(uname | grep MINGW) ];
then

echo no WCC for mingw sorry

else

c99 -o wcc -DWCC codegen_x86.c diagnostics.c general.c main.c optimize.c parser.c tokenizer.c tokens.c tree.c typeof.c escapes.c

fi