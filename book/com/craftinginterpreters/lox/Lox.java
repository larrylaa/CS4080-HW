package com.craftinginterpreters.lox;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

public class Lox {
  private static final Interpreter interpreter = new Interpreter();
  static boolean hadError = false;
  static boolean hadRuntimeError = false;

  public static void main(String[] args) throws IOException {
    if (args.length > 1) {
      System.out.println("Usage: jlox [script]");
      System.exit(64);
    } else if (args.length == 1) {
      runFile(args[0]);
    } else {
      runPrompt();
    }
  }

  private static void runFile(String path) throws IOException {
    byte[] bytes = Files.readAllBytes(Paths.get(path));
    run(new String(bytes, Charset.defaultCharset()));

    if (hadError) System.exit(65);
    if (hadRuntimeError) System.exit(70);
  }

  private static void runPrompt() throws IOException {
    InputStreamReader input = new InputStreamReader(System.in);
    BufferedReader reader = new BufferedReader(input);

    for (;;) { 
      System.out.print("> ");
      String line = reader.readLine();
      if (line == null) break;
      runRepl(line);
      hadError = false;
    }
  }

  private static void run(String source) {
    Scanner scanner = new Scanner(source);
    List<Token> tokens = scanner.scanTokens();
    Parser parser = new Parser(tokens);
    List<Stmt> statements = parser.parse();

    if (hadError) return;

    Resolver resolver = new Resolver(interpreter);
    resolver.resolve(statements);

    if (hadError) return;

    interpreter.interpret(statements);
  }

  private static void runRepl(String source) {
    Scanner scanner = new Scanner(source);
    List<Token> tokens = scanner.scanTokens();
    
    // Check if the first token indicates it's definitely a statement
    boolean isObviousStatement = false;
    if (tokens.size() > 0) {
      TokenType firstToken = tokens.get(0).type;
      isObviousStatement = firstToken == TokenType.VAR || 
                          firstToken == TokenType.PRINT ||
                          firstToken == TokenType.LEFT_BRACE;
    }
    
    if (!isObviousStatement) {
      // Try parsing as a bare expression first
      Parser parser = new Parser(tokens);
      try {
        Expr expression = parser.parseExpression();
        if (expression != null && !hadError && parser.isAtEnd()) {
          // Successfully parsed as expression and consumed all tokens
          Object value = interpreter.evaluate(expression);
          System.out.println(interpreter.stringify(value));
          return;
        }
      } catch (Exception e) {
        // Fall through to try parsing as statements
      }
    }
    
    // Reset error state and try parsing as statements
    hadError = false;
    scanner = new Scanner(source);
    tokens = scanner.scanTokens();
    Parser parser = new Parser(tokens);
    List<Stmt> statements = parser.parse();

    if (hadError) return;

    interpreter.interpret(statements);
  }

  static void error(int line, String message) {
    report(line, "", message);
  }

  private static void report(int line, String where,
                             String message) {
    System.err.println(
        "[line " + line + "] Error" + where + ": " + message);
    hadError = true;
  }

  static void error(Token token, String message) {
    if (token.type == TokenType.EOF) {
      report(token.line, " at end", message);
    } else {
      report(token.line, " at '" + token.lexeme + "'", message);
    }
  }

  static void runtimeError(RuntimeError error) {
    System.err.println(error.getMessage() + 
        "\n[line " + error.token.line + "]");
    hadRuntimeError = true;
  }
}