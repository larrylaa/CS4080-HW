package com.craftinginterpreters.lox;

class RpnPrinter implements Expr.Visitor<String> {
    String print(Expr expr) {
        return expr.accept(this);
    }

    @Override
    public String visitBinaryExpr(Expr.Binary expr) {
        // In RPN: left, right, operator
        return expr.left.accept(this) + " " + 
               expr.right.accept(this) + " " + 
               expr.operator.lexeme;
    }

    @Override
    public String visitTernaryExpr(Expr.Ternary expr) {
        return expr.condition.accept(this) + " " +
               expr.thenBranch.accept(this) + " " +
               expr.elseBranch.accept(this) + " ?:";
    }

    @Override
    public String visitGroupingExpr(Expr.Grouping expr) {
        // Parentheses don't exist in RPN; just visit the inner expression
        return expr.expression.accept(this);
    }

    @Override
    public String visitLiteralExpr(Expr.Literal expr) {
        if (expr.value == null) return "nil";
        return expr.value.toString();
    }

    @Override
    public String visitUnaryExpr(Expr.Unary expr) {
        // For unary, the operand comes before the operator (e.g., "123 -")
        return expr.right.accept(this) + " " + expr.operator.lexeme;
    }

    @Override
    public String visitAssignExpr(Expr.Assign expr) {
        // For assignment in RPN: value, variable, =
        return expr.value.accept(this) + " " + 
               expr.name.lexeme + " =";
    }

    @Override
    public String visitVariableExpr(Expr.Variable expr) {
        return expr.name.lexeme;
    }

    @Override
    public String visitLogicalExpr(Expr.Logical expr) {
        // In RPN: left, right, operator
        return expr.left.accept(this) + " " + 
               expr.right.accept(this) + " " + 
               expr.operator.lexeme;
    }
}