# Uxnbruteforce

A small C program to optimize little pieces of [uxntal](https://wiki.xxiivv.com/site/uxntal.html) code.
It is a work in progress, a little bit messy for the moment :)

I had the idea to do this because I remembered seeing this concerning the Joy programming language :
[A Brute-force Automated Construction Finder](http://tunes.org/~iepos/joy.html#finder)
And I thought it would be cool to do the same for uxn.

# How to use it ?

```
git clone https://github.com/max22-/uxnbruteforce
cd uxnbruteforce
make run
```
The current example solves %MIN2 (finds the minimum of 2 shorts). The first solution is not that correct (it generates a correct answer, but jumps in the middle of nowhere...). This one looks better : LTH2k JMP SWP2 POP2

To solve a different problem, you need to edit
- TESTS : number of tests
- inputs[TESTS][] : input data
- init_tests() : fills outputs[TESTS]
- check() --> loads inputs, runs uxn_eval, and checks working stack is equal to the expected output, etc
- check_instructions() : if you want to discard some opcodes from the search space

A quick and dirty hack is made in uxn-fast.c, to avoid infinite loops. (the main loop in uxn_eval is limited to 100 iterations).

# TODO / Ideas

- Find a better way to load the data (inputs and expected outputs). It is quite painful now with the actual implementation !
- Use other techniques like genetic programming ? (suggestion of Sigrid)

# Bugs

- LIT won't be well decoded in the disassembler output
- There is a trailing "1" opcode in the generated programs, but it is not a big deal since there are some BRK before.