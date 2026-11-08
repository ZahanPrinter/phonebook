# Phone Book CLI

A command-line contact manager written in C++17. Every contact is indexed in three **custom-built data structures** (no `std::unordered_map` for the main index, no `std::map`), each chosen for the kind of query it answers fastest:

| Query | Structure | Complexity |
|---|---|---|
| Look up a contact by exact phone number | Hash table (separate chaining) | O(1) average |
| Find all names starting with a prefix | Trie | O(L + k) |
| List everyone alphabetically / names in a range | Binary search tree | O(n) / O(h + k) |

*L = length of the prefix, k = number of matches, h = height of the tree, n = number of contacts.*

## Features

- Add, update, delete and look up contacts
- Prefix search by name (autocomplete-style, case-insensitive)
- Alphabetical listing and name-range queries (e.g. everyone from "Dan" to "Priya")
- Automatic CSV save on exit, automatic load on startup
- Hash table resizes itself (doubles at 0.75 load factor) as the book grows

## How it works

`PhoneBook` is the one class the CLI talks to. It owns each contact as a `shared_ptr<Contact>` and hands that same pointer to all three structures, so the data exists once but can be reached three ways. Every add, update or delete goes through `PhoneBook`, which keeps the three indexes in sync.

**Hash table** (`HashTable.cpp`): the phone number is hashed with a polynomial rolling hash (base 31) and reduced modulo the bucket count. Collisions are handled by separate chaining, where each bucket is a singly linked list. When the load factor passes 0.75 the bucket array doubles and every entry is rehashed.

**Trie** (`Trie.cpp`): names are lowercased and inserted one character per node. A prefix search walks down to the node for the prefix, then collects every contact stored beneath it. Duplicate names are supported because each end-of-name node holds a list of contacts.

**Binary search tree** (`BST.cpp`): keyed by lowercase name. An in-order traversal yields contacts alphabetically. Range queries skip subtrees that cannot contain matches. Deletion handles the leaf, one-child and two-child cases (the two-child case uses the in-order successor).

## Project structure

```
phonebook/
├── .vscode/            # VS Code build, debug and IntelliSense config
│   ├── tasks.json
│   ├── launch.json
│   ├── c_cpp_properties.json
│   └── settings.json
├── include/
│   ├── Contact.h       # plain struct: name, phone, email, address
│   ├── HashTable.h
│   ├── Trie.h
│   ├── BST.h
│   └── PhoneBook.h     # ties the three structures together
├── src/
│   ├── HashTable.cpp
│   ├── Trie.cpp
│   ├── BST.cpp
│   ├── PhoneBook.cpp   # add / update / remove / search / save / load
│   └── main.cpp        # menu-driven CLI
├── bench.cpp           # benchmark harness (see Benchmarks)
├── contacts.csv        # sample data: 50,000 contacts
└── README.md
```

## Building and running

Requires a C++17 compiler (tested with g++ from MSYS2/MinGW-w64).

From the project root:

```bash
g++ -std=c++17 -O2 -Iinclude src/main.cpp src/PhoneBook.cpp src/HashTable.cpp src/Trie.cpp src/BST.cpp -o phonebook
./phonebook
```

On Windows the output will be `phonebook.exe`; run it with `.\phonebook.exe`.

**Run from the project root.** Contacts are saved to and loaded from `contacts.csv` in the *current working directory*, so running from another folder will create or read a different file.

Keep `-O2` on. Without optimization, loading large contact files is noticeably slower.

### Building in VS Code

The `.vscode/` folder is included so you can see how the project is compiled and linked. Press **F5** (Run and Debug) to build and launch under the debugger.

- **`tasks.json`** is the build step. It runs `g++` once with every `.cpp` file listed explicitly. Each `.cpp` is compiled into its own object, then the linker joins them into a single `phonebook.exe`. Compiling only the open file (the editor's default "Run C/C++ File" button) fails here because `main.cpp` calls code that lives in the other files.
  - `-I${workspaceFolder}/include` tells the compiler where to find the headers
  - `-std=c++17` selects the language standard
  - `-g` adds debug symbols so breakpoints and `gdb` work
  - `-O2` turns on optimization
- **`launch.json`** starts the built exe under `gdb`, runs `tasks.json` first, and sets the working directory to the project root so `contacts.csv` lands in the right place.
- **`c_cpp_properties.json`** and **`settings.json`** only configure IntelliSense (squiggles and autocomplete). They do not affect the real build.

The compiler and debugger paths in these files point to an MSYS2 UCRT64 install (`C:/msys64/ucrt64/bin/`). If your toolchain lives elsewhere, edit those paths first.

## Usage

```
===== Phone Book (Hash Table + Trie + BST) =====
1. Add contact
2. Look up by phone number
3. Search by name prefix
4. List all contacts (sorted)
5. List contacts in a name range
6. Update contact
7. Delete contact
8. Save to file
9. Exit
```

Phone number is the unique key: adding a second contact with an existing number is rejected, and updating a contact looks it up by number.

### Data file format

`contacts.csv` has one contact per line with no header:

```
Zahan,1234567890,zahan@example.com,12 Main St
Priya Rao,555-0104,,
```

Fields are split on commas, so **commas inside a field are not supported**. Email and address may be empty.

## Benchmarks

`bench.cpp` loads `contacts.csv` (50,000 contacts) into a `PhoneBook` and times each query type. Hash lookups use all 50,000 phone numbers in random order, repeated and averaged, so they are not served from a single cached key.

Build and run from the project root:

```bash
g++ -std=c++17 -O2 -Iinclude bench.cpp src/PhoneBook.cpp src/HashTable.cpp src/Trie.cpp src/BST.cpp -o bench
./bench
```

Results from one machine (g++ `-O2`, 50,000 contacts). Your numbers will differ, but the relative picture should hold.

| Operation | Structure | Result |
|---|---|---|
| Look up by phone number (random order) | Hash table | ~135 ns average (median over repeated runs) |
| Prefix search, e.g. "Aisha" (954 matches) | Trie | ~22 µs per query |
| Name range "Dan" to "Priya" (27,986 matches) | BST | ~0.8 to 1.1 ms per query |
| Load 50,000 contacts from a **sorted** CSV | all three | ~5.0 s |
| Load 50,000 contacts in **shuffled** order | all three | ~0.22 s |

Repeating a lookup of the *same* key in a tight loop reads much faster (roughly 40 to 60 ns here), but that only measures cache hits and is not representative, so it is not the figure reported above.

### Finding: reloading a sorted file is slow

The benchmark showed that loading the sorted CSV is about 20 times slower than loading the same contacts in shuffled order. `saveToFile` writes contacts in alphabetical order, and `loadFromFile` inserts them back in that same order into a BST that does not rebalance. Sorted insertion turns the tree into a linked list, so loading becomes O(n^2) instead of O(n log n). See the limitations below.

## Design notes and limitations

- **The BST is not self-balancing.** Inserting names in already-sorted order degrades it toward a linked list and lookups toward O(n). Random or mixed input stays close to O(log n).
- **Reloading a saved file hits that worst case.** Because `saveToFile` writes names in sorted order, loading the CSV back inserts them in sorted order, which took ~5 s for 50,000 contacts versus ~0.22 s when shuffled. Possible fixes: shuffle the rows before inserting, build a balanced tree directly from the sorted list, or replace the BST with a self-balancing tree (AVL or red-black). Not implemented yet.
- **The trie does not prune empty branches** after a delete. It is correct, but memory is not reclaimed until exit.
- **Names are case-insensitive** in the trie and BST; phone numbers are matched exactly as typed.
- **No input validation** on phone number format.
- **Persistence is whole-file:** the CSV is rewritten in full on save.