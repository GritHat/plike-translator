# P-Like Language Documentation

## Table of Contents

1. [Language Overview](#language-overview)
2. [Lexical Elements](#lexical-elements)
3. [Data Types](#data-types)
4. [Variables and Constants](#variables-and-constants)
5. [Arrays](#arrays)
6. [Procedures and Functions](#procedures-and-functions)
7. [Control Structures](#control-structures)
8. [Expressions and Operators](#expressions-and-operators)
9. [Input/Output](#input-output)
10. [Configuration Options](#configuration-options)

## Language Overview

The P-Like language is a Pascal-inspired programming language that compiles to C. It features static typing, explicit scope boundaries, and support for procedures and functions with various parameter passing modes.

## Lexical Elements

### Keywords
```
function    procedure   var         begin       end
if         then        else        elseif      endif
while      do          endwhile    for         to
step       endfor      repeat      until       return
in         out         inout       print       read
array      of          
```

### Types
```
integer    real    logical    character
```

### Operators
```
:=   =    +    -    *    /    %    
>    <    >=   <=   ==   !=   
and  or   not  &    |    ^    <<   >>
```

## Data Types

### Basic Types
```pascal
var i: integer;    // Whole numbers
var r: real;       // Floating point numbers
var b: logical;    // Boolean values (true/false)
var c: character;  // Single characters
```

### Type Modifiers
```pascal
// Pointer types
var ptr: *integer;
var ptr_arr: array[10] of *real;
```

## Variables and Constants

### Variable Declaration
```pascal
var x, y: integer;
var radius: real;
var done: logical;

// Multiple variables of the same type
var a, b, c: real;
```

## Arrays

### Fixed-Size Arrays
```pascal
var arr: array[10] of integer;        // Single dimension
var matrix: array[3,3] of real;       // Multi-dimensional
```

### Range-Based Arrays
```pascal
var items: array[1..100] of integer;
var grid: array[0..9, 0..9] of real;
```

### Dynamic Arrays
```pascal
var dynamic: array[n] of integer;     // Size determined at runtime
var matrix: array[rows,cols] of real; // Multi-dimensional dynamic
```

### Array Access
```pascal
// Using square brackets (default)
arr[0] := 42;
matrix[i,j] := 3.14;

// Using parentheses (when mixed access is enabled)
arr(0) := 42;
matrix(i,j) := 3.14;
```

## Procedures and Functions

### Function Declaration
```pascal
function max(in a: integer, in b: integer): integer
begin
    if a > b then
        max := a;
    else
        max := b;
    endif
end max
```

### Procedure Declaration
```pascal
procedure swap(inout x: integer, inout y: integer)
var temp: integer;
begin
    temp := x;
    x := y;
    y := temp;
end swap
```

### Parameter Passing Modes

#### In Parameters (Pass by Value)
```pascal
procedure print_value(in x: integer)
begin
    print(x);
end print_value
```

#### Out Parameters (Write-Only Reference)
```pascal
procedure get_dimensions(out width: integer, out height: integer)
begin
    read(width);
    read(height);
end get_dimensions
```

#### Inout Parameters (Read-Write Reference)
```pascal
procedure increment(inout x: integer)
begin
    x := x + 1;
end increment
```

## Control Structures

### If Statement
```pascal
if condition then
    // statements
elseif other_condition then
    // statements
else
    // statements
endif
```

### While Loop
```pascal
while condition do
    // statements
endwhile
```

### For Loop
```pascal
for i := 1 to 10 do
    // statements
endfor

// With step
for i := 10 to 0 step -2 do
    // statements
endfor
```

### Repeat Until
```pascal
repeat
    // statements
until condition
```

## Expressions and Operators

### Arithmetic Operators
```pascal
a + b    // Addition
a - b    // Subtraction
a * b    // Multiplication
a / b    // Division
a % b    // Modulo
```

### Comparison Operators
```pascal
a == b   // Equal
a != b   // Not equal
a < b    // Less than
a <= b   // Less than or equal
a > b    // Greater than
a >= b   // Greater than or equal
```

### Logical Operators
```pascal
// Standard style
x and y
x or y
not x

// Dotted style (when enabled)
x .and. y
x .or. y
.not. x
```

### Bitwise Operators
```pascal
a & b    // Bitwise AND
a | b    // Bitwise OR
a ^ b    // Bitwise XOR
~a       // Bitwise NOT
a << b   // Left shift
a >> b   // Right shift
```

## Input/Output

### Print Statement
```pascal
print(x);                    // Print variable
print("Hello, World!");      // Print string
```

### Read Statement
```pascal
read(x);                     // Read into variable
```

## Configuration Options

### Assignment Style
```pascal
// Colon-equals style
x := 42;

// Equals style (when configured)
x = 42;
```

### Array Indexing
```pascal
// 0-based indexing
arr[0] := 42;

// 1-based indexing (when configured)
arr[1] := 42;
```

### Parameter Style
```pascal
// Type in declaration
function f(x: integer, y: real)

// Type after declaration (when configured)
function f(x,y)
    x: integer;
    y: real;
```

### Bounds Checking
```pascal
// When enabled, generates runtime checks:
arr[i] := 42;  // Generates bounds check for i
```

### Mixed Array Access
```pascal
// When enabled, allows both styles:
arr[i] := 42;  // Using brackets
arr(i) := 42;  // Using parentheses
```
