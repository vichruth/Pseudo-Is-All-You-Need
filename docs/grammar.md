# Pseudo Language Grammar Specification

## Overall Structure
```
program             = statement-list ;
statement-list      = statement { statement } ;
statement           = assignment-statement
                    | if-statement
                    | while-statement
                    | for-statement
                    | repeat-until-statement
                    | function-statement
                    | return-statement
                    | block-statement
                    | read-statement
                    | write-statement ;
```

### Control Flow Constructs

#### IF/THEN/ELSE Statement
```
if-statement        = "IF" condition "THEN" statement-list
                    [ "ELSE" statement-list ]
                    "ENDIF" ;
condition           = logical-expression ;
logical-expression  = logical-term { ("OR") logical-term } ;
logical-term        = logical-factor { ("AND") logical-factor } ;
logical-factor      = "NOT" logical-factor
                    | "(" logical-expression ")"
                    | expression ;
expression          = simple-expression [ ("=" | "<>" | "<" | ">" | "<=" | ">=") simple-expression ] ;
simple-expression   = term { ("+" | "-") term } ;
term                = factor { ("*" | "/" | "%") factor } ;
factor              = "(" logical-expression ")"
                    | identifier
                    | array-access
                    | number
                    | string-literal
                    | "NOT" factor
                    | "TRUE"
                    | "FALSE" ;
```

#### WHILE Statement
```
while-statement     = "WHILE" condition "DO" statement-list "ENDWHILE" ;
```

#### FOR Statement
```
for-statement       = "FOR" identifier "=" expression "TO" expression
                    [ "BY" expression ]
                    "DO" statement-list "ENDFOR" ;
```

#### REPEAT/UNTIL Statement
```
repeat-until-statement = "REPEAT" statement-list "UNTIL" condition "ENDREPEAT" ;
```

### Functions
```
function-statement  = "FUNCTION" identifier [ "(" parameter-list ")" ]
                    "BEGIN" statement-list "ENDFUNCTION" ;
parameter-list      = identifier { "," identifier } ;
return-statement    = "RETURN" [ expression ] ;
```

### Arrays
```
array-access        = identifier "[" expression { "," expression } "]" ;
array-declaration   = "ARRAY" identifier "[" expression { "," expression } "]" [ "OF" type-name ] ;
type-name           = "INTEGER" | "REAL" | "BOOLEAN" | "CHARACTER" | "STRING" ;
```

### Input/Output Statements
```
read-statement      = "READ" identifier { "," identifier } ;
write-statement     = "WRITE" expression { "," expression }
                    | "PRINT" expression { "," expression } ;
```

### Comments
```
comment             = "/*" { character } "*/"
                    | "//" { character } newline ;
```

### Additional Elements
```
block-statement     = "BEGIN" statement-list "END" ;
assignment-statement = identifier "=" expression ;
identifier          = letter { letter | digit } ;
number              = digit { digit } [ "." digit { digit } ] ;
string-literal      = '"' { character } '"' ;
newline             = "\n" ;
character           = any-character-except-newline ;
```

### Enhanced Features
The grammar now includes:
- **Logical operators** (AND, OR, NOT) in conditions
- **READ statement** for input operations  
- **WRITE/PRINT statements** for output operations
- **Comment syntax** with both /* */ and // styles
- **Element type specification** in array declarations (OF INTEGER, OF REAL, etc.)
- **Proper precedence** for logical operators (NOT > AND > OR)
- **Extended expression grammar** to support all standard operations

This enhanced grammar provides a complete foundation for the pseudocode compiler with all the features specified in your requirements.