// LARRY LA - CS 4080 - HW 4

/* 
The code below for Ch.8 challenge Question 2 (see Lines 9-10, 18-24, 43-47)

Maybe you want Lox to be a little more explicit about variable initialization. Instead of implicitly 
initializing variables to nil, make it a runtime error to access a variable that has not been 
initialized or assigned to.

SOLUTION:
- Lines 9-10: Added UNINITIALIZED sentinel object to mark uninitialized variables
- Lines 18-24: Modified get() method to check for UNINITIALIZED and throw runtime error
- Lines 43-47: Added defineUninitialized() method for declaring variables without initialization

EXAMPLE INPUT/OUTPUT:
var a;
var b;
a = "assigned";
print a;  // Output: assigned
print b;  // Runtime Error: Variable 'b' is used before being initialized.

NOTE: Running from here won't work, if you would like to run the code, please use the files in the book directory.
*/

package com.craftinginterpreters.lox;

import java.util.HashMap;
import java.util.Map;

class Environment {
  final Environment enclosing;
  private final Map<String, Object> values = new HashMap<>();
  
  // Sentinel object to mark uninitialized variables
  private static final Object UNINITIALIZED = new Object();

  Environment() {
    enclosing = null;
  }

  Environment(Environment enclosing) {
    this.enclosing = enclosing;
  }

  Object get(Token name) {
    if (values.containsKey(name.lexeme)) {
      Object value = values.get(name.lexeme);
      if (value == UNINITIALIZED) {
        throw new RuntimeError(name, 
            "Variable '" + name.lexeme + "' is used before being initialized.");
      }
      return value;
    }

    if (enclosing != null) return enclosing.get(name);

    throw new RuntimeError(name, 
        "Undefined variable '" + name.lexeme + "'.");
  }

  void assign(Token name, Object value) {
    if (values.containsKey(name.lexeme)) {
      values.put(name.lexeme, value);
      return;
    }

    if (enclosing != null) {
      enclosing.assign(name, value);
      return;
    }

    throw new RuntimeError(name, 
        "Undefined variable '" + name.lexeme + "'.");
  }

  void define(String name, Object value) {
    values.put(name, value);
  }
  
  void defineUninitialized(String name) {
    values.put(name, UNINITIALIZED);
  }
}