// LARRY LA - CS 4080 - HW 5

/*
Ch.10 Q2: Added constructor for anonymous functions (lines 27-32)
Supports both named and anonymous function objects
*/

package com.craftinginterpreters.lox;

import java.util.List;

class LoxFunction implements LoxCallable {
  private final Stmt.Function declaration;
  private final Environment closure;
  private final String name;
  private final List<Token> params;
  private final List<Stmt> body;

  LoxFunction(Stmt.Function declaration, Environment closure) {
    this.declaration = declaration;
    this.closure = closure;
    this.name = declaration.name.lexeme;
    this.params = declaration.params;
    this.body = declaration.body;
  }

  LoxFunction(List<Token> params, List<Stmt> body, Environment closure) {
    this.declaration = null;
    this.closure = closure;
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
      return returnValue.value;
    }

    return null;
  }
}