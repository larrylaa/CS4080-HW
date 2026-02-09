-- LARRY LA - CS 4080 - HW 2

-- OUTPUT:
-- Structure: (1 + (2 + 3))
-- Result: 6

-- The "Interface" defined as a record of operations
data Expr = Expr {
    eval :: Int,
    view :: String
}

-- The "Literal" type implementation
literal :: Int -> Expr
literal n = Expr {
    eval = n,
    view = show n
}

-- The "Addition" type implementation
add :: Expr -> Expr -> Expr
add left right = Expr {
    eval = (eval left) + (eval right),
    view = "(" ++ (view left) ++ " + " ++ (view right) ++ ")"
}

-- Main entry point to demonstrate the pattern in action
main :: IO ()
main = do
    let expression = add (literal 1) (add (literal 2) (literal 3))
    putStrLn $ "Structure: " ++ view expression
    putStrLn $ "Result: " ++ show (eval expression)