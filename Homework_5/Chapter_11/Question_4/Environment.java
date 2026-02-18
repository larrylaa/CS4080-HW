// LARRY LA - CS 4080 - HW 5

/*
Ch.11 Q4: Added slots ArrayList for index-based local variable storage (line 19)
define() and defineUninitialized() append to slots (lines 64-65, 69-70)
Added getAtIndex() and assignAtIndex() for O(1) slot access (lines 73-78)
*/

package com.craftinginterpreters.lox;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

class Environment {
  final Environment enclosing;
  private final Map<String, Object> values = new HashMap<>();
  private final List<Object> slots = new ArrayList<>();
  
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
    slots.add(value);
  }
  
  void defineUninitialized(String name) {
    values.put(name, UNINITIALIZED);
    slots.add(UNINITIALIZED);
  }

  Object getAtIndex(int distance, int index) {
    return ancestor(distance).slots.get(index);
  }

  void assignAtIndex(int distance, int index, Object value) {
    ancestor(distance).slots.set(index, value);
  }
  
  Object getAt(int distance, String name) {
    return ancestor(distance).values.get(name);
  }

  void assignAt(int distance, Token name, Object value) {
    ancestor(distance).values.put(name.lexeme, value);
  }

  Environment ancestor(int distance) {
    Environment environment = this;
    for (int i = 0; i < distance; i++) {
      environment = environment.enclosing;
    }

    return environment;
  }
}