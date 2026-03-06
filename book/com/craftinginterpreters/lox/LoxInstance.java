package com.craftinginterpreters.lox;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

class LoxInstance {
  protected LoxClass klass;
  protected final Map<String, Object> fields = new HashMap<>();

  LoxInstance(LoxClass klass) {
    this.klass = klass;
  }

  Object get(Token name, Interpreter interpreter) {
    if (fields.containsKey(name.lexeme)) {
      return fields.get(name.lexeme);
    }

    LoxClass.MethodWithClass result = klass.findMethodWithClass(name.lexeme);
    if (result != null) {
      LoxFunction bound = result.method.bind(this, result.declaringClass);
      // If it's a getter, invoke it automatically
      if (bound.isGetter()) {
        return bound.call(interpreter, new ArrayList<>());
      }
      return bound;
    }

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
