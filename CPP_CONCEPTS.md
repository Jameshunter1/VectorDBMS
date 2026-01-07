# C++ Concepts Explained (For Beginners)

This document explains key C++ concepts you'll encounter in the database engine code.

## Table of Contents
1. [Basic Syntax](#basic-syntax)
2. [Headers and Includes](#headers-and-includes)
3. [Data Types](#data-types)
4. [Pointers and References](#pointers-and-references)
5. [Functions and Methods](#functions-and-methods)
6. [Classes and Objects](#classes-and-objects)
7. [Memory Management](#memory-management)
8. [Concurrency](#concurrency)
9. [Error Handling](#error-handling)
10. [Modern C++ Features](#modern-c-features)

---

## Basic Syntax

### Comments
```cpp
// Single-line comment

/* Multi-line
   comment */
```

### Variables
```cpp
int count = 42;              // Integer
double price = 19.99;        // Floating point
std::string name = "Alice";  // Text string
bool is_valid = true;        // Boolean (true/false)
```

### Semicolons
Every statement in C++ ends with a semicolon `;` (unlike Python).

---

## Headers and Includes

### What is `#include`?
Think of `#include` like `import` in Python or JavaScript. It brings in code from other files.

```cpp
#include <string>                    // Standard library header
#include <core_engine/engine.hpp>    // Our project header
```

### Header Files (`.hpp`, `.h`)
- Declare what functions/classes exist
- Like a "table of contents" for code
- Usually contain **declarations** only

### Source Files (`.cpp`)
- Contain the actual **implementation**
- Where the real code lives

---

## Data Types

### `std::string`
Text data type. Works like strings in Python/JavaScript.

```cpp
std::string key = "username";
std::string value = "alice";
std::string combined = key + "=" + value;  // Concatenation
```

### `std::vector<T>`
Dynamic array (like Python list or JavaScript array).

```cpp
std::vector<int> numbers = {1, 2, 3, 4, 5};
numbers.push_back(6);        // Add to end
int first = numbers[0];      // Access by index
size_t count = numbers.size(); // Get size
```

### `std::map<K, V>`
Dictionary/hash map (like Python dict).

```cpp
std::map<std::string, std::string> data;
data["key1"] = "value1";     // Insert
auto value = data["key1"];   // Retrieve
```

### `std::optional<T>`
A container that either holds a value **or is empty**.

```cpp
std::optional<std::string> maybe_value = GetValue("key");

if (maybe_value.has_value()) {
    std::string value = *maybe_value;  // Extract value with *
    // Use the value
} else {
    // Key not found
}
```

---

## Pointers and References

### Pointers (`*`)
A variable that holds a **memory address**.

```cpp
int x = 42;
int* ptr = &x;    // ptr points to x (& = "address of")
int value = *ptr; // Get value at address (* = "dereference")
```

### References (`&`)
An alias (another name) for a variable. Always valid, can't be null.

```cpp
int x = 42;
int& ref = x;     // ref is another name for x
ref = 100;        // This changes x to 100!
```

### When to use what?
- **Reference (`&`)**: When you want to pass/modify without copying
- **Pointer (`*`)**: When you need optional values or manual memory management

---

## Functions and Methods

### Regular Functions
```cpp
// Declaration (in .hpp file)
int Add(int a, int b);

// Implementation (in .cpp file)
int Add(int a, int b) {
    return a + b;
}
```

### Methods (Functions in Classes)
```cpp
class Calculator {
public:
    int Add(int a, int b) {
        return a + b;
    }
};

Calculator calc;
int result = calc.Add(5, 3);  // Call method on object
```

### Return Types
```cpp
int GetNumber() { return 42; }           // Returns integer
void PrintMessage() { /* no return */ }  // Returns nothing
std::string GetName() { return "Alice"; } // Returns string
```

---

## Classes and Objects

### Defining a Class
```cpp
class Person {
public:                        // Public = accessible from outside
    std::string name;
    int age;
    
    void SayHello() {
        std::cout << "Hello, I'm " << name << std::endl;
    }

private:                       // Private = only accessible inside class
    std::string secret;
};
```

### Creating Objects
```cpp
Person alice;           // Create object
alice.name = "Alice";   // Set member variable
alice.age = 30;
alice.SayHello();       // Call method
```

### Constructors
Special methods called when creating objects.

```cpp
class Person {
public:
    // Constructor
    Person(std::string n, int a) : name(n), age(a) {}
    
private:
    std::string name;
    int age;
};

Person alice("Alice", 30);  // Calls constructor
```

---

## Memory Management

### Stack vs Heap
- **Stack**: Automatic memory, cleaned up automatically when scope ends
- **Heap**: Manual memory, you control when it's allocated/freed

```cpp
// Stack (automatic)
int x = 42;  // Cleaned up when function returns

// Heap (manual - avoid in modern C++)
int* ptr = new int(42);  // Allocate
delete ptr;              // Must manually free!
```

### `std::unique_ptr<T>`
**Smart pointer** - automatically frees memory when no longer needed.

```cpp
#include <memory>

std::unique_ptr<MyClass> obj = std::make_unique<MyClass>();
// Use obj like a pointer: obj->Method()
// Memory automatically freed when obj goes out of scope
```

### RAII (Resource Acquisition Is Initialization)
**Core C++ principle**: Acquire resources in constructor, release in destructor.

```cpp
class File {
public:
    File(const std::string& path) {
        file_ = fopen(path.c_str(), "w");  // Acquire resource
    }
    
    ~File() {  // Destructor (called automatically when object destroyed)
        if (file_) fclose(file_);          // Release resource
    }
    
private:
    FILE* file_;
};
```

---

## Concurrency

### `std::mutex`
**Mutual exclusion lock** - ensures only one thread accesses data at a time.

```cpp
#include <mutex>

std::mutex data_mutex;
std::map<std::string, std::string> data;

void ThreadSafeInsert(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(data_mutex);  // Acquires lock
    data[key] = value;
    // Lock automatically released when 'lock' goes out of scope
}
```

### Why Mutexes?
Without mutexes, two threads could modify data simultaneously, causing **race conditions** (corrupted data).

---

## Error Handling

### Return Codes (Our Approach)
```cpp
class Status {
public:
    static Status OK() { return Status(true, ""); }
    static Status Error(const std::string& msg) { return Status(false, msg); }
    
    bool ok() const { return is_ok_; }
    std::string ToString() const { return message_; }
    
private:
    Status(bool ok, const std::string& msg) : is_ok_(ok), message_(msg) {}
    bool is_ok_;
    std::string message_;
};

Status DoSomething() {
    if (/* success */) {
        return Status::OK();
    } else {
        return Status::Error("Something went wrong");
    }
}

// Usage:
auto status = DoSomething();
if (!status.ok()) {
    std::cerr << "Error: " << status.ToString() << std::endl;
}
```

### Exceptions (Not Used in This Project)
Alternative error handling (throws errors up the call stack).

```cpp
throw std::runtime_error("Error message");  // Throw exception

try {
    DoSomething();
} catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;    // Catch and handle
}
```

---

## Modern C++ Features

### `auto` Keyword
Compiler figures out the type automatically.

```cpp
auto x = 42;                    // x is int
auto name = std::string("Alice"); // name is std::string
auto result = engine.Get("key");  // result is std::optional<std::string>
```

### Range-Based For Loop
Easier way to iterate over collections.

```cpp
std::vector<int> numbers = {1, 2, 3, 4, 5};

// Old way:
for (size_t i = 0; i < numbers.size(); ++i) {
    std::cout << numbers[i] << std::endl;
}

// Modern way:
for (int num : numbers) {
    std::cout << num << std::endl;
}
```

### Lambda Functions
Anonymous functions (like Python lambda or JavaScript arrow functions).

```cpp
// Basic lambda
auto add = [](int a, int b) { return a + b; };
int result = add(5, 3);  // result = 8

// Lambda with capture [&]
int multiplier = 10;
auto multiply = [&](int x) { return x * multiplier; };
int result = multiply(5);  // result = 50

// Lambda in algorithm
std::vector<int> nums = {1, 2, 3, 4, 5};
std::for_each(nums.begin(), nums.end(), [](int n) {
    std::cout << n << std::endl;
});
```

**Capture modes:**
- `[&]` = Capture all variables by reference
- `[=]` = Capture all variables by copy
- `[x, &y]` = Capture `x` by copy, `y` by reference

### Move Semantics
**Transfer ownership** instead of copying (much faster for large objects).

```cpp
std::vector<int> v1 = {1, 2, 3, 4, 5};
std::vector<int> v2 = std::move(v1);  // Move (fast), don't copy
// v1 is now empty, v2 owns the data
```

### Raw String Literals
Multi-line strings without escaping.

```cpp
const char* html = R"HTML(
<html>
  <body>
    <p>No need to escape "quotes"!</p>
  </body>
</html>
)HTML";
```

---

## Common Patterns in This Project

### 1. Passing by `const` Reference
Efficient way to pass large objects without copying.

```cpp
void ProcessData(const std::string& data) {
    // Can read 'data' but not modify it
}
```

### 2. Returning `std::optional`
Indicates "value or nothing" (like Python's `None` or JavaScript's `null/undefined`).

```cpp
std::optional<std::string> Get(const std::string& key) {
    if (/* key exists */) {
        return value;
    } else {
        return std::nullopt;  // Return "nothing"
    }
}
```

### 3. Using Status Objects
Check if operations succeeded without exceptions.

```cpp
auto status = engine.Open(db_path);
if (!status.ok()) {
    // Handle error
}
```

### 4. RAII with Lock Guards
Automatically acquire/release mutex locks.

```cpp
std::lock_guard<std::mutex> lock(mutex_);
// Lock acquired here
// ... critical section ...
// Lock automatically released when 'lock' goes out of scope
```

---

## Learning Resources

- **C++ Basics**: https://www.learncpp.com/
- **Modern C++**: https://github.com/AnthonyCalandra/modern-cpp-features
- **Standard Library**: https://en.cppreference.com/
- **Best Practices**: https://isocpp.github.io/CppCoreGuidelines/

---

## Tips for Reading Our Code

1. **Start with headers (`.hpp`)**: See what's public/available
2. **Read top-down**: Classes → public methods → private members
3. **Follow the data**: Track how key-value pairs flow through the system
4. **Ignore complex parts first**: Focus on high-level structure
5. **Use comments**: We've added extensive explanations for learners

Happy learning! 🚀
