# 🔧 Vexium Debugging & Troubleshooting Guide

A comprehensive guide to debugging Vexium programs and fixing common issues.

---

## Quick Reference

| Problem | Solution |
|---------|----------|
| "undefined variable" | Declare variable before use |
| Program produces no output | Use `display` to print |
| Array index out of bounds | Check bounds before accessing |
| Function not found | Define function before calling |
| Cannot concatenate string + number | Use `str()` to convert |
| Infinite loop | Check loop condition |
| Wrong output | Debug with `display` statements |
| Syntax error | Check syntax guide or examples |

---

## Debugging Techniques

### 1. Use `display` for Debugging

**Problem:** Program runs but you don't know what's happening

**Solution:** Add `display` statements:

```vex
fn calculate_discount(price, percent):
    display "Starting price: " + str(price)
    display "Discount percent: " + str(percent)
    
    let discount be price * (percent / 100)
    display "Discount amount: " + str(discount)
    
    let final_price be price - discount
    display "Final price: " + str(final_price)
    
    give back final_price

calculate_discount(100, 20)
```

Output:
```
Starting price: 100
Discount percent: 20
Discount amount: 20
Final price: 80
```

### 2. Debug Variables

**Problem:** Variable has unexpected value

**Solution:** Print at key points:

```vex
let count be 0
for i in 0 to 5:
    count be count + i
    display "Iteration " + str(i) + ", count = " + str(count)

display "Final count: " + str(count)
```

Output:
```
Iteration 0, count = 0
Iteration 1, count = 1
Iteration 2, count = 3
Iteration 3, count = 6
Iteration 4, count = 10
Final count: 10
```

### 3. Debug Arrays

```vex
let arr be [1, 2, 3]
display "Array: "
for each item in arr:
    display "  - " + str(item)

display "Length: " + str(len(arr))
```

### 4. Debug Maps

```vex
let person be {"name": "Alice", "age": 30, "city": "NYC"}
display "Person data:"
for key in keys(person):
    display "  " + key + ": " + str(person[key])
```

### 5. Debug Function Returns

```vex
fn get_discount_percent(customer_type):
    display "Calculating discount for: " + customer_type
    
    let percent be 0
    if customer_type is "vip":
        percent be 20
    elif customer_type is "regular":
        percent be 10
    else:
        percent be 0
    
    display "Returning discount: " + str(percent) + "%"
    give back percent

let discount be get_discount_percent("vip")
display "Discount applied: " + str(discount) + "%"
```

---

## Common Errors & Solutions

### Error: "Undefined variable"

**Code:**
```vex
display x
let x be 5
```

**Problem:** Variable used before declaration

**Solution:** Declare before use
```vex
let x be 5
display x
```

### Error: "Function not defined"

**Code:**
```vex
display add(5, 3)

fn add(a, b):
    give back a + b
```

**Problem:** Function called before definition

**Solution:** Define function first
```vex
fn add(a, b):
    give back a + b

display add(5, 3)
```

### Error: "Index out of bounds"

**Code:**
```vex
let arr be [1, 2, 3]
display arr[5]  # Array only has indices 0, 1, 2
```

**Problem:** Accessing non-existent array index

**Solution:** Check bounds
```vex
let arr be [1, 2, 3]
let index be 5

if index is at least 0 and index is less than len(arr):
    display arr[index]
else:
    display "Index " + str(index) + " out of bounds"
```

### Error: Cannot concatenate string and number

**Code:**
```vex
let age be 25
display "Age: " + age  # Wrong - mixing string and number
```

**Problem:** Cannot add string and number directly

**Solution:** Convert to string
```vex
let age be 25
display "Age: " + str(age)
```

### Error: Map key not found

**Code:**
```vex
let person be {"name": "Alice"}
display person.email  # Key doesn't exist
```

**Problem:** Accessing non-existent map key

**Solution:** Check if key exists first
```vex
let person be {"name": "Alice"}

if has_key(person, "email"):
    display person.email
else:
    display "Email not found"
```

### Error: Constant reassignment

**Code:**
```vex
const PI be 3.14159
PI be 3.14  # Cannot reassign
```

**Problem:** Trying to change constant value

**Solution:** Use `let` for variables
```vex
let PI be 3.14159
PI be 3.14  # OK

# Or keep as constant and use different variable
const PI be 3.14159
let pi_approx be 3.14
```

---

## Logic Errors

### Problem: Infinite Loop

**Code:**
```vex
let i be 0
while i is less than 10:
    display i
    # Missing: i be i + 1
```

**Symptom:** Program hangs, keeps running

**Solution:** Update loop variable
```vex
let i be 0
while i is less than 10:
    display i
    i be i + 1
```

### Problem: Wrong Loop Range

**Code:**
```vex
for i in 0 to 5:
    display i
# Expected: 0,1,2,3,4,5
# Actually prints: 0,1,2,3,4 (doesn't include 5)
```

**Solution:** Adjust range
```vex
for i in 0 to 6:  # 0 to 5 inclusive
    display i

# Or use correct semantics - 0 to 5 is 0,1,2,3,4
for i in 1 to 5:  # If you want 1-5
    display i
```

### Problem: Off-by-One Error

**Code:**
```vex
let arr be [10, 20, 30]
for i in 0 to len(arr) + 1:  # Too many iterations
    display arr[i]  # Error on last iteration
```

**Solution:** Use correct length
```vex
let arr be [10, 20, 30]
for i in 0 to len(arr):
    display arr[i]
```

### Problem: Variable Shadowing

**Code:**
```vex
let x be "outer"
if true:
    let x be "inner"
    display x  # "inner"
display x      # "outer" - unexpected!
```

**Explanation:** Inner `let x` creates new variable

**Solution:** Use different variable names
```vex
let x be "outer"
if true:
    let x_inner be "inner"
    display x_inner

display x
```

---

## Debugging Checklist

### Before Running
- [ ] All variables declared before use?
- [ ] All functions defined before calling?
- [ ] Correct array indices?
- [ ] Numbers vs strings handled correctly?
- [ ] Loop conditions correct?
- [ ] Function parameters match arguments?

### If No Output
- [ ] Used `display` to print?
- [ ] Program completes successfully?
- [ ] Check syntax errors first

### If Wrong Output
- [ ] Add `display` statements to trace execution
- [ ] Check variable values at each step
- [ ] Verify function logic
- [ ] Check loop behavior
- [ ] Validate conditions

### If Program Crashes
- [ ] Check error message carefully
- [ ] Look at line number
- [ ] Debug variables around error
- [ ] Check array bounds
- [ ] Verify map keys exist

---

## Debugging with Print Statements

### Level 1: Basic Tracing

```vex
fn process_data(data):
    display "START: process_data"
    
    let result be transform(data)
    display "After transform: " + str(result)
    
    let final be validate(result)
    display "After validate: " + str(final)
    
    display "END: process_data"
    give back final
```

### Level 2: Variable State

```vex
fn calculate(numbers):
    display "Input: " + str(numbers)
    
    let sum be 0
    let count be 0
    
    for each num in numbers:
        sum be sum + num
        count be count + 1
        display "After item " + str(count) + ": sum = " + str(sum)
    
    let average be sum / count
    display "Final: sum = " + str(sum) + ", count = " + str(count)
    display "Average: " + str(average)
    
    give back average
```

### Level 3: Condition Tracking

```vex
fn is_valid(age):
    display "Checking age: " + str(age)
    
    if age is less than 0:
        display "Age is negative - INVALID"
        give back false
    
    if age is greater than 150:
        display "Age too high - INVALID"
        give back false
    
    display "Age check passed - VALID"
    give back true
```

---

## Testing Your Code

### Unit Test Example

```vex
fn assert_equals(actual, expected, name):
    if actual is expected:
        display "✓ " + name
    else:
        display "✗ " + name
        display "  Expected: " + str(expected)
        display "  Got: " + str(actual)

# Test add function
fn add(a, b):
    give back a + b

assert_equals(add(2, 3), 5, "add(2, 3) = 5")
assert_equals(add(0, 0), 0, "add(0, 0) = 0")
assert_equals(add(-1, 1), 0, "add(-1, 1) = 0")
```

### Integration Test Example

```vex
fn test_user_creation():
    let user be create_user("Alice", "alice@example.com")
    
    display "Testing user creation:"
    
    if user.name is "Alice":
        display "✓ Name set correctly"
    else:
        display "✗ Name not set"
    
    if user.email is "alice@example.com":
        display "✓ Email set correctly"
    else:
        display "✗ Email not set"
    
    if user.id is not null:
        display "✓ ID generated"
    else:
        display "✗ ID not generated"

test_user_creation()
```

---

## Debugging Strategies

### Strategy 1: Narrow Down the Problem

```vex
# Start simple
let result be complex_calculation()
display result

# If wrong, test components
display "Part A: " + str(part_a)
display "Part B: " + str(part_b)
display "Combined: " + str(combine(part_a, part_b))
```

### Strategy 2: Use Temporary Code

```vex
# Add temporary test code
fn test_edge_cases():
    # Test empty array
    let empty be []
    display "Empty: " + str(process(empty))
    
    # Test single element
    let single be [1]
    display "Single: " + str(process(single))
    
    # Test normal case
    let normal be [1, 2, 3]
    display "Normal: " + str(process(normal))

test_edge_cases()

# Remove when done
```

### Strategy 3: Isolate Variables

```vex
# Instead of:
let x be a + b * c - d / e

# Break down:
let temp1 be b * c
let temp2 be d / e
let temp3 be a + temp1
let x be temp3 - temp2

display "temp1: " + str(temp1)
display "temp2: " + str(temp2)
display "x: " + str(x)
```

---

## Performance Debugging

### Slow Program?

**Check for:**

1. **Nested loops**
```vex
# Slow: O(n²)
for i in 0 to len(arr):
    for j in 0 to len(arr):
        process(arr[i], arr[j])

# Better: if possible, O(n)
for each item in arr:
    process(item)
```

2. **Recalculation in loops**
```vex
# Slow
for i in 0 to len(arr):
    let len_val be len(arr)  # Recalculated!

# Fast
let len_val be len(arr)
for i in 0 to len_val:
    # Use len_val
```

3. **Inefficient string operations**
```vex
# Slow: creates many intermediate strings
let result be "" + "a" + "b" + "c" + "d"

# Better: use array and join
let parts be ["a", "b", "c", "d"]
let result be join(parts, "")
```

---

## Error Handling Debugging

### Catch and Debug Exceptions

```vex
attempt:
    let result be risky_operation()
    display "Success: " + str(result)
otherwise err:
    display "ERROR DETAILS:"
    display "  Type: " + str(type(err))
    display "  Message: " + str(err)
    
    # Debug surrounding variables
    display "DEBUG INFO:"
    display "  Input was: " + str(input_data)
    display "  Expected: valid JSON"
```

### Defensive Programming

```vex
fn safe_divide(a, b):
    display "Dividing " + str(a) + " by " + str(b)
    
    if b is 0:
        display "ERROR: Division by zero"
        give back null
    
    if not is_number(a) or not is_number(b):
        display "ERROR: Invalid input types"
        give back null
    
    let result be a / b
    display "Result: " + str(result)
    give back result
```

---

## Tips for Effective Debugging

1. **Read error messages carefully** - They tell you exactly what's wrong
2. **Use meaningful debug output** - "Value is 5" not just "5"
3. **Test edge cases** - Empty arrays, zero, negative numbers
4. **Simplify test cases** - Start simple, build complexity
5. **Check assumptions** - Don't assume, verify with display
6. **Use consistent naming** - Easier to track variables
7. **Test one thing at a time** - Don't change multiple things
8. **Keep old code** - Comment out instead of delete, test removal
9. **Take breaks** - Fresh eyes help find bugs
10. **Pair program** - Explaining helps find issues

---

## Useful Debugging Functions

Create these helper functions:

```vex
# Debug helper - print any value nicely
fn debug(label, value):
    display "[DEBUG] " + label + ": " + str(value)

# Assert helper - catch logic errors
fn assert_true(condition, message):
    if not condition:
        display "[ASSERT FAILED] " + message
    else:
        display "[ASSERT PASSED] " + message

# Print array
fn print_array(label, arr):
    display "[ARRAY] " + label + " (length: " + str(len(arr)) + ")"
    for i in 0 to len(arr):
        display "  [" + str(i) + "] = " + str(arr[i])

# Print map
fn print_map(label, map):
    display "[MAP] " + label + " (keys: " + str(len(keys(map))) + ")"
    for key in keys(map):
        display "  " + key + " = " + str(map[key])

# Usage:
debug("username", user.name)
assert_true(age is at least 0, "Age cannot be negative")
print_array("scores", [85, 90, 78])
print_map("user", {"name": "Alice", "age": 30})
```

---

## Common Mistakes Summary

| Mistake | Result | Fix |
|---------|--------|-----|
| Use variable before declaring | Error | Declare first |
| Call function before defining | Error | Define first |
| Wrong array index | Out of bounds error | Check bounds |
| Forget `str()` in concatenation | Type error | Convert types |
| Infinite loop | Program hangs | Fix condition |
| Miss `give back` | Returns null | Add return |
| Change loop var in loop | Unexpected behavior | Be careful |
| Access non-existent key | Error or null | Check first |
| Forget `display` | No output | Add output |
| Off-by-one in loop | Wrong count | Check range |

---

## When to Ask for Help

If you've tried debugging and still stuck:

1. **Create minimal example** - Simplify problem to smallest case
2. **Share exact error message** - Include line number
3. **Show what you tried** - What debugging did you attempt?
4. **Provide context** - What were you trying to do?
5. **Share code snippet** - Relevant code, not entire program

Example:
```
I'm trying to find items in an array.

My code:
fn find_item(list, target):
    for i in 0 to len(list):
        if list[i] is target:
            give back i

Error: Index out of bounds on line 4

I tried:
- Using different arrays
- Checking list length
- Adding display statements

What am I missing?
```

---

## Debugging Resources

- [VEXIUM_SYNTAX.md](VEXIUM_SYNTAX.md) - Reference for syntax rules
- [VEXIUM_QUICK_START.md](VEXIUM_QUICK_START.md) - Basic examples
- [VEXIUM_FAQ.md](VEXIUM_FAQ.md) - Common questions
- [VEXIUM_BEST_PRACTICES.md](VEXIUM_BEST_PRACTICES.md) - Write better code

---

Happy debugging! Remember: Every bug is a learning opportunity. 🐛→🐦
