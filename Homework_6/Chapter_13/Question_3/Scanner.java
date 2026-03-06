// LARRY LA
// CS 4080
// HW 6 - Chapter 13, Question 3
// This file implements f-string scanning and tokenization. Added FStringParts helper class
// to store parsed f-string structure (alternating literals and expression code). Modified
// identifier() to detect f" prefix. Added fstring() method to parse f-string content,
// handling {expression} interpolation and \{ escape sequences for literal braces.

package com.craftinginterpreters.lox;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import static com.craftinginterpreters.lox.TokenType.*;

class Scanner {
    // Helper class for storing f-string parts
    static class FStringParts {
        final List<Object> parts = new ArrayList<>();        // Strings for literals, Strings for expression code
        final List<Boolean> isExpression = new ArrayList<>(); // true = expression, false = literal
        
        void addLiteral(String text) {
            parts.add(text);
            isExpression.add(false);
        }
        
        void addExpression(String code) {
            parts.add(code);
            isExpression.add(true);
        }
    }

    private final String source;
    private final List<Token> tokens = new ArrayList<>();
    private int start = 0;
    private int current = 0;
    private int line = 1;

    private static final Map<String, TokenType> keywords;

    static {
        keywords = new HashMap<>();
        keywords.put("and",    AND);
        keywords.put("break",  BREAK);
        keywords.put("class",  CLASS);
        keywords.put("else",   ELSE);
        keywords.put("false",  FALSE);
        keywords.put("for",    FOR);
        keywords.put("fun",    FUN);
        keywords.put("if",     IF);
        keywords.put("nil",    NIL);
        keywords.put("or",     OR);
        keywords.put("print",  PRINT);
        keywords.put("return", RETURN);
        keywords.put("inner",  INNER);
        keywords.put("this",   THIS);
        keywords.put("true",   TRUE);
        keywords.put("var",    VAR);
        keywords.put("while",  WHILE);
    }

    Scanner(String source) {
        this.source = source;
    }

    List<Token> scanTokens() {
        while (!isAtEnd()) {
            // We are at the beginning of the next lexeme.
            start = current;
            scanToken();
        }

        tokens.add(new Token(EOF, "", null, line));
        return tokens;
    }

    private void scanToken() {
        char c = advance();
        switch (c) {
            case '(': addToken(LEFT_PAREN); break;
            case ')': addToken(RIGHT_PAREN); break;
            case '{': addToken(LEFT_BRACE); break;
            case '}': addToken(RIGHT_BRACE); break;
            case ',': addToken(COMMA); break;
            case '.': addToken(DOT); break;
            case '-': addToken(MINUS); break;
            case '+': addToken(PLUS); break;
            case ';': addToken(SEMICOLON); break;
            case '*': addToken(STAR); break;
            case '?': addToken(QUESTION); break;
            case ':': addToken(COLON); break;
            case '!':
                addToken(match('=') ? BANG_EQUAL : BANG);
                break;
            case '=':
                addToken(match('=') ? EQUAL_EQUAL : EQUAL);
                break;
            case '<':
                addToken(match('=') ? LESS_EQUAL : LESS);
                break;
            case '>':
                addToken(match('=') ? GREATER_EQUAL : GREATER);
                break;
            case '/':
                if (match('/')) {
                    // A comment goes until the end of the line.
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else if (match('*')) { 
                    // Challenge 4: Block comments with nesting support
                    blockComment();
                } else {
                    addToken(SLASH);
                }
                break;

            case ' ':
            case '\r':
            case '\t':
                // Ignore whitespace.
                break;

            case '\n':
                line++;
                break;

            case '"': string(); break;

            default:
                if (isDigit(c)) {
                    number();
                } else if (isAlpha(c)) {
                    identifier();
                } else {
                    Lox.error(line, "Unexpected character.");
                }
                break;
        }
    }

    // Challenge 4: Helper for nested block comments
    private void blockComment() {
        int nesting = 1;
        while (nesting > 0) {
            if (isAtEnd()) {
                Lox.error(line, "Unterminated block comment.");
                return;
            }

            if (peek() == '\n') {
                line++;
                advance();
            } else if (peek() == '/' && peekNext() == '*') {
                advance(); // consume /
                advance(); // consume *
                nesting++;
            } else if (peek() == '*' && peekNext() == '/') {
                advance(); // consume *
                advance(); // consume /
                nesting--;
            } else {
                advance();
            }
        }
    }

    private void identifier() {
        while (isAlphaNumeric(peek())) advance();

        String text = source.substring(start, current);
        TokenType type = keywords.get(text);
        if (type == null) type = IDENTIFIER;
        
        // Check for f-string prefix
        if (text.equals("f") && peek() == '"') {
            advance(); // consume the "
            fstring();
            return;
        }
        
        addToken(type);
    }

    private void number() {
        while (isDigit(peek())) advance();

        // Look for a fractional part.
        if (peek() == '.' && isDigit(peekNext())) {
            // Consume the "."
            advance();

            while (isDigit(peek())) advance();
        }

        addToken(NUMBER, Double.parseDouble(source.substring(start, current)));
    }

    private void string() {
        while (peek() != '"' && !isAtEnd()) {
            if (peek() == '\n') line++;
            advance();
        }

        if (isAtEnd()) {
            Lox.error(line, "Unterminated string.");
            return;
        }

        // The closing ".
        advance();

        // Trim the surrounding quotes.
        String value = source.substring(start + 1, current - 1);
        addToken(STRING, value);
    }

    private void fstring() {
        FStringParts parts = new FStringParts();
        StringBuilder currentLiteral = new StringBuilder();
        
        while (peek() != '"' && !isAtEnd()) {
            if (peek() == '\n') line++;
            
            // Handle escape sequences
            if (peek() == '\\') {
                advance(); // consume \
                char next = peek();
                
                if (next == '{' || next == '}') {
                    // Escaped brace - add literal { or }
                    currentLiteral.append(next);
                    advance();
                } else if (next == 'n') {
                    currentLiteral.append('\n');
                    advance();
                } else if (next == 't') {
                    currentLiteral.append('\t');
                    advance();
                } else if (next == '\\') {
                    currentLiteral.append('\\');
                    advance();
                } else if (next == '"') {
                    currentLiteral.append('"');
                    advance();
                } else {
                    // Unknown escape - just add the backslash and character
                    currentLiteral.append('\\');
                    if (!isAtEnd()) {
                        currentLiteral.append(next);
                        advance();
                    }
                }
            }
            // Handle interpolation start
            else if (peek() == '{') {
                // Save current literal part (could be empty)
                parts.addLiteral(currentLiteral.toString());
                currentLiteral = new StringBuilder();
                
                advance(); // consume {
                
                // Parse the expression code until matching }
                int braceDepth = 1;
                StringBuilder exprCode = new StringBuilder();
                
                while (braceDepth > 0 && !isAtEnd()) {
                    if (peek() == '{') {
                        braceDepth++;
                    } else if (peek() == '}') {
                        braceDepth--;
                        if (braceDepth == 0) break; // don't add the closing }
                    }
                    
                    if (peek() == '\n') line++;
                    exprCode.append(peek());
                    advance();
                }
                
                if (braceDepth != 0) {
                    Lox.error(line, "Unmatched '{' in f-string.");
                    return;
                }
                
                advance(); // consume closing }
                
                // Add expression part
                String expr = exprCode.toString().trim();
                if (expr.length() == 0) {
                    Lox.error(line, "Empty expression in f-string interpolation.");
                }
                parts.addExpression(expr);
            }
            // Regular character
            else {
                currentLiteral.append(peek());
                advance();
            }
        }
        
        if (isAtEnd()) {
            Lox.error(line, "Unterminated f-string.");
            return;
        }
        
        // Add final literal part
        parts.addLiteral(currentLiteral.toString());
        
        advance(); // consume closing "
        
        addToken(F_STRING, parts);
    }

    private boolean match(char expected) {
        if (isAtEnd()) return false;
        if (source.charAt(current) != expected) return false;

        current++;
        return true;
    }

    private char peek() {
        if (isAtEnd()) return '\0';
        return source.charAt(current);
    }

    private char peekNext() {
        if (current + 1 >= source.length()) return '\0';
        return source.charAt(current + 1);
    }

    private boolean isAlpha(char c) {
        return (c >= 'a' && c <= 'z') ||
               (c >= 'A' && c <= 'Z') ||
                c == '_';
    }

    private boolean isAlphaNumeric(char c) {
        return isAlpha(c) || isDigit(c);
    }

    private boolean isDigit(char c) {
        return c >= '0' && c <= '9';
    }

    private boolean isAtEnd() {
        return current >= source.length();
    }

    private char advance() {
        return source.charAt(current++);
    }

    private void addToken(TokenType type) {
        addToken(type, null);
    }

    private void addToken(TokenType type, Object literal) {
        String text = source.substring(start, current);
        tokens.add(new Token(type, text, literal, line));
    }
}
