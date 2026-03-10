# 🏆 Vexium Best Practices Guide

Professional guidance for writing clean, maintainable, and efficient Vexium code.

---

## Quick Index

- [Naming Conventions](#naming-conventions)
- [Code Organization](#code-organization)
- [Function Design](#function-design)
- [Error Handling](#error-handling)
- [Performance](#performance)
- [Testing](#testing)
- [Documentation](#documentation)
- [Common Patterns](#common-patterns)

---

## Naming Conventions

### Variables & Constants

**✅ DO:**
```vex
# Use descriptive names
let user_name be "Alice"
let is_active be true
let max_retries be 3

# Use snake_case
let customer_email be "user@example.com"
let total_amount be 0

# Constants are UPPERCASE
const MAX_CONNECTIONS be 100
const DEFAULT_TIMEOUT be 30
const API_BASE_URL be "https://api.example.com"
```

**❌ DON'T:**
```vex
# Too vague
let x be "Alice"
let temp be true

# Use camelCase (Python-style is better)
let userName be "Alice"
let isActive be true

# Unclear abbreviations
let ua be "Alice"
let max_r be 3
```

### Functions

**✅ DO:**
```vex
# Describe what it does
fn calculate_total_price(items):
    ...

fn is_valid_email(email):
    ...

fn format_datetime(date, format):
    ...
```

**❌ DON'T:**
```vex
# Too generic
fn process(data):
    ...

fn check(value):
    ...

# Unclear abbreviations
fn calc_tp(items):
    ...
```

### Structs & Classes

**✅ DO:**
```vex
# Use PascalCase
struct UserProfile:
    has username
    has email_address

struct PaymentProcessor:
    has provider_name
```

**❌ DON'T:**
```vex
# Not capitalized
struct user_profile:
    ...

struct payment_processor:
    ...
```

---

## Code Organization

### File Structure

**✅ DO:**
```
my_project/
├── main.vxm                    # Entry point
├── lib/                        # Library code
│   ├── models.vxm            # Data structures
│   ├── constants.vxm         # Constants
│   ├── validators.vxm        # Validation logic
│   │
│   ├── services/             # Business logic
│   │   ├── user_service.vxm
│   │   ├── order_service.vxm
│   │   └── payment_service.vxm
│   │
│   └── utils/                # Utilities
│       ├── string_utils.vxm
│       ├── math_utils.vxm
│       └── date_utils.vxm
│
├── tests/                      # Tests
│   ├── test_models.vxm
│   ├── test_validators.vxm
│   └── test_services.vxm
│
└── data/                       # Configuration
    ├── config.vxm
    └── constants.vxm
```

**❌ DON'T:**
```
# Everything in one file
my_project/
├── main.vxm  # 5000+ lines, impossible to maintain

# Random organization
my_project/
├── stuff.vxm
├── more_stuff.vxm
├── helpers.vxm
├── old_code.vxm
```

### Module Organization

**✅ DO:**
```vex
# models.vxm - Group related data structures
struct User:
    has id
    has name
    has email

struct Order:
    has id
    has user_id
    has amount

struct Product:
    has id
    has name
    has price
```

**❌ DON'T:**
```vex
# Mixed concerns
struct User:
    has id
    
    can send_email():
        # Email logic shouldn't be in User

fn calculate_tax(amount):
    # Tax calculation mixed with model
```

---

## Function Design

### Keep Functions Focused

**✅ DO:**
```vex
# Single responsibility
fn validate_email(email):
    if contains(email, "@"):
        if contains(email, "."):
            give back true
    give back false

fn send_email(recipient, subject, body):
    # Sends email - one job

fn process_order(order):
    # Process one order - single responsibility
```

**❌ DON'T:**
```vex
# Too many responsibilities
fn process_everything(data, should_email, should_validate, format):
    if should_validate:
        if not validate(data):
            give back false
    
    if should_email:
        send_email(data.email, "Hi", "data")
    
    if format:
        data be format_data(data)
    
    return save_data(data)
```

### Function Parameters

**✅ DO:**
```vex
# Clear parameter list (max 3-4)
fn create_user(name, email):
    ...

fn create_user(name, email, is_active):
    ...

fn update_user(user_id, updates):
    ...
```

**❌ DON'T:**
```vex
# Too many parameters
fn process(a, b, c, d, e, f, g):
    ...

# Use a struct instead
fn process(config):
    let db_url be config.db_url
    let api_key be config.api_key
    ...
```

### Default Parameters

**✅ DO:**
```vex
# Provide sensible defaults
fn send_email(to, subject, priority be "normal"):
    ...

fn create_timer(duration, repeat be false):
    ...

fn log(message, level be "info"):
    ...
```

**❌ DON'T:**
```vex
# Every parameter required
fn send_email(to, subject, priority):
    # Caller must specify priority every time

# Magic values
fn send_email(to, subject):
    let priority be "normal"  # Magic string
```

### Return Values

**✅ DO:**
```vex
# Be consistent
fn find_user(id):
    if id > 0:
        give back user
    give back null

# Use descriptive returns
fn parse_json(text):
    attempt:
        give back {"success": true, "data": parsed}
    otherwise err:
        give back {"success": false, "error": err}

# Return early for error cases
fn process_payment(amount):
    if amount is less than 0:
        give back false
    if not validate_card():
        give back false
    return process(amount)
```

**❌ DON'T:**
```vex
# Inconsistent returns
fn find_user(id):
    if id > 0:
        give back user
    # No explicit return

# Nested returns
fn process_payment(amount):
    if amount is greater than 0:
        if validate_card():
            if sufficient_funds():
                give back process(amount)
```

---

## Error Handling

### Always Handle Errors

**✅ DO:**
```vex
fn read_user_data(filename):
    attempt:
        let content be read_file(filename)
        let data be parse_json(content)
        give back data
    otherwise err:
        display "Error reading file: " + err
        give back null

# Use error returns
fn divide(a, b):
    if b is 0:
        give back {"success": false, "error": "Division by zero"}
    give back {"success": true, "result": a / b}
```

**❌ DON'T:**
```vex
# Ignore errors
fn read_user_data(filename):
    let content be read_file(filename)
    let data be parse_json(content)
    give back data

# Silent failures
fn divide(a, b):
    if b is 0:
        give back 0  # Misleading, 0 is a valid result
    give back a / b
```

### Error Information

**✅ DO:**
```vex
# Provide context
attempt:
    let user be find_user(user_id)
otherwise err:
    display "Failed to find user " + str(user_id) + ": " + err

# Log important information
attempt:
    process_payment(order_id, amount)
otherwise err:
    log("Payment failed for order " + str(order_id), "error")
    log("Amount: " + str(amount) + ", Error: " + err, "error")
```

**❌ DON'T:**
```vex
# Generic errors
attempt:
    ...
otherwise err:
    display "Error"  # What error? Where?

# Swallowed errors
attempt:
    ...
otherwise err:
    pass  # Error ignored silently
```

---

## Performance

### Avoid Unnecessary Loops

**✅ DO:**
```vex
# Check once
fn is_in_list(numbers, target):
    for each num in numbers:
        if num is target:
            give back true
    give back false

# Use built-in search
fn find_index(numbers, target):
    for i in 0 to len(numbers):
        if numbers[i] is target:
            give back i
    give back -1
```

**❌ DON'T:**
```vex
# Nested loops when not needed
fn is_in_list(numbers, target):
    for each num in numbers:
        for each num2 in numbers:  # Why?
            if num is target:
                give back true

# Recalculate in loop
fn calculate(list):
    for i in 0 to len(list):  # len() called each iteration
        let item be list[i]
        ...
```

### Cache Results

**✅ DO:**
```vex
fn expensive_operation(data):
    let result be complex_calculation(data)
    return result

# Store in variable
let length be len(my_list)
for i in 0 to length:
    process(my_list[i])
```

**❌ DON'T:**
```vex
# Recalculate repeatedly
let total be 0
for item in list:
    total be total + len(list)  # Recalculates each time

# Nested expensive calls
for i in 0 to len(calculate_items()):  # Called each iteration
    ...
```

### String Operations

**✅ DO:**
```vex
# Build strings efficiently
let parts be ["Hello", " ", "World"]
let result be ""
for each part in parts:
    result be result + part

# Use string functions
let text be "hello world"
let upper be upper(text)
```

**❌ DON'T:**
```vex
# Inefficient concatenation in loops
let result be ""
for i in 0 to 10000:
    result be result + i + " "  # Slow!
```

---

## Testing

### Write Tests

**✅ DO:**
```vex
# test_calculator.vxm
use calculator

fn test_add():
    let result be add(2, 3)
    if result is 5:
        display "✓ add test passed"
    else:
        display "✗ add test failed"

fn test_divide_by_zero():
    let result be divide(10, 0)
    if result.success is false:
        display "✓ divide by zero handled"
    else:
        display "✗ should handle division by zero"

test_add()
test_divide_by_zero()
```

**❌ DON'T:**
```vex
# No tests - hope it works
# Manual testing - error prone
# Test only happy path - miss edge cases
```

### Test Edge Cases

**✅ DO:**
```vex
fn test_find_in_list():
    # Empty list
    if not find([],5):
        display "✓ handles empty list"
    
    # Single item
    if find([5], 5):
        display "✓ finds single item"
    
    # Not found
    if not find([1,2,3], 5):
        display "✓ returns false when not found"
    
    # Multiple occurrences
    if find([5, 5, 5], 5):
        display "✓ finds in list with duplicates"
```

**❌ DON'T:**
```vex
# Only test:
test_find([1, 2, 3], 2)  # Happy path only
```

---

## Documentation

### Comment When Necessary

**✅ DO:**
```vex
fn calculate_compound_interest(principal, rate, time):
    # Formula: A = P(1 + r/n)^(nt)
    # This uses annual compounding
    let compound_amount be principal * ((1 + rate) ** time)
    give back compound_amount

# Complex algorithm - explain why, not what
fn fibonacci(n):
    # Uses iterative approach instead of recursion
    # for O(n) performance instead of O(2^n)
    if n is less than 2:
        give back n
    ...
```

**❌ DON'T:**
```vex
# Over-commenting obvious code
fn add(a, b):
    # Add a to b
    let result be a + b
    # Return result
    give back result

# Lie in comments
fn process(data):
    # Returns success - actually returns null sometimes
    ...
```

### Document Functions

**✅ DO:**
```vex
# Calculate total price with tax
# 
# Parameters:
#   items: list of items with 'price' property
#   tax_rate: decimal tax rate (0.08 for 8%)
#
# Returns:
#   Float - total price including tax
#
# Example:
#   let total be calculate_total([{price: 100}, {price: 50}], 0.08)
fn calculate_total(items, tax_rate):
    let subtotal be 0
    for each item in items:
        subtotal be subtotal + item.price
    give back subtotal * (1 + tax_rate)
```

**❌ DON'T:**
```vex
fn calculate_total(items, tax_rate):
    # No documentation
    ...
```

### Update Documentation

**✅ DO:**
```vex
# Keep docs in sync with code
# If you change function signature, update docs
# If you change behavior, update comments
```

**❌ DON'T:**
```vex
# Stale documentation
# Function changed but docs didn't
# Comments describe old behavior
```

---

## Common Patterns

### Builder Pattern

```vex
struct QueryBuilder:
    has conditions
    has order_fields
    has limit_value

    can init():
        self.conditions be []
        self.order_fields be []
        self.limit_value be null

    can where(condition):
        push(self.conditions, condition)
        give back self

    can order_by(field):
        push(self.order_fields, field)
        give back self

    can limit(count):
        self.limit_value be count
        give back self

    can build():
        give back {"conditions": self.conditions, "order": self.order_fields, "limit": self.limit_value}

# Usage
let query be QueryBuilder()
    .where("age > 18")
    .where("status = 'active'")
    .order_by("name")
    .limit(10)
    .build()
```

### Strategy Pattern

```vex
struct PaymentProcessor:
    has strategy

    can init(pay_strategy):
        self.strategy be pay_strategy

    can process_payment(amount):
        give back self.strategy.process(amount)

struct CreditCardStrategy:
    can process(amount):
        # Credit card processing
        give back {"type": "credit_card", "amount": amount}

struct PayPalStrategy:
    can process(amount):
        # PayPal processing
        give back {"type": "paypal", "amount": amount}

# Usage
let processor be PaymentProcessor(CreditCardStrategy())
processor.process_payment(100)

processor.strategy be PayPalStrategy()
processor.process_payment(100)
```

### Factory Pattern

```vex
struct AnimalFactory:
    can create(animal_type):
        if animal_type is "dog":
            give back Dog()
        elif animal_type is "cat":
            give back Cat()
        elif animal_type is "bird":
            give back Bird()
        give back null

struct Dog:
    can speak():
        give back "Woof"

struct Cat:
    can speak():
        give back "Meow"

# Usage
let factory be AnimalFactory()
let dog be factory.create("dog")
display dog.speak()
```

### Validation Pattern

```vex
fn validate_user_input(email, password, name):
    let errors be []
    
    # Validate email
    if not is_valid_email(email):
        push(errors, "Email is invalid")
    
    # Validate password
    if len(password) is less than 8:
        push(errors, "Password must be 8+ characters")
    
    # Validate name
    if len(trim(name)) is 0:
        push(errors, "Name cannot be empty")
    
    if len(errors) is 0:
        give back {"success": true}
    give back {"success": false, "errors": errors}
```

---

## Code Review Checklist

Before submitting code, verify:

- [ ] **Naming** - Are names clear and consistent?
- [ ] **Functions** - Do they have single responsibility?
- [ ] **Comments** - Only where necessary, accurate?
- [ ] **Error Handling** - All failure cases handled?
- [ ] **Testing** - Are edge cases tested?
- [ ] **Performance** - Any unnecessary loops or calculations?
- [ ] **Constants** - Magic numbers extracted?
- [ ] **Documentation** - Functions documented?
- [ ] **Style** - Consistent formatting?
- [ ] **DRY** - No repeated code?

---

## Performance Checklist

- [ ] **Loops** - Are there unnecessary nested loops?
- [ ] **Calculations** - Are expensive calls in loops?
- [ ] **Caching** - Are results reused?
- [ ] **Strings** - Efficient concatenation?
- [ ] **Collections** - Right structure for task?
- [ ] **Conditionals** - Short-circuit evaluations?

---

## Security Considerations

### Input Validation

**✅ DO:**
```vex
fn process_user_input(input):
    # Always validate and sanitize
    let trimmed be trim(input)
    
    if is_empty(trimmed):
        give back {"error": "Input cannot be empty"}
    
    if len(trimmed) is greater than 1000:
        give back {"error": "Input too long"}
    
    give back safe_process(trimmed)
```

**❌ DON'T:**
```vex
fn process_user_input(input):
    # Trust user input - dangerous!
    direct_process(input)
```

---

## Summary

Good Vexium code is:
- **Clear** - Easy to understand
- **Maintainable** - Easy to modify
- **Tested** - Verified to work correctly
- **Documented** - Comments explain why, not what
- **Efficient** - Performs well
- **Safe** - Handles errors gracefully

Follow these practices and you'll write professional-quality code! 🚀

---

## Appendix: Inheritance & Composition (v1.0)

### ⚠️ Current Limitation

Vexium v1.0 **only supports single inheritance**. Multiple inheritance is coming in v2.0.

### Current Best Practice: Composition Over ???

Since multiple inheritance isn't available yet, **use composition with delegation**:

**✅ Good: Composition (Recommended)**
```vex
struct FeaturesExtractor:
    can extract(data):
        give back transform_features(data)

struct DataValidator:
    can validate(data):
        give back is_valid(data)

struct Logger:
    can log(message):
        display "[LOG] " + message

struct MLPipeline:
    has extractor
    has validator
    has logger
    
    can init():
        self.extractor be FeaturesExtractor()
        self.validator be DataValidator()
        self.logger be Logger()
    
    can execute(data):
        self.logger.log("Starting pipeline")
        
        let features be self.extractor.extract(data)
        self.logger.log("Extracted features")
        
        if self.validator.validate(features):
            self.logger.log("Validation passed")
            give back features
        
        self.logger.log("Validation failed")
        give back null
```

**See Also:** [MULTIPLE_INHERITANCE_PROPOSAL.md](MULTIPLE_INHERITANCE_PROPOSAL.md) for planned v2.0 features

---

## Summary

Good Vexium code is:
- **Clear** - Easy to understand
- **Maintainable** - Easy to modify
- **Tested** - Verified to work correctly
- **Documented** - Comments explain why, not what
- **Efficient** - Performs well
- **Safe** - Handles errors gracefully

Follow these practices and you'll write professional-quality code! 🚀
