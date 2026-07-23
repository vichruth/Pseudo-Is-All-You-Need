# Grammar

## Overview

This document outlines the grammar rules for the pseudocode compiler.

## Syntax Rules

1. **IF-THEN-ELSE Statements**
   ```plaintext
   if (condition) then
       statement_list
   [else
       statement_list]
   endif
   ```

2. **FOR Loops**
   ```plaintext
   for variable = start to end do
       statement_list
   endfor
   ```

3. **WHILE Loops**
   ```plaintext
   while (condition) do
       statement_list
   endwhile
   ```

4. **REPEAT-UNTIL Loops**
   ```plaintext
   repeat
       statement_list
   until (condition)
   ```

5. **FUNCTION Declarations**
   ```plaintext
   function name(parameter_list)
       statement_list
   endfunction
   ```

6. **Arrays**
   ```plaintext
   array_name[index] = value
   ```
   ```plaintext
   index = array_name.length - 1
   ```

## Semantics Rules

- Conditions must evaluate to boolean values.
- Loop variables must be integers.
- Arrays must have valid indices and values.
- Function calls must pass the correct number of arguments.