# ThunderHackathon-JS-Interpreter
A JavaScript Interpreter built in C++ supporting variables, arrays, objects, functions, loops and higher-order functions.

# ThunderHackathon JS Interpreter

A JavaScript Interpreter built in C++ for Thunder Hackathon 2.0.

## Features

### Variables
- let
- const
- var

### Data Types
- number
- string
- boolean
- undefined
- null

### Arrays
- push()
- pop()
- shift()
- unshift()
- concat()
- includes()
- indexOf()
- reverse()
- slice()
- splice()
- sort()
- flat()
- flatMap()

### Strings
- trim()
- split()
- replace()
- replaceAll()
- toUpperCase()
- toLowerCase()
- includes()
- startsWith()
- endsWith()
- substring()
- slice()
- indexOf()

### Objects
- Object literals
- Property access
- Property assignment

### Functions
- Function declarations
- Function expressions
- Arrow functions
- Closures
- Rest parameters
- Spread operator

### Higher Order Functions
- map()
- filter()
- reduce()
- find()
- some()
- every()
- forEach()

### Control Flow
- if / else
- for loops
- while loops
- break
- continue

### Math Object
- Math.random()
- Math.floor()
- Math.ceil()
- Math.round()
- Math.abs()
- Math.max()

---

# Compilation

Using g++:

```bash
g++ JsInterpreterVersion8.cpp -o interpreter
```

---

# Running

Windows:

```bash
interpreter.exe
```

Linux:

```bash
./interpreter
```

---

# Input Method

After running the interpreter, type JavaScript code.

Example:

```javascript
let arr = [1,2,3,4,5];

let even =
    arr.filter(
        x => x % 2 == 0
    );

console.log(even);
```

When finished entering code:

Press

CTRL + Z

and then

ENTER

to execute the program.

---

# Example

Input

```javascript
let nums = [1,2,3,4,5];

let result =
    nums.map(
        x => x * x
    );

console.log(result.join(", "));
```

Output

```text
1, 4, 9, 16, 25
```

---

# Author

Vivek Pant

Thunder Hackathon 2.0 Submission
