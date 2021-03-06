#!/usr/bin/env python
from subprocess import Popen, PIPE

tests = [('2', '2'),
         ('(* 3 4)', '12'),
         ('(( (let (x 2) (fn () (fn () x))) ))', '2'),
         ('(let (x 4) (+ (+ x x) 1))', '9'),
         ('((let (x 4) (fn (y) (+ (+ x y) 1))) 2)', '7'),
         ('(if true 1 0)', '1'),
         ('(if false 1 0)', '0'),
]

for code, expected in tests:
    p = Popen(["./imp"], stdin=PIPE, stdout=PIPE)
    stdout, stderr = p.communicate(code)
    if expected != stdout.strip():
        print 'Test failure', code
        print 'Expected', expected, ' but got', stdout.replace('\n','\n> ')
