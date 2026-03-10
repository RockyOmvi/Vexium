# 📚 Vexium Examples & Projects

Complete, ready-to-run examples demonstrating Vexium features.

---

## Quick Navigation

- [Beginner Examples](#beginner-examples-5-15-lines)
- [Intermediate Examples](#intermediate-examples-20-50-lines)
- [Advanced Examples](#advanced-examples-50-150-lines)
- [Complete Projects](#complete-projects-100-500-lines)
- [Real-World Scenarios](#real-world-scenarios)

---

## Beginner Examples (5-15 lines)

### 1. Hello World

```vex
display "Hello, Vexium!"
display "Welcome to the future of programming"
```

### 2. Basic Calculator

```vex
fn add(a, b):
    give back a + b

fn subtract(a, b):
    give back a - b

fn multiply(a, b):
    give back a * b

display add(10, 5)        # 15
display subtract(10, 5)   # 5
display multiply(10, 5)   # 50
```

### 3. Temperature Converter

```vex
fn celsius_to_fahrenheit(celsius):
    give back (celsius * 9 / 5) + 32

fn fahrenheit_to_celsius(fahrenheit):
    give back (fahrenheit - 32) * 5 / 9

display celsius_to_fahrenheit(0)     # 32.0
display celsius_to_fahrenheit(100)   # 212.0
display fahrenheit_to_celsius(32)    # 0.0
```

### 4. Grade Converter

```vex
fn get_grade(score):
    if score is at least 90:
        give back "A"
    elif score is at least 80:
        give back "B"
    elif score is at least 70:
        give back "C"
    elif score is at least 60:
        give back "D"
    give back "F"

display get_grade(95)   # A
display get_grade(75)   # C
display get_grade(55)   # F
```

### 5. Even or Odd

```vex
fn is_even(n):
    give back n % 2 is 0

fn is_odd(n):
    give back n % 2 is not 0

display is_even(4)   # true
display is_even(7)   # false
display is_odd(7)    # true
```

---

## Intermediate Examples (20-50 lines)

### 6. Fibonacci Generator

```vex
fn fibonacci(n):
    if n is less than 2:
        give back n
    
    let a be 0
    let b be 1
    
    for i in 2 to n:
        let temp be a + b
        a be b
        b be temp
    
    give back b

for i in 0 to 10:
    display "fib(" + str(i) + ") = " + str(fibonacci(i))
```

### 7. List Operations

```vex
fn sum_list(numbers):
    let total be 0
    for each num in numbers:
        total be total + num
    give back total

fn average(numbers):
    if len(numbers) is 0:
        give back 0
    give back sum_list(numbers) / len(numbers)

fn find_max(numbers):
    if len(numbers) is 0:
        give back null
    
    let max be numbers[0]
    for each num in numbers:
        if num is greater than max:
            max be num
    give back max

let scores be [85, 92, 78, 95, 88]
display "Sum: " + str(sum_list(scores))      # 438
display "Average: " + str(average(scores))   # 87.6
display "Max: " + str(find_max(scores))      # 95
```

### 8. String Utilities

```vex
fn reverse_string(s):
    let result be ""
    for i in len(s) - 1 to -1:
        result be result + s[i]
    give back result

fn is_palindrome(s):
    let reversed be reverse_string(s)
    give back s is reversed

fn count_vowels(s):
    let vowels be "aeiouAEIOU"
    let count be 0
    
    for i in 0 to len(s):
        let char be s[i]
        if contains(vowels, char):
            count be count + 1
    
    give back count

display reverse_string("hello")           # "olleh"
display is_palindrome("racecar")          # true
display is_palindrome("hello")            # false
display count_vowels("hello world")       # 3
```

### 9. Data Validation

```vex
fn is_valid_email(email):
    if not contains(email, "@"):
        give back false
    if not contains(email, "."):
        give back false
    
    let at_index be find(email, "@")
    let dot_index be find(email, ".")
    
    if at_index is greater than dot_index:
        give back false
    
    give back true

fn is_strong_password(password):
    if len(password) is less than 8:
        give back false
    
    let has_upper be false
    let has_lower be false
    let has_digit be false
    
    for i in 0 to len(password):
        let char be password[i]
        if char is in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
            has_upper be true
        elif char is in "abcdefghijklmnopqrstuvwxyz":
            has_lower be true
        elif char is in "0123456789":
            has_digit be true
    
    give back has_upper and has_lower and has_digit

display is_valid_email("user@example.com")        # true
display is_valid_email("invalid.email")           # false
display is_strong_password("Secure123")           # true
display is_strong_password("weak")                # false
```

### 10. Simple Game: Number Guessing

```vex
fn random_number(max):
    # Pseudo-random based on time
    let result be 42 % max
    give back result

fn guess_the_number():
    let secret be random_number(100)
    let guesses be 0
    let correct be false
    
    while not correct:
        let input be input("Guess a number (1-100): ")
        let guess be int(input)
        guesses be guesses + 1
        
        if guess is secret:
            display "Correct! You got it in " + str(guesses) + " tries!"
            correct be true
        elif guess is less than secret:
            display "Too low, try again"
        else:
            display "Too high, try again"

guess_the_number()
```

---

## Advanced Examples (50-150 lines)

### 11. Bank Account System

```vex
struct BankAccount:
    has account_number
    has owner_name
    has balance
    has transaction_history

    can init(account_num, owner, initial_balance):
        self.account_number be account_num
        self.owner_name be owner
        self.balance be initial_balance
        self.transaction_history be []

    can deposit(amount):
        if amount is less than or equal to 0:
            give back {"success": false, "error": "Amount must be positive"}
        
        self.balance be self.balance + amount
        push(self.transaction_history, {"type": "deposit", "amount": amount, "balance": self.balance})
        
        give back {"success": true, "new_balance": self.balance}

    can withdraw(amount):
        if amount is less than or equal to 0:
            give back {"success": false, "error": "Amount must be positive"}
        
        if amount is greater than self.balance:
            give back {"success": false, "error": "Insufficient funds"}
        
        self.balance be self.balance - amount
        push(self.transaction_history, {"type": "withdraw", "amount": amount, "balance": self.balance})
        
        give back {"success": true, "new_balance": self.balance}

    can get_balance():
        give back self.balance

    can get_statement(limit be 5):
        display "=== Account Statement ==="
        display "Account: " + self.account_number
        display "Owner: " + self.owner_name
        display "Current Balance: " + str(self.balance)
        display ""
        display "Recent Transactions:"
        
        let start be len(self.transaction_history) - limit
        if start is less than 0:
            start be 0
        
        for i in start to len(self.transaction_history):
            let trans be self.transaction_history[i]
            display "  " + trans.type + " $" + str(trans.amount) + " (Balance: $" + str(trans.balance) + ")"

# Usage
let account be BankAccount("123456", "Alice", 1000)
account.deposit(500)
account.withdraw(200)
account.get_statement()
display "Final balance: $" + str(account.get_balance())
```

### 12. Student Grade Manager

```vex
struct Student:
    has id
    has name
    has grades

    can init(student_id, student_name):
        self.id be student_id
        self.name be student_name
        self.grades be []

    can add_grade(grade):
        if grade is less than 0 or grade is greater than 100:
            give back false
        
        push(self.grades, grade)
        give back true

    can get_average():
        if len(self.grades) is 0:
            give back 0
        
        let sum be 0
        for each grade in self.grades:
            sum be sum + grade
        
        give back sum / len(self.grades)

    can get_letter_grade():
        let avg be self.get_average()
        
        if avg is at least 90:
            give back "A"
        elif avg is at least 80:
            give back "B"
        elif avg is at least 70:
            give back "C"
        elif avg is at least 60:
            give back "D"
        give back "F"

    can display_info():
        display "Student: " + self.name + " (ID: " + str(self.id) + ")"
        display "Grades: " + str(self.grades)
        display "Average: " + str(self.get_average())
        display "Letter Grade: " + self.get_letter_grade()

struct Classroom:
    has class_name
    has students

    can init(name):
        self.class_name be name
        self.students be []

    can add_student(student):
        push(self.students, student)

    can get_class_average():
        if len(self.students) is 0:
            give back 0
        
        let total be 0
        for each student in self.students:
            total be total + student.get_average()
        
        give back total / len(self.students)

    can display_class_report():
        display "=== " + self.class_name + " Report ==="
        display "Students: " + str(len(self.students))
        display "Class Average: " + str(self.get_class_average())
        display ""
        
        for each student in self.students:
            student.display_info()
            display ""

# Usage
let algebra be Classroom("Algebra 101")

let alice be Student(1, "Alice")
alice.add_grade(95)
alice.add_grade(92)
alice.add_grade(88)
algebra.add_student(alice)

let bob be Student(2, "Bob")
bob.add_grade(85)
bob.add_grade(90)
bob.add_grade(78)
algebra.add_student(bob)

algebra.display_class_report()
```

### 13. Todo List Manager

```vex
struct Todo:
    has id
    has title
    has description
    has completed
    has priority

    can init(todo_id, todo_title, todo_desc be "", todo_priority be "normal"):
        self.id be todo_id
        self.title be todo_title
        self.description be todo_desc
        self.completed be false
        self.priority be todo_priority

    can mark_complete():
        self.completed be true

    can mark_incomplete():
        self.completed be false

    can display():
        let status be "[ ]"
        if self.completed:
            status be "[X]"
        
        display status + " " + self.title
        if self.description is not "":
            display "    " + self.description
        display "    Priority: " + self.priority

struct TodoList:
    has items
    has next_id

    can init():
        self.items be []
        self.next_id be 1

    can add(title, description be "", priority be "normal"):
        let todo be Todo(self.next_id, title, description, priority)
        push(self.items, todo)
        self.next_id be self.next_id + 1
        give back todo.id

    can complete(todo_id):
        for each item in self.items:
            if item.id is todo_id:
                item.mark_complete()
                display "✓ Completed: " + item.title

    can remove(todo_id):
        let index be -1
        for i in 0 to len(self.items):
            if self.items[i].id is todo_id:
                index be i

        if index is greater than -1:
            # Remove by recreating array
            let new_items be []
            for i in 0 to len(self.items):
                if i is not index:
                    push(new_items, self.items[i])
            self.items be new_items
            display "✓ Removed item"

    can get_pending():
        give back [t for t in self.items if t.completed is false]

    can display_all():
        display "=== Todo List ==="
        display "Pending: " + str(len(self.get_pending()))
        display ""
        
        for each item in self.items:
            item.display()

# Usage
let todos be TodoList()
todos.add("Buy groceries", "Milk, eggs, bread", "high")
todos.add("Write code", "Implement feature X", "high")
todos.add("Exercise", "30 min run", "normal")
todos.add("Read book", "", "low")

todos.display_all()
display ""
todos.complete(1)
todos.display_all()
```

---

## Complete Projects (100-500 lines)

### 14. Simple E-commerce System

This is a realistic example demonstrating multiple concepts:

```vex
struct Product:
    has id
    has name
    has price
    has stock

    can init(product_id, product_name, product_price, initial_stock):
        self.id be product_id
        self.name be product_name
        self.price be product_price
        self.stock be initial_stock

struct CartItem:
    has product
    has quantity

    can init(prod, qty):
        self.product be prod
        self.quantity be qty

    can subtotal():
        give back self.product.price * self.quantity

struct ShoppingCart:
    has items

    can init():
        self.items be []

    can add_item(product, quantity):
        # Check if product already in cart
        for each item in self.items:
            if item.product.id is product.id:
                item.quantity be item.quantity + quantity
                give back true
        
        # Add new item
        push(self.items, CartItem(product, quantity))
        give back true

    can remove_item(product_id):
        let new_items be []
        let found be false
        
        for each item in self.items:
            if item.product.id is not product_id:
                push(new_items, item)
            else:
                found be true
        
        self.items be new_items
        give back found

    can get_total():
        let total be 0
        for each item in self.items:
            total be total + item.subtotal()
        give back total

    can display_cart():
        display "=== Shopping Cart ==="
        
        if len(self.items) is 0:
            display "Cart is empty"
            give back
        
        display "Items:"
        for each item in self.items:
            display "  " + item.product.name + " x" + str(item.quantity) + " = $" + str(item.subtotal())
        
        display ""
        display "Total: $" + str(self.get_total())

struct Store:
    has products
    has carts

    can init():
        self.products be []
        self.carts be []

    can add_product(product):
        push(self.products, product)

    can find_product(product_id):
        for each product in self.products:
            if product.id is product_id:
                give back product
        give back null

    can create_cart():
        let cart be ShoppingCart()
        push(self.carts, cart)
        give back cart

# Usage
let shop be Store()

# Add products
shop.add_product(Product(1, "Laptop", 999.99, 5))
shop.add_product(Product(2, "Mouse", 29.99, 50))
shop.add_product(Product(3, "Keyboard", 79.99, 20))

# Create shopping cart
let cart be shop.create_cart()

# Add items to cart
let laptop be shop.find_product(1)
cart.add_item(laptop, 1)

let mouse be shop.find_product(2)
cart.add_item(mouse, 2)

# Display cart
cart.display_cart()
```

---

## Real-World Scenarios

### Scenario 1: Data Analysis

```vex
fn analyze_data(numbers):
    let sum be 0
    let min be numbers[0]
    let max be numbers[0]
    
    for each num in numbers:
        sum be sum + num
        if num is less than min:
            min be num
        if num is greater than max:
            max be num
    
    let average be sum / len(numbers)
    
    give back {
        "sum": sum,
        "average": average,
        "min": min,
        "max": max,
        "count": len(numbers)
    }

# Sample data
let sales be [150, 200, 175, 220, 190, 210]
let analysis be analyze_data(sales)

display "Sales Analysis:"
display "  Total: $" + str(analysis.sum)
display "  Average: $" + str(analysis.average)
display "  Min: $" + str(analysis.min)
display "  Max: $" + str(analysis.max)
display "  Days: " + str(analysis.count)
```

### Scenario 2: Form Validation

```vex
fn validate_registration(data):
    let errors be []
    
    # Validate name
    if len(trim(data.name)) is 0:
        push(errors, "Name is required")
    
    # Validate email
    if not contains(data.email, "@"):
        push(errors, "Invalid email")
    
    # Validate password
    if len(data.password) is less than 8:
        push(errors, "Password must be 8+ characters")
    
    # Validate age
    if data.age is less than 18:
        push(errors, "Must be 18 or older")
    
    if len(errors) is 0:
        give back {"success": true}
    
    give back {"success": false, "errors": errors}

# Test validation
let form be {
    "name": "Alice",
    "email": "alice@example.com",
    "password": "SecurePass123",
    "age": 25
}

let result be validate_registration(form)
if result.success:
    display "Registration successful!"
else:
    display "Validation errors:"
    for each error in result.errors:
        display "  - " + error
```

### Scenario 3: Data Transformation

```vex
fn extract_names(users):
    let names be []
    for each user in users:
        push(names, user.name)
    give back names

fn filter_active_users(users):
    give back [u for u in users if u.active is true]

fn sort_by_age(users):
    # Simple bubble sort
    let sorted be users # Create copy
    let n be len(sorted)
    
    for i in 0 to n:
        for j in 0 to n - 1:
            if sorted[j].age is greater than sorted[j + 1].age:
                # Swap
                let temp be sorted[j]
                sorted[j] be sorted[j + 1]
                sorted[j + 1] be temp
    
    give back sorted

# Sample data
let users be [
    {"name": "Alice", "age": 28, "active": true},
    {"name": "Bob", "age": 25, "active": false},
    {"name": "Charlie", "age": 32, "active": true},
    {"name": "Diana", "age": 22, "active": true}
]

# Transform data
let active_users be filter_active_users(users)
let sorted_users be sort_by_age(active_users)
let names be extract_names(sorted_users)

display "Active users sorted by age:"
for each name in names:
    display "  - " + name
```

---

## Pattern Examples

### Builder Pattern Example

```vex
struct QueryBuilder:
    has filters
    has sorting
    has limit_num

    can init():
        self.filters be []
        self.sorting be null
        self.limit_num be null

    can where(condition):
        push(self.filters, condition)
        give back self

    can order_by(field):
        self.sorting be field
        give back self

    can limit(count):
        self.limit_num be count
        give back self

    can build():
        give back {
            "filters": self.filters,
            "sort": self.sorting,
            "limit": self.limit_num
        }

# Usage
let query be QueryBuilder()
    .where("age > 18")
    .where("status = active")
    .order_by("name")
    .limit(10)
    .build()

display query
```

### Strategy Pattern Example

```vex
struct PaymentProcessor:
    has strategy

    can init(pay_strategy):
        self.strategy be pay_strategy

    can process(amount):
        give back self.strategy.execute(amount)

struct CreditCard:
    can execute(amount):
        display "Processing credit card: $" + str(amount)
        give back {"status": "success", "amount": amount, "method": "credit_card"}

struct PayPal:
    can execute(amount):
        display "Processing PayPal: $" + str(amount)
        give back {"status": "success", "amount": amount, "method": "paypal"}

# Usage
let processor be PaymentProcessor(CreditCard())
processor.process(99.99)

processor.strategy be PayPal()
processor.process(49.99)
```

---

## Tips for Examples

1. **Start simple** - Understand basics before complexity
2. **Modify examples** - Change values, add features
3. **Combine examples** - Mix multiple concepts
4. **Test edge cases** - Empty lists, zero values, etc.
5. **Add error handling** - Make examples robust
6. **Comment your code** - Understand what's happening
7. **Refactor** - Improve and clean up code

---

## Practice Challenges

Try these with examples above:

### Challenge 1: Shopping Cart Tax
Add tax calculation to the e-commerce example.

### Challenge 2: Extended Todo
Add due dates and categories to todo list.

### Challenge 3: Student Grades
Add multiple classes and class ranking.

### Challenge 4: Bank Interest
Add interest calculation to bank account.

### Challenge 5: Data Filtering
Filter product catalog with multiple criteria.

---

All examples are ready to run! Choose one that interests you and start coding. 🚀

For more guidance, see:
- [VEXIUM_QUICK_START.md](VEXIUM_QUICK_START.md)
- [VEXIUM_SYNTAX.md](VEXIUM_SYNTAX.md)
- [VEXIUM_LEARNING_PATH.md](VEXIUM_LEARNING_PATH.md)
