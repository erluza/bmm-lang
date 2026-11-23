# Tutorial

## 1. First program

```bmm
println("Hello, world")
```

Run it with `bmm hello.bmm`.

## 2. Variables and arithmetic

```bmm
let x = 10
let y = 3
println(str(x + y))
println(str(x * y))
x = x + 1
println(str(x))
```

## 3. Conditionals

```bmm
let n = 7
if n % 2 == 0
  println("even")
else
  println("odd")
end
```

## 4. Loops

```bmm
let i = 0
loop i < 5
  println(str(i))
  i = i + 1
end
```

## 5. Functions

```bmm
fn square(x)
  ret x * x
end

println(str(square(6)))
```

## 6. Recursion

```bmm
fn fib(n)
  if n < 2
    ret n
  end
  ret fib(n - 1) + fib(n - 2)
end

println(str(fib(10)))
```

## 7. Arrays

```bmm
let a = [1, 2, 3]
push(a, 4)
println(str(a[1]))
println(str(len(a)))
```

## 8. Strings

```bmm
let s = "hello" .. " " .. "world"
println(s)
println(str(len(s)))
println(str(sub(s, 0, 5)))
```

## Exercises

1. Write a function that returns the larger of two integers.
2. Print the first ten Fibonacci numbers.
3. Read a line of input and print it reversed.
4. Implement insertion sort on an array.
