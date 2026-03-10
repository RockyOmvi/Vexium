# 🏗️ Vexium Object-Oriented Programming Guide

Master structs, methods, and inheritance in Vexium.

---

## Table of Contents

1. [Structs (Classes)](#structs-classes)
2. [Constructors & Initialization](#constructors--initialization)
3. [Methods](#methods)
4. [Properties](#properties)
5. [Inheritance](#inheritance)
6. [Polymorphism](#polymorphism)
7. [Structs vs Maps](#structs-vs-maps)
8. [Design Patterns](#design-patterns)
9. [Best Practices](#best-practices)

---

## Structs (Classes)

### Basic Struct

```vex
struct Person:
    has name
    has age

# Create an instance
let alice be Person()
```

### Struct with Properties

```vex
struct Dog:
    has name
    has breed
    has age

let dog be Dog()
dog.name be "Rex"
dog.breed be "German Shepherd"
dog.age be 3

display dog.name              # "Rex"
display dog.breed             # "German Shepherd"
```

### Check Struct Type

```vex
struct Cat:
    has name

let cat be Cat()
display type(cat)             # "Cat"
```

---

## Constructors & Initialization

### Constructor Method (init)

```vex
struct Point:
    has x
    has y

    can init(x, y):
        self.x be x
        self.y be y

let p be Point(3, 4)
display p.x                   # 3
display p.y                   # 4
```

### Constructor with Default Values

```vex
struct Rectangle:
    has width
    has height
    has color

    can init(w be 10, h be 10, color be "black"):
        self.width be w
        self.height be h
        self.color be color

let r1 be Rectangle()
display r1.width              # 10

let r2 be Rectangle(20, 30, "red")
display r2.color              # "red"
```

### Multiple Constructors (Workaround)

```vex
struct Date:
    has day
    has month
    has year

    can init(day, month, year):
        self.day be day
        self.month be month
        self.year be year

# Alternative constructor - factory function
fn Date_today():
    let d be Date(0, 0, 0)
    # Set to today's values
    d.day be 4
    d.month be 3
    d.year be 2026
    give back d

let today be Date_today()
```

---

## Methods

### Instance Methods

```vex
struct Circle:
    has radius

    can init(r):
        self.radius be r

    can area():
        give back 3.14159 * self.radius * self.radius

    can circumference():
        give back 2 * 3.14159 * self.radius

    can display_info():
        display "Radius: " + str(self.radius)
        display "Area: " + str(self.area())
        display "Circumference: " + str(self.circumference())

let c be Circle(5)
display c.area()              # 78.54975
display c.circumference()     # 31.4159
c.display_info()
```

### Methods with Parameters

```vex
struct Vector:
    has x
    has y

    can init(x, y):
        self.x be x
        self.y be y

    can add(other):
        give back Vector(self.x + other.x, self.y + other.y)

    can distance_to(other):
        let dx be other.x - self.x
        let dy be other.y - self.y
        give back sqrt(dx * dx + dy * dy)

    can display():
        display "(" + str(self.x) + ", " + str(self.y) + ")"

let v1 be Vector(3, 4)
let v2 be Vector(1, 2)

let v3 be v1.add(v2)
v3.display()                  # (4, 6)

display v1.distance_to(v2)    # 2.236...
```

### Methods Returning Self (Chaining)

```vex
struct StringBuilder:
    has text

    can init(initial be ""):
        self.text be initial
        give back self

    can append(str):
        self.text be self.text + str
        give back self

    can add_newline():
        self.text be self.text + "\n"
        give back self

    can get():
        give back self.text

let sb be StringBuilder("Hello")
let result be sb.append(" ").append("World").add_newline().get()
display result
```

---

## Properties

### Reading Properties

```vex
struct Car:
    has brand
    has model
    has year

    can init(brand, model, year):
        self.brand be brand
        self.model be model
        self.year be year

    can full_name():
        give back self.year + " " + self.brand + " " + self.model

let car be Car("Toyota", "Camry", 2020)
display car.brand             # "Toyota"
display car.full_name()       # "2020 Toyota Camry"
```

### Computed Properties (Methods)

```vex
struct BankAccount:
    has balance
    has interest_rate

    can init(initial, rate):
        self.balance be initial
        self.interest_rate be rate

    can interest_earned():
        # Computed property
        give back self.balance * self.interest_rate

    can projected_balance(years):
        let total be self.balance
        for i in 1 to years + 1:
            total be total * (1 + self.interest_rate)
        give back total

let account be BankAccount(1000, 0.05)
display account.interest_earned()    # 50
display account.projected_balance(5) # ~1276.28
```

---

## Inheritance

### Basic Inheritance

```vex
struct Animal:
    has name
    has sound

    can init(name, sound):
        self.name be name
        self.sound be sound

    can speak():
        give back self.name + " says " + self.sound

struct Dog extends Animal:
    has breed

let dog be Dog()
dog.name be "Rex"
dog.sound be "Woof"
dog.breed be "Labrador"

display dog.speak()           # "Rex says Woof"
display dog.breed             # "Labrador"
```

### Overriding Methods

```vex
struct Vehicle:
    has make
    has model

    can init(make, model):
        self.make be make
        self.model be model

    can info():
        give back self.make + " " + self.model

struct ElectricCar extends Vehicle:
    has battery_capacity

    can info():
        # Override parent's info()
        give back Vehicle.info(self) + " (Electric)"

struct SportsCar extends Vehicle:
    has horse_power

    can info():
        give back Vehicle.info(self) + " (Sports)"

let ev be ElectricCar()
ev.make be "Tesla"
ev.model be "Model 3"
ev.battery_capacity be 75
display ev.info()             # "Tesla Model 3 (Electric)"
```

### Calling Parent Methods

```vex
struct Employee:
    has name
    has salary

    can init(name, salary):
        self.name be name
        self.salary be salary

    can description():
        give back "Employee: " + self.name

struct Manager extends Employee:
    has team_size

    can init(name, salary, team_size):
        self.name be name
        self.salary be salary
        self.team_size be team_size

    can description():
        let base be Employee.description(self)
        give back base + ", managing " + str(self.team_size) + " people"

let mgr be Manager("Alice", 100000, 5)
display mgr.description()
# "Employee: Alice, managing 5 people"
```

### Multi-Level Inheritance

```vex
struct LivingThing:
    has name

struct Animal extends LivingThing:
    has sound

struct Mammal extends Animal:
    has warm_blooded

struct Dog extends Mammal:
    has breed

let dog be Dog()
dog.name be "Rex"
dog.sound be "Woof"
dog.warm_blooded be true
dog.breed be "Labrador"

# Inherits all properties and methods
```

### ⚠️ Limitation: Single Inheritance Only (For Now)

**Current Status:** Vexium v1.0 only supports single inheritance (one parent).

**What doesn't work yet:**
```vex
# Multiple inheritance NOT supported in v1.0
struct SearchEngine extends Indexable, Rankable:
    ...
```

**Why This Matters for AI Builders:**
Multiple inheritance is essential for AI/ML/data science workflows:
- **Mixins** - Composing reusable behaviors (e.g., LoggableMixin, SerializableMixin)
- **Data pipelines** - Combining processors (Reader, Transformer, Validator)
- **ML frameworks** - Composing model layers with task-specific behaviors
- **Interface composition** - Combining abstract behaviors

**Current Workaround: Composition with Delegation**

Instead of multiple inheritance, compose behaviors:

```vex
struct Indexable:
    can build_index(data):
        display "Building index"

struct Rankable:
    can rank_results(results):
        display "Ranking results"

struct Cacheable:
    can cache_result(key, value):
        display "Caching: " + key

# Compose instead of inherit
struct SearchEngine:
    has indexer
    has ranker
    has cache
    
    can init():
        self.indexer be Indexable()
        self.ranker be Rankable()
        self.cache be Cacheable()
    
    can search(query):
        self.indexer.build_index([])
        display "Searching for: " + query
        let results be [1, 2, 3]
        let ranked be self.ranker.rank_results(results)
        self.cache.cache_result(query, ranked)
        give back ranked

let engine be SearchEngine()
engine.search("machine learning")
```

**Better Alternative: Struct Composition Pattern**

```vex
# For AI builders: Create behavior structs
struct DataPreprocessor:
    can preprocess(data):
        give back transform_data(data)

struct FeatureExtractor:
    can extract(data):
        give back extract_features(data)

struct ModelValidator:
    can validate(model, data):
        give back is_valid(model)

# Compose all behaviors in ML pipeline
struct MLPipeline:
    has preprocessor
    has extractor
    has validator
    
    can init():
        self.preprocessor be DataPreprocessor()
        self.extractor be FeatureExtractor()
        self.validator be ModelValidator()
    
    can execute(raw_data):
        let processed be self.preprocessor.preprocess(raw_data)
        let features be self.extractor.extract(processed)
        let valid be self.validator.validate(features)
        
        if valid:
            give back features
        give back null
```

**See also:** [MULTIPLE_INHERITANCE_PROPOSAL.md](MULTIPLE_INHERITANCE_PROPOSAL.md) for planned v2.0 enhancements and real-world examples.

---

## Polymorphism

### Treating Different Types Uniformly

```vex
struct Shape:
    can area():
        give back 0

struct Rectangle extends Shape:
    has width
    has height

    can area():
        give back self.width * self.height

struct Circle extends Shape:
    has radius

    can area():
        give back 3.14159 * self.radius * self.radius

struct Triangle extends Shape:
    has base
    has height

    can area():
        give back self.base * self.height / 2

# Process different types
let rect be Rectangle()
rect.width be 5
rect.height be 10

let circle be Circle()
circle.radius be 5

let triangle be Triangle()
triangle.base be 10
triangle.height be 6

let shapes be [rect, circle, triangle]

let total_area be 0
for each shape in shapes:
    total_area be total_area + shape.area()

display total_area  # 50 + 78.54975 + 30 = 158.54975
```

---

## Structs vs Maps

### Struct Advantages

```vex
# With Struct
struct User:
    has id
    has username
    has email

    can init(id, username, email):
        self.id be id
        self.username be username
        self.email be email

    can display_profile():
        display "User: " + self.username

let user be User(1, "alice", "alice@example.com")
user.display_profile()        # Methods available
```

### Map Advantages

```vex
# With Map (for unstructured data)
let user be {
    "id": 1,
    "username": "alice",
    "email": "alice@example.com",
    "settings": {
        "theme": "dark",
        "language": "en"
    }
}

display user.settings.theme   # Easy nested access
```

### When to Use Which

```vex
# Use Structs when:
# - Data has specific structure
# - Need methods/behavior
# - Want type safety
struct Product:
    has id
    has name
    has price

# Use Maps when:
# - Data is dynamic/unstructured
# - Have varying fields
# - Just storing data
let config be {
    "debug": true,
    "api_url": "https://api.example.com",
    "features": ["auth", "payments"]
}
```

---

## Design Patterns

### Builder Pattern

```vex
struct QueryBuilder:
    has where_clause
    has order_by
    has limit_val

    can init():
        self.where_clause be ""
        self.order_by be ""
        self.limit_val be nothing

    can where(condition):
        self.where_clause be condition
        give back self

    can order_by_field(field):
        self.order_by be field
        give back self

    can limit(count):
        self.limit_val be count
        give back self

    can build():
        let query be "SELECT * "
        if self.where_clause is not "":
            query be query + "WHERE " + self.where_clause + " "
        if self.order_by is not "":
            query be query + "ORDER BY " + self.order_by + " "
        if self.limit_val is not nothing:
            query be query + "LIMIT " + str(self.limit_val)
        give back query

let query be QueryBuilder()
    .where("age > 18")
    .order_by_field("name")
    .limit(10)
    .build()

display query
# SELECT * WHERE age > 18 ORDER BY name LIMIT 10
```

### Strategy Pattern

```vex
struct PaymentProcessor:
    has strategy

    can init(strategy):
        self.strategy be strategy

    can process(amount):
        give back self.strategy(amount)

# Strategies
let credit_card be (amount) => {
    give back "Processing " + str(amount) + " via Credit Card"
}

let paypal be (amount) => {
    give back "Processing " + str(amount) + " via PayPal"
}

let bitcoin be (amount) => {
    give back "Processing " + str(amount) + " via Bitcoin"
}

# Using different strategies
let cc_processor be PaymentProcessor(credit_card)
display cc_processor.process(100)     # Credit Card

let paypal_processor be PaymentProcessor(paypal)
display paypal_processor.process(50)  # PayPal
```

### Factory Pattern

```vex
struct Animal:
    has type

struct Dog extends Animal:
    can init():
        self.type be "Dog"

struct Cat extends Animal:
    can init():
        self.type be "Cat"

fn AnimalFactory(animal_type):
    if animal_type is "dog":
        give back Dog()
    elif animal_type is "cat":
        give back Cat()
    give back nothing

let pet1 be AnimalFactory("dog")
let pet2 be AnimalFactory("cat")

display pet1.type             # "Dog"
display pet2.type             # "Cat"
```

---

## Best Practices

### 1. Use Constructors

```vex
# ✅ Good
struct Person:
    has name
    has age

    can init(name, age):
        self.name be name
        self.age be age

let alice be Person("Alice", 30)

# ❌ Avoid
let bob be Person()
bob.name be "Bob"
bob.age be 30
# Properties uninitialized until set
```

### 2. Encapsulation with Methods

```vex
# ✅ Good: Control access through methods
struct BankAccount:
    has balance

    can init(initial):
        self.balance be initial

    can deposit(amount):
        if amount is greater than 0:
            self.balance be self.balance + amount
            give back true
        give back false

    can withdraw(amount):
        if amount is greater than 0 and amount is at most self.balance:
            self.balance be self.balance - amount
            give back true
        give back false

    can get_balance():
        give back self.balance

# ❌ Avoid: Direct property manipulation
# account.balance be -1000  # Invalid state!
```

### 3. Single Responsibility

```vex
# ✅ Good: Separate concerns
struct User:
    has name
    has email

    can init(name, email):
        self.name be name
        self.email be email

struct UserValidator:
    can is_valid_email(email):
        give back email contains "@"

struct UserRepository:
    can save(user):
        # Save to database
        display "Saving user: " + user.name

# ❌ Avoid: User does too much
struct UserBad:
    has name
    has email

    can validate():
        give back self.email contains "@"

    can save_to_database():
        # Mixing validation and persistence
```

### 4. Use Inheritance Properly

```vex
# ✅ Good: "is-a" relationship
struct Animal:
    has name

struct Dog extends Animal:
    # Dog IS-AN Animal
    has breed

# ❌ Avoid: "has-a" modeled as inheritance
# struct Dog extends FurryThing:
#     # This is more of a "has-a" relationship
#     # Better: has furliness: 0-100
```

### 5. Document Complex Classes

```vex
struct LinkedListNode:
    # Represents a node in a singly-linked list
    has value
    has next

    can init(val):
        self.value be val
        self.next be nothing

    can append(node):
        # Add node to end of chain
        if self.next is nothing:
            self.next be node
        else:
            self.next.append(node)

    can length():
        # Count nodes from this point
        if self.next is nothing:
            give back 1
        give back 1 + self.next.length()
```

---

This guide covers OOP comprehensively. For more on other topics, check the other documentation files.
