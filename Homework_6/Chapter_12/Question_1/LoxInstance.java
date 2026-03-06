// LARRY LA - CS 4080 - HW 6

/* 
Ch.12 Q1: Added static method support - Made fields accessible to LoxClass
- Changed klass and fields to protected (lines 7-8)
- Allows LoxClass subclass to access these fields for metaclass support

Example:
  class Math {
    class square(n) { return n * n; }  // static method
  }
  print Math.square(3);  // Output: 9
*/

package com.craftinginterpreters.lox;

import java.util.HashMap;
import java.util.Map;

class LoxInstance {
  protected LoxClass klass;
  protected final Map<String, Object> fields = new HashMap<>();

  LoxInstance(LoxClass klass) {
    this.klass = klass;
  }

  Object get(Token name) {
    if (fields.containsKey(name.lexeme)) {
      return fields.get(name.lexeme);
    }

    LoxFunction method = klass.findMethod(name.lexeme);
    if (method != null) return method.bind(this);

    throw new RuntimeError(name, 
        "Undefined property '" + name.lexeme + "'.");
  }

  void set(Token name, Object value) {
    fields.put(name.lexeme, value);
  }

  @Override
  public String toString() {
    return klass.name + " instance";
  }
}
