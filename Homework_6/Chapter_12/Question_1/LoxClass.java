// LARRY LA - CS 4080 - HW 6

/* 
Ch.12 Q1: Added static method support - Metaclass implementation
- LoxClass extends LoxInstance (line 6) - metaclass approach
- LoxClass is its own metaclass (line 13)
- Override get() to look up static methods from fields (line 25)
- Static methods stored as fields on the class object

Example:
  class Math {
    class square(n) { return n * n; }  // static method
  }
  print Math.square(3);  // Output: 9
*/

package com.craftinginterpreters.lox;

import java.util.List;
import java.util.Map;

class LoxClass extends LoxInstance implements LoxCallable {
  final String name;
  private final Map<String, LoxFunction> methods;

  LoxClass(String name, Map<String, LoxFunction> methods) {
    super(null);
    this.name = name;
    this.methods = methods;
    this.klass = this;  // LoxClass is its own metaclass
  }

  LoxFunction findMethod(String name) {
    if (methods.containsKey(name)) {
      return methods.get(name);
    }

    return null;
  }

  @Override
  Object get(Token name) {
    // First check for static methods (stored as fields)
    if (fields.containsKey(name.lexeme)) {
      return fields.get(name.lexeme);
    }

    throw new RuntimeError(name, 
        "Undefined property '" + name.lexeme + "'.");
  }

  @Override
  public String toString() {
    return name;
  }

  @Override
  public Object call(Interpreter interpreter,
                     List<Object> arguments) {
    LoxInstance instance = new LoxInstance(this);
    LoxFunction initializer = findMethod("init");
    if (initializer != null) {
      initializer.bind(instance).call(interpreter, arguments);
    }

    return instance;
  }

  @Override
  public int arity() {
    LoxFunction initializer = findMethod("init");
    if (initializer == null) return 0;
    return initializer.arity();
  }
}
