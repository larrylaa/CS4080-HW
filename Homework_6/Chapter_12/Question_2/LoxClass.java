// LARRY LA - CS 4080 - HW 6

/* 
Ch.12 Q2: Added getter method support - Handle static getters
- Updated get() to accept Interpreter parameter (line 26)
- Added ArrayList import for empty argument list (line 3)
- Auto-invoke static getters when accessed (lines 30-35)

Example:
  class Math {
    class pi { return 3.14159; }  // static getter
  }
  print Math.pi;  // Output: 3.14159 (automatically called)
*/

package com.craftinginterpreters.lox;

import java.util.ArrayList;
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
  Object get(Token name, Interpreter interpreter) {
    // First check for static methods (stored as fields)
    if (fields.containsKey(name.lexeme)) {
      Object value = fields.get(name.lexeme);
      // If it's a static getter, invoke it automatically
      if (value instanceof LoxFunction) {
        LoxFunction function = (LoxFunction) value;
        if (function.isGetter()) {
          return function.call(interpreter, new ArrayList<>());
        }
      }
      return value;
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
