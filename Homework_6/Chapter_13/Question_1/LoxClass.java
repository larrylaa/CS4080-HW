// LARRY LA - CS 4080 - HW 6

/* 
Ch.13 Q1: Added multiple inheritance support - Store multiple superclasses and search left-to-right
- Changed superclass to List<LoxClass> superclasses (line 9)
- Modified findMethod() to iterate through superclasses list (lines 21-28)
- Method resolution: check class first, then each superclass left-to-right
- First match wins (simple MRO)

Example:
  class Cat < Animal, Mammal, Pet {
    meow() { print "meow"; }
  }
  var cat = Cat();
  cat.meow();  // Methods resolved: Cat -> Animal -> Mammal -> Pet
*/

package com.craftinginterpreters.lox;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

class LoxClass extends LoxInstance implements LoxCallable {
  final String name;
  final List<LoxClass> superclasses;
  private final Map<String, LoxFunction> methods;

  LoxClass(String name, List<LoxClass> superclasses,
           Map<String, LoxFunction> methods) {
    super(null);
    this.superclasses = superclasses;
    this.name = name;
    this.methods = methods;
    this.klass = this;  // LoxClass is its own metaclass
  }

  LoxFunction findMethod(String name) {
    if (methods.containsKey(name)) {
      return methods.get(name);
    }

    // Search through superclasses in left-to-right order
    for (LoxClass superclass : superclasses) {
      LoxFunction method = superclass.findMethod(name);
      if (method != null) {
        return method;
      }
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
