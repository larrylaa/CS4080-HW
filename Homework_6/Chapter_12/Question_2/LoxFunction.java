// LARRY LA - CS 4080 - HW 6

/* 
Ch.12 Q2: Added getter method support - Expose isGetter flag from declaration
- Added isGetter() method (line 74) to check if function is a getter
- Returns declaration.isGetter, or false for anonymous functions

Example:
  class Circle {
    area { return 3.14159 * this.radius * this.radius; }  // getter
  }
  print circle.area;  // Output: 50.2655
*/

package com.craftinginterpreters.lox;

import java.util.List;

class LoxFunction implements LoxCallable {
  private final Stmt.Function declaration;
  private final Environment closure;
  private final boolean isInitializer;
  private final String name;
  private final List<Token> params;
  private final List<Stmt> body;

  LoxFunction(Stmt.Function declaration, Environment closure,
              boolean isInitializer) {
    this.declaration = declaration;
    this.closure = closure;
    this.isInitializer = isInitializer;
    this.name = declaration.name.lexeme;
    this.params = declaration.params;
    this.body = declaration.body;
  }

  LoxFunction(List<Token> params, List<Stmt> body, Environment closure) {
    this.declaration = null;
    this.closure = closure;
    this.isInitializer = false;
    this.name = null;
    this.params = params;
    this.body = body;
  }

  @Override
  public String toString() {
    if (name != null) {
      return "<fn " + name + ">";
    } else {
      return "<fn anonymous>";
    }
  }

  @Override
  public int arity() {
    return params.size();
  }

  @Override
  public Object call(Interpreter interpreter, List<Object> arguments) {
    Environment environment = new Environment(closure);
    for (int i = 0; i < params.size(); i++) {
      environment.define(params.get(i).lexeme,
          arguments.get(i));
    }

    try {
      interpreter.executeBlock(body, environment);
    } catch (Return returnValue) {
      if (isInitializer) return closure.getAt(0, "this");

      return returnValue.value;
    }

    if (isInitializer) return closure.getAt(0, "this");
    return null;
  }

  LoxFunction bind(LoxInstance instance) {
    Environment environment = new Environment(closure);
    environment.define("this", instance);
    return new LoxFunction(declaration, environment,
                           isInitializer);
  }

  boolean isGetter() {
    return declaration != null && declaration.isGetter;
  }
}