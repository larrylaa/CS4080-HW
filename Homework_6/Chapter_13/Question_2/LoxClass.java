// LARRY LA
// CS 4080
// HW 6 - Chapter 13, Question 2
// This file implements BETA-style method resolution where superclass methods take priority
// over subclass methods (reverse of standard OOP). Added inner keyword to allow superclass
// methods to call down to subclass implementations. Modified findMethod() to check
// superclasses first, added findMethodWithClass() to track declaring class for inner
// resolution, and added findMethodBelowClass() to search from instance class toward
// declaring class for inner.method() calls.

package com.craftinginterpreters.lox;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

class LoxClass extends LoxInstance implements LoxCallable {
  final String name;
  final List<LoxClass> superclasses;
  private final Map<String, LoxFunction> methods;

  // Helper class to return method with its declaring class
  static class MethodWithClass {
    final LoxFunction method;
    final LoxClass declaringClass;
    
    MethodWithClass(LoxFunction method, LoxClass declaringClass) {
      this.method = method;
      this.declaringClass = declaringClass;
    }
  }

  LoxClass(String name, List<LoxClass> superclasses,
           Map<String, LoxFunction> methods) {
    super(null);
    this.superclasses = superclasses;
    this.name = name;
    this.methods = methods;
    this.klass = this;  // LoxClass is its own metaclass
  }

  LoxFunction findMethod(String name) {
    // BETA-style: Check superclasses FIRST (superclass methods win)
    for (LoxClass superclass : superclasses) {
      LoxFunction method = superclass.findMethod(name);
      if (method != null) {
        return method;
      }
    }

    // Then check own methods
    if (methods.containsKey(name)) {
      return methods.get(name);
    }

    return null;
  }

  // Find method and return which class it's declared in
  MethodWithClass findMethodWithClass(String name) {
    // BETA-style: Check superclasses FIRST (superclass methods win)
    for (LoxClass superclass : superclasses) {
      MethodWithClass result = superclass.findMethodWithClass(name);
      if (result != null) {
        return result;
      }
    }

    // Then check own methods
    if (methods.containsKey(name)) {
      return new MethodWithClass(methods.get(name), this);
    }

    return null;
  }

  // Find a method in subclasses for inner() calls
  // Searches from actualClass down to (but not including) declaringClass
  LoxFunction findMethodBelowClass(String name, LoxClass declaringClass, LoxClass actualClass) {
    // If we've reached the declaring class, stop
    if (actualClass == declaringClass) {
      return null;
    }

    // Check if actualClass has the method
    if (actualClass.methods.containsKey(name)) {
      return actualClass.methods.get(name);
    }

    // Otherwise, check actualClass's superclasses
    for (LoxClass superclass : actualClass.superclasses) {
      LoxFunction method = findMethodBelowClass(name, declaringClass, superclass);
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
    MethodWithClass result = findMethodWithClass("init");
    if (result != null) {
      result.method.bind(instance, result.declaringClass).call(interpreter, arguments);
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
