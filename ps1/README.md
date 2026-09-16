# Butterscotch PS1

This is the real PS1 platform experiment for Butterscotch.

The first hardware milestone is deliberately small: boot a PS1 executable, initialize the CD-ROM/GPU, find the original DATA.WIN, and read only its first sector. The complete file is never loaded into the 2 MB main RAM.

This is not a replacement runtime or a fake Deltarune build. It is the first piece of the PS1 platform layer needed before wiring the normal Butterscotch parser/Runner into the PS1 memory and CD I/O model.

Target path:

DATA.WIN (original Chapter 1)
-> PS1 streaming DataWin I/O
-> normal Butterscotch DataWin parser
-> Runner
-> PS1 renderer/input/audio
-> Deltarune Chapter 1
