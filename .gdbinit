define do_load
	monitor reset init
	monitor halt

	echo --> Loading Application: load bin/demo.elf\n
	load bin/demo.elf

	echo --> Loading Symboltable: file bin/demo.elf\n
	file bin/demo.elf
end
document do_load
	Download der Applikation
	Usage: do_load
end


echo --> Connecting to Remote-GDB (openOCD)\n
target remote localhost:3333

do_load

echo --> Befehle fuer Programmausfuehrung\n
echo -->   do_load   -> Applikation auf NXT downloaden\n
echo -->   break / info br / delete nr / disable nr\n
echo -->   continue/next/step/CTRL-C\n
echo -->   list/list -\n
echo --> Befehle fuer Variablen\n
echo -->    bt/up/down       Stackframe darstellen\n
echo -->    info locals/variable\n
echo -->    print variable\n
echo -->    set var=xyz\n
echo --> Befehle für Speicher\n
echo -->    x addr\n
echo -->    set *(int *)adr)=xyz\n
echo -->    info register\n

